#!/usr/bin/env python3
"""Derive a Minecraft glTF export for Quantiloom Studio, optionally with per-texel PBR maps.

    python3 -I scripts/gen_minecraft_scene.py assets/models/网安大楼.gltf
    python3 -I scripts/gen_minecraft_scene.py assets/models/网安大楼.gltf \\
        --pbr-pack "ModernArch v4.11.1 [26.3 - 26.4] [512x].zip" \\
        --vanilla <baker data>/minecraft/26.4-pre-1/assets/minecraft/textures/block \\
        --pbr-maps mr

The Blender export gives every one of its ~190 materials metallic 0 / roughness 1 and
textures them all from one atlas of 16 px tiles. With no --pbr-* option the derived
glTF is a byte copy of the input. With --pbr-pack the script works out which block
texture each atlas tile is (exact pixel match against the vanilla 16x textures, then
a tolerant match against the texture of the material's own name, for the glass the
exporter blends), reads that block's LabPBR _s / _n maps from the pack, resamples
them onto the same tile grid and writes UV-aligned images next to the glTF. --pbr-maps
chooses which: mr (the default), normal or both. A normal map costs a second map's GPU
memory plus a tangent upload for every vertex of the materials that use it, for 16 texels
per tile, so it is opt-in.


  <atlas>_mr.png   G = 1 - smoothness (perceptual roughness), B = 1 for LabPBR metal
                   ids 230-237 and 255, else 0 (a fraction where a tile mixes them),
                   R unused. glTF reads G as roughness and B as metallic.
  <atlas>_n.png    OpenGL (+Y) tangent-space normal: the pack's DirectX Y flipped,
                   Z rebuilt from X and Y. The pack's AO and height are ignored.

Only materials with pack data reference a map (factors 1.0); the others keep what the
export gave them. The pack is third-party data: it is read in place, nothing is
extracted, and the generated images are not for a repository (assets/models is
git-ignored). Run with `python3 -I`; the zip is untrusted input.

Physics is not decided here: roughness and a metal flag only. What a metal is made of
(n,k) still comes from the spectral library bound to the material by name.
"""

import argparse
import collections
import io
import json
import shutil
import sys
import time
import urllib.parse
import zipfile
from pathlib import Path

import numpy as np
from PIL import Image

Image.MAX_IMAGE_PIXELS = None

TILE = 16  # the atlas is a grid of 16 px tiles, one per vanilla block texture
FUZZY_TOLERANCE = 12.0  # mean abs 8-bit difference accepted for a name-guided match
METAL_IDS = np.array([*range(230, 238), 255])
COMPONENT = {5121: "u1", 5123: "<u2", 5125: "<u4", 5126: "<f4"}
WIDTH = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}


def log(msg):
    print(msg, flush=True)


class Gltf:
    """Read-only access to a glTF's accessors without loading its (hundreds of MB) buffer."""

    def __init__(self, path: Path):
        self.path = path
        self.doc = json.loads(path.read_text(encoding="utf-8"))
        buf = self.doc["buffers"][0]
        self.bin = np.memmap(path.parent / urllib.parse.unquote(buf["uri"]), dtype=np.uint8, mode="r")

    def accessor(self, index):
        a = self.doc["accessors"][index]
        view = self.doc["bufferViews"][a["bufferView"]]
        dt = np.dtype(COMPONENT[a["componentType"]])
        n = WIDTH[a["type"]]
        start = view.get("byteOffset", 0) + a.get("byteOffset", 0)
        stride = view.get("byteStride", 0)
        if stride and stride != dt.itemsize * n:
            raise SystemExit(f"{self.path.name}: interleaved accessor {index} is not supported")
        raw = self.bin[start : start + a["count"] * n * dt.itemsize]
        return np.frombuffer(raw, dtype=dt).reshape(a["count"], n)


def material_tiles(gltf, atlas_tiles):
    """material index -> {(tx, ty)} atlas tiles its triangles sit in, plus its vertex count.

    A triangle belongs to the tile its UV centroid falls in: a vertex exactly on a tile
    border would otherwise count for the neighbour.
    """
    tiles = collections.defaultdict(set)
    vertices = collections.Counter()
    for mesh in gltf.doc["meshes"]:
        for prim in mesh["primitives"]:
            m = prim["material"]
            uv = gltf.accessor(prim["attributes"]["TEXCOORD_0"]).astype(np.float64)
            idx = gltf.accessor(prim["indices"])[:, 0].astype(np.int64)
            centroid = uv[idx].reshape(-1, 3, 2).mean(axis=1) * atlas_tiles
            tiles[m] |= set(map(tuple, np.unique(np.floor(centroid).astype(np.int64), axis=0)))
            vertices[m] += len(uv)
    return tiles, vertices


def normalized(tile):
    """A tile's bytes with the colour of fully transparent texels zeroed (it is not data)."""
    tile = tile.copy()
    tile[tile[..., 3] == 0] = 0
    return tile.tobytes()


def load_vanilla(directory: Path):
    """frame bytes -> [(texture name, frame)], and name -> frame-0 array, for every 16 px-wide texture."""
    exact, by_name = collections.defaultdict(list), {}
    for path in sorted(directory.rglob("*.png")):
        im = np.array(Image.open(path).convert("RGBA"))
        h, w = im.shape[:2]
        if w != TILE or h % TILE:
            continue  # not a block tile; animated strips are stacked 16 px frames
        name = path.relative_to(directory).with_suffix("").as_posix()
        for frame in range(h // TILE):
            exact[normalized(im[frame * TILE : (frame + 1) * TILE])].append((name, frame))
        by_name[name] = im[:TILE].astype(np.float32)
    return exact, by_name


def match_tile(tile, material_name, exact, by_name):
    """-> ((name, frame), how) or (None, why)."""
    hit = exact.get(normalized(tile))
    if hit:
        return hit[0], "exact"
    stem = material_name.split(":", 1)[-1]
    best = None
    for cand in {stem, stem.removesuffix("_pane")}:
        ref = by_name.get(cand)
        if ref is not None:
            err = float(np.abs(ref - tile.astype(np.float32)).mean())
            if best is None or err < best[0]:
                best = (err, cand)
    if best and best[0] <= FUZZY_TOLERANCE:
        return (best[1], 0), "fuzzy"
    return None, "no vanilla texture"


class Pack:
    """LabPBR _s / _n maps of a resource pack, read straight out of the zip."""

    ROOT = "assets/minecraft/textures/block/"

    def __init__(self, path: Path):
        self.zip = zipfile.ZipFile(path)
        self.names = set(self.zip.namelist())

    def find(self, name, suffix):
        """The member for `<name><suffix>.png`, else a numbered variant (`<name>/<name>1`)."""
        for member in (f"{self.ROOT}{name}{suffix}.png", f"{self.ROOT}{name}/{name.rsplit('/', 1)[-1]}1{suffix}.png"):
            if member in self.names:
                return member
        return None

    def frame(self, member, frame):
        im = Image.open(io.BytesIO(self.zip.read(member))).convert("RGBA")
        w, h = im.size
        if h % w:
            raise ValueError(f"{member}: {w}x{h} is not a stack of square frames")
        return im.crop((0, frame * w, w, (frame + 1) * w)) if h > w else im


def resample(channel, size):
    """Box-filter a float32 square image down (or nearest-up) to size x size."""
    im = Image.fromarray(channel.astype(np.float32), mode="F")
    if im.size[0] >= size:
        return np.asarray(im.resize((size, size), Image.BOX), dtype=np.float32)
    return np.asarray(im.resize((size, size), Image.NEAREST), dtype=np.float32)


def mr_tile(im, size):
    s = np.asarray(im, dtype=np.float32)
    smooth = s[..., 0] / 255.0
    metal = np.isin(s[..., 1].astype(np.int64), METAL_IDS).astype(np.float32)
    out = np.zeros((size, size, 4), np.uint8)
    out[..., 1] = np.rint(np.clip(1.0 - resample(smooth, size), 0, 1) * 255)
    out[..., 2] = np.rint(np.clip(resample(metal, size), 0, 1) * 255)
    out[..., 3] = 255
    return out


def normal_tile(im, size):
    n = np.asarray(im, dtype=np.float32)
    x = resample(n[..., 0] / 255.0 * 2 - 1, size)
    y = resample(-(n[..., 1] / 255.0 * 2 - 1), size)  # DirectX (Y-) -> OpenGL (Y+)
    length = np.maximum(np.sqrt(x * x + y * y), 1.0)  # averaging can leave the unit disc
    x, y = x / length, y / length
    z = np.sqrt(np.clip(1.0 - x * x - y * y, 0, 1))
    out = np.empty((size, size, 4), np.uint8)
    for c, v in enumerate((x, y, z)):
        out[..., c] = np.rint((v * 0.5 + 0.5) * 255)
    out[..., 3] = 255
    return out


def mib(n):
    return n / (1 << 20)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("gltf", type=Path, help="the exporter's glTF (its .bin and atlas stay beside it)")
    ap.add_argument("--out", type=Path, help="derived glTF (default: <name>.derived.gltf beside the input)")
    ap.add_argument("--pbr-pack", type=Path, metavar="PACK.zip", help="a LabPBR resource pack to take _s / _n maps from")
    ap.add_argument("--pbr-maps", choices=("mr", "normal", "both"), help="which maps to write (default: mr; normal adds a second map and a tangent upload per vertex)")
    ap.add_argument("--vanilla", type=Path, help="the vanilla textures/block directory the atlas was exported from (required with --pbr-pack)")
    ap.add_argument("--pbr-size", type=int, help="map edge in texels (default: the atlas edge, texel for texel)")
    args = ap.parse_args()

    out = args.out or args.gltf.with_suffix(".derived.gltf")
    if not args.pbr_pack:
        if args.pbr_maps or args.vanilla or args.pbr_size:
            ap.error("--pbr-maps, --vanilla and --pbr-size need --pbr-pack")
        shutil.copyfile(args.gltf, out)
        log(f"no maps requested: {out} is a byte copy of {args.gltf}")
        return 0
    if not args.vanilla:
        ap.error("--pbr-pack needs --vanilla")
    maps = args.pbr_maps or "mr"
    t0 = time.time()

    gltf = Gltf(args.gltf)
    doc = gltf.doc
    if len(doc["images"]) != 1:
        raise SystemExit("expected one shared atlas image")
    atlas_path = args.gltf.parent / urllib.parse.unquote(doc["images"][0]["uri"])
    atlas_im = Image.open(atlas_path)
    atlas = atlas_im.size[0]
    if atlas_im.size[0] != atlas_im.size[1] or atlas % 512:
        raise SystemExit(f"atlas {atlas_im.size} is not a square multiple of 512")
    grid = atlas // TILE
    size = args.pbr_size or atlas
    if size % grid:
        raise SystemExit(f"--pbr-size must be a multiple of {grid} (one tile per {TILE} atlas texels)")
    tile_px = size // grid

    tiles, vertices = material_tiles(gltf, grid)
    top = max(ty for ts in tiles.values() for _, ty in ts) + 1
    atlas_rgba = np.array(atlas_im.convert("RGBA").crop((0, 0, atlas, top * TILE)))
    log(f"WORKING: atlas {atlas}x{atlas}, tile {TILE} px, geometry touches {sum(map(len, tiles.values()))} "
        f"material-tiles in the top {top * TILE} rows ({time.time() - t0:.1f} s)")

    exact, by_name = load_vanilla(args.vanilla)
    pack = Pack(args.pbr_pack)

    # tile -> (texture, frame), per distinct tile of the atlas
    resolved, how = {}, collections.Counter()
    name_of = {i: m["name"] for i, m in enumerate(doc["materials"])}
    for m, ts in sorted(tiles.items()):
        for tx, ty in sorted(ts):
            if (tx, ty) in resolved:
                continue
            tile = atlas_rgba[ty * TILE : (ty + 1) * TILE, tx * TILE : (tx + 1) * TILE]
            resolved[(tx, ty)], why = match_tile(tile, name_of[m], exact, by_name)
            how[why] += 1
    log(f"WORKING: tiles matched to a vanilla texture: {dict(how)} ({time.time() - t0:.1f} s)")

    mr = np.zeros((size, size, 4), np.uint8) if maps != "normal" else None
    nm = np.zeros((size, size, 4), np.uint8) if maps != "mr" else None
    if mr is not None:
        mr[...] = (0, 255, 0, 255)  # roughness 1, not metal: what the export had
    if nm is not None:
        nm[...] = (128, 128, 255, 255)
    has = {"mr": set(), "n": set()}
    missing = collections.Counter()
    cache = {}
    for (tx, ty), key in resolved.items():
        if key is None:
            continue
        name, frame = key
        for kind, suffix, enabled, canvas, make in (
            ("mr", "_s", mr is not None, mr, mr_tile),
            ("n", "_n", nm is not None, nm, normal_tile),
        ):
            if not enabled:
                continue
            if (kind, name, frame) not in cache:
                member = pack.find(name, suffix)
                cache[(kind, name, frame)] = make(pack.frame(member, frame), tile_px) if member else None
                if member is None:
                    missing[f"{kind}:{name}"] += 1
            px = cache[(kind, name, frame)]
            if px is not None:
                canvas[ty * tile_px : (ty + 1) * tile_px, tx * tile_px : (tx + 1) * tile_px] = px
                has[kind].add((tx, ty))
    log(f"WORKING: maps painted ({time.time() - t0:.1f} s); textures without a pack file: "
        f"{len({k for k in missing})} ({sorted(missing)[:6]}...)")

    stem = atlas_path.stem
    written = {}
    for kind, canvas in (("mr", mr), ("n", nm)):
        if canvas is None:
            continue
        path = atlas_path.with_name(f"{stem}_{kind}.png")
        Image.fromarray(canvas, "RGBA").save(path, compress_level=6)
        written[kind] = path
        log(f"wrote {path} ({size}x{size}, {path.stat().st_size / 1e6:.1f} MB on disk)")

    # glTF: one image + one texture per map, shared by every material that has pack data
    sampler = doc["textures"][0].get("sampler")
    refs = {}
    for kind, path in written.items():
        doc["images"].append({"mimeType": "image/png", "name": path.stem, "uri": urllib.parse.quote(path.name)})
        tex = {"source": len(doc["images"]) - 1}
        if sampler is not None:
            tex["sampler"] = sampler
        doc["textures"].append(tex)
        refs[kind] = len(doc["textures"]) - 1
    counts = collections.Counter()
    normal_vertices = 0
    for i, mat in enumerate(doc["materials"]):
        ts = tiles.get(i, set())
        pbr = mat.setdefault("pbrMetallicRoughness", {})
        if "mr" in refs and ts & has["mr"]:
            pbr["metallicRoughnessTexture"] = {"index": refs["mr"]}
            pbr["metallicFactor"] = 1.0
            pbr["roughnessFactor"] = 1.0
            counts["mr"] += 1
        if "n" in refs and ts & has["n"]:
            mat["normalTexture"] = {"index": refs["n"]}
            counts["n"] += 1
            normal_vertices += vertices[i]
    out.write_text(json.dumps(doc, ensure_ascii=False, indent=2), encoding="utf-8")

    n_mat = len(doc["materials"])
    log(f"wrote {out}")
    log(f"materials with a metallicRoughnessTexture: {counts['mr']} / {n_mat}; with a normalTexture: {counts['n']} / {n_mat}")
    log(f"distinct atlas tiles with _s data: {len(has['mr'])}, with _n data: {len(has['n'])}, resolved: "
        f"{sum(v is not None for v in resolved.values())} / {len(resolved)}")
    unmatched = sorted(name_of[m] for m, ts in tiles.items() if all(resolved[t] is None for t in ts))
    log(f"materials with no resolved tile at all ({len(unmatched)}): {unmatched}")

    log("GPU cost per map (RGBA8 + full mip chain, 4/3 of the base level):")
    for edge in sorted({size, atlas, atlas // 2, atlas // 4}, reverse=True):
        log(f"  {edge:>5}x{edge:<5} {mib(edge * edge * 4 * 4 / 3):8.1f} MiB   (tile {edge // grid} px)")
    log(f"normal-map tangents: {normal_vertices} vertices x 16 B = {mib(normal_vertices * 16):.1f} MiB "
        f"(float4 per vertex of every normal-mapped primitive)")
    log(f"done in {time.time() - t0:.1f} s")
    return 0


if __name__ == "__main__":
    sys.exit(main())
