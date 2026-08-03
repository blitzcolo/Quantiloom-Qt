#!/usr/bin/env python3
"""Generate the four environment look-dev scenes as self-contained GLBs.

Builds grassland / desert / seaside / concrete scenes for the renderer:
procedural terrain, water, rocks and buildings with procedurally generated
PBR textures (embedded PNGs), merged with the real models under
assets/models/ (ford.glb, su7/source/SU7.glb, CesiumMan.glb), each
auto-scaled to real-world size and seated on the terrain.

Only numpy and Pillow are required.  Output: assets/models/env/*.glb.
"""

import io
import json
import math
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image

REPO = Path(__file__).resolve().parents[1]
MODELS = REPO / "assets" / "models"
OUT_DIR = MODELS / "env"

GLB_MAGIC = 0x46546C67
CHUNK_JSON = 0x4E4F534A
CHUNK_BIN = 0x004E4942

FLOAT = 5126
UINT32 = 5125
ARRAY_BUFFER = 34962
ELEMENT_ARRAY_BUFFER = 34963
REPEAT = 10497
LINEAR_MIPMAP_LINEAR = 9987
LINEAR = 9729


# ---------------------------------------------------------------------------
# GLB read / write
# ---------------------------------------------------------------------------

def load_glb(path):
    data = Path(path).read_bytes()
    magic, _version, _length = struct.unpack_from("<III", data, 0)
    if magic != GLB_MAGIC:
        raise ValueError(f"{path} is not a GLB")
    offset = 12
    gltf, blob = None, b""
    while offset < len(data):
        clen, ctype = struct.unpack_from("<II", data, offset)
        offset += 8
        chunk = data[offset:offset + clen]
        offset += clen
        if ctype == CHUNK_JSON:
            gltf = json.loads(chunk.decode("utf-8"))
        elif ctype == CHUNK_BIN:
            blob = chunk
    return gltf, blob


def save_glb(path, gltf, blob):
    js = json.dumps(gltf, separators=(",", ":")).encode("utf-8")
    js += b" " * (-len(js) % 4)
    blob = bytes(blob) + b"\x00" * (-len(blob) % 4)
    total = 12 + 8 + len(js) + 8 + len(blob)
    with open(path, "wb") as f:
        f.write(struct.pack("<III", GLB_MAGIC, 2, total))
        f.write(struct.pack("<II", len(js), CHUNK_JSON))
        f.write(js)
        f.write(struct.pack("<II", len(blob), CHUNK_BIN))
        f.write(blob)


# ---------------------------------------------------------------------------
# Procedural textures
# ---------------------------------------------------------------------------

def _lattice(res, freq, seed):
    """Tileable value noise in [0,1], bicubic-ish smoothstep interpolation."""
    rng = np.random.default_rng(seed)
    grid = rng.random((freq, freq)).astype(np.float32)
    ys = np.linspace(0, freq, res, endpoint=False)
    x0 = np.floor(ys).astype(int)
    t = (ys - x0).astype(np.float32)
    t = t * t * (3 - 2 * t)
    x1 = (x0 + 1) % freq
    g00 = grid[np.ix_(x0 % freq, x0 % freq)]
    g10 = grid[np.ix_(x1, x0 % freq)]
    g01 = grid[np.ix_(x0 % freq, x1)]
    g11 = grid[np.ix_(x1, x1)]
    a = g00 * (1 - t[:, None]) + g10 * t[:, None]
    b = g01 * (1 - t[:, None]) + g11 * t[:, None]
    return a * (1 - t[None, :]) + b * t[None, :]


def fbm(res, base_freq, octaves, seed, gain=0.5):
    out = np.zeros((res, res), np.float32)
    amp, freq, norm = 1.0, base_freq, 0.0
    for i in range(octaves):
        out += amp * _lattice(res, freq, seed + i * 131)
        norm += amp
        amp *= gain
        freq *= 2
    return out / norm


def height_to_normal(height, strength):
    """Tangent-space normal map (OpenGL +Y) from a tileable height field."""
    dx = (np.roll(height, -1, axis=1) - np.roll(height, 1, axis=1)) * 0.5
    dy = (np.roll(height, -1, axis=0) - np.roll(height, 1, axis=0)) * 0.5
    n = np.stack([-dx * strength, dy * strength, np.ones_like(height)], axis=-1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return ((n * 0.5 + 0.5) * 255).astype(np.uint8)


def to_png(arr):
    img = Image.fromarray(arr)
    buf = io.BytesIO()
    img.save(buf, format="PNG", optimize=True)
    return buf.getvalue()


def albedo_png(base_rgb, variation, tint_rgb=None, tint_mask=None, speckle=None):
    """base colour modulated by a variation field, optionally tinted by a mask."""
    res = variation.shape[0]
    col = np.zeros((res, res, 3), np.float32)
    base = np.asarray(base_rgb, np.float32)
    v = (variation - variation.mean()) * 2.0
    for c in range(3):
        col[..., c] = base[c] * (1.0 + v)
    if tint_rgb is not None and tint_mask is not None:
        tint = np.asarray(tint_rgb, np.float32)
        for c in range(3):
            col[..., c] = col[..., c] * (1 - tint_mask) + tint[c] * tint_mask
    if speckle is not None:
        col *= (1.0 + speckle)[..., None]
    return (np.clip(col, 0, 1) ** (1 / 2.2) * 255).astype(np.uint8)


def mr_png(roughness_field, metallic=0.0):
    res = roughness_field.shape[0]
    arr = np.zeros((res, res, 3), np.uint8)
    arr[..., 1] = (np.clip(roughness_field, 0, 1) * 255).astype(np.uint8)
    arr[..., 2] = int(np.clip(metallic, 0, 1) * 255)
    return arr


def make_texture_set(kind, res=512):
    """Returns dict: albedo PNG bytes, normal PNG bytes, mr PNG bytes."""
    if kind == "grass":
        patches = fbm(res, 3, 5, seed=11, gain=0.6)
        blades = fbm(res, 64, 3, seed=12)
        h = patches * 0.6 + blades * 0.4
        dry = np.clip((patches - 0.58) * 3, 0, 1)
        alb = albedo_png((0.075, 0.16, 0.04), blades * 1.1 - 0.55 + patches * 0.5 - 0.25,
                         tint_rgb=(0.26, 0.22, 0.07), tint_mask=dry * 0.5)
        nrm = height_to_normal(h, 2.2)
        mr = mr_png(0.85 + (blades - 0.5) * 0.2)
    elif kind == "sand":
        u = np.linspace(0, 2 * np.pi * 14, res, dtype=np.float32)
        warp = fbm(res, 6, 3, seed=21)
        ripple = 0.5 + 0.5 * np.sin(u[None, :] + warp * 9.0)
        grains = fbm(res, 128, 2, seed=22)
        h = ripple * 0.75 + grains * 0.25
        alb = albedo_png((0.74, 0.60, 0.42), ripple * 0.16 - 0.08 + grains * 0.12 - 0.06)
        nrm = height_to_normal(h, 1.6)
        mr = mr_png(0.9 + (grains - 0.5) * 0.1)
    elif kind == "concrete":
        stains = fbm(res, 3, 4, seed=31)
        speck = fbm(res, 96, 2, seed=32)
        h = speck * 0.5 + stains * 0.5
        # expansion joints: texture tile covers 4 m, joints on a 2 m grid
        px = np.arange(res)
        joint = np.zeros((res, res), np.float32)
        for line in (0, res // 2):
            band = np.exp(-((px - line + res // 2) % (res // 2) - res // 2) ** 2 / 6.0)
            joint = np.maximum(joint, band[None, :])
            joint = np.maximum(joint, band[:, None])
        h = h * (1 - joint * 0.8) - joint * 0.5
        alb = albedo_png((0.42, 0.42, 0.40), speck * 0.10 - 0.05 + stains * 0.16 - 0.08,
                         tint_rgb=(0.20, 0.20, 0.19), tint_mask=joint * 0.8)
        nrm = height_to_normal(h, 2.5)
        mr = mr_png(0.75 + stains * 0.15)
    elif kind == "rock":
        big = fbm(res, 5, 5, seed=41)
        alb = albedo_png((0.30, 0.28, 0.24), big * 0.5 - 0.25)
        nrm = height_to_normal(big, 3.0)
        mr = mr_png(0.9 - big * 0.1)
    elif kind == "plaster":
        n = fbm(res, 12, 3, seed=51)
        alb = albedo_png((0.78, 0.74, 0.66), n * 0.10 - 0.05)
        nrm = height_to_normal(n, 0.8)
        mr = mr_png(0.8 * np.ones((res, res), np.float32))
    elif kind == "roof":
        rows = np.linspace(0, 2 * np.pi * 8, res, dtype=np.float32)
        tile = 0.5 + 0.5 * np.sin(rows)[:, None] * np.ones((1, res), np.float32)
        n = fbm(res, 16, 3, seed=61)
        alb = albedo_png((0.26, 0.13, 0.10), tile * 0.2 - 0.1 + n * 0.12 - 0.06)
        nrm = height_to_normal(tile * 0.7 + n * 0.3, 2.0)
        mr = mr_png(0.75 * np.ones((res, res), np.float32))
    elif kind == "wood":
        streaks = fbm(res, 3, 2, seed=71)
        grain = _lattice(res, 96, 72) * 0.5 + streaks * 0.5
        alb = albedo_png((0.32, 0.19, 0.10), grain * 0.35 - 0.18)
        nrm = height_to_normal(grain, 1.0)
        mr = mr_png(0.6 * np.ones((res, res), np.float32))
    else:
        raise ValueError(kind)
    return {"albedo": to_png(alb), "normal": to_png(nrm), "mr": to_png(mr)}


# ---------------------------------------------------------------------------
# Mesh helpers
# ---------------------------------------------------------------------------

def compute_normals(pos, idx):
    nrm = np.zeros_like(pos)
    a, b, c = pos[idx[0::3]], pos[idx[1::3]], pos[idx[2::3]]
    fn = np.cross(b - a, c - a)
    for i in range(3):
        np.add.at(nrm, idx[i::3], fn)
    ln = np.linalg.norm(nrm, axis=1, keepdims=True)
    ln[ln == 0] = 1
    return nrm / ln


def grid_mesh(size, divisions, height_fn, uv_repeat):
    n = divisions + 1
    xs = np.linspace(-size / 2, size / 2, n, dtype=np.float32)
    zs = np.linspace(-size / 2, size / 2, n, dtype=np.float32)
    X, Z = np.meshgrid(xs, zs, indexing="xy")
    Y = height_fn(X, Z).astype(np.float32)
    pos = np.stack([X, Y, Z], axis=-1).reshape(-1, 3)
    u = (X / size + 0.5) * uv_repeat
    v = (Z / size + 0.5) * uv_repeat
    uv = np.stack([u, v], axis=-1).reshape(-1, 2).astype(np.float32)
    ii, jj = np.meshgrid(np.arange(divisions), np.arange(divisions), indexing="xy")
    v0 = (jj * n + ii).ravel()
    idx = np.stack([v0, v0 + n, v0 + 1, v0 + 1, v0 + n, v0 + n + 1], axis=-1)
    idx = idx.ravel().astype(np.uint32)
    nrm = compute_normals(pos, idx).astype(np.float32)
    tan = np.tile(np.array([1, 0, 0, 1], np.float32), (len(pos), 1))
    return pos.astype(np.float32), nrm, uv, tan, idx


def uv_sphere(radius, seg_u=48, seg_v=32, displace=None):
    us = np.linspace(0, 2 * np.pi, seg_u + 1, dtype=np.float32)
    vs = np.linspace(0, np.pi, seg_v + 1, dtype=np.float32)
    U, V = np.meshgrid(us, vs, indexing="xy")
    x = np.sin(V) * np.cos(U)
    y = np.cos(V)
    z = np.sin(V) * np.sin(U)
    dirs = np.stack([x, y, z], axis=-1).reshape(-1, 3)
    r = radius
    if displace is not None:
        r = radius * (1.0 + displace(dirs))[:, None]
    pos = (dirs * r).astype(np.float32)
    uv = np.stack([U / (2 * np.pi), V / np.pi], axis=-1).reshape(-1, 2).astype(np.float32)
    n = seg_u + 1
    ii, jj = np.meshgrid(np.arange(seg_u), np.arange(seg_v), indexing="xy")
    v0 = (jj * n + ii).ravel()
    idx = np.stack([v0, v0 + 1, v0 + n, v0 + 1, v0 + n + 1, v0 + n], axis=-1)
    idx = idx.ravel().astype(np.uint32)
    nrm = compute_normals(pos, idx).astype(np.float32) if displace is not None \
        else dirs.astype(np.float32)
    tan = np.stack([-np.sin(U), np.zeros_like(U), np.cos(U)], axis=-1).reshape(-1, 3)
    tan = np.concatenate([tan, np.ones((len(pos), 1), np.float32)], axis=1).astype(np.float32)
    return pos, nrm, uv, tan, idx


def box_mesh(w, h, d, uv_scale=1.0):
    hw, hh, hd = w / 2, h / 2, d / 2
    faces = [  # normal, tangent, corner order (CCW from outside)
        ((0, 0, 1), (1, 0, 0), [(-hw, -hh, hd), (hw, -hh, hd), (hw, hh, hd), (-hw, hh, hd)], (w, h)),
        ((0, 0, -1), (-1, 0, 0), [(hw, -hh, -hd), (-hw, -hh, -hd), (-hw, hh, -hd), (hw, hh, -hd)], (w, h)),
        ((1, 0, 0), (0, 0, -1), [(hw, -hh, hd), (hw, -hh, -hd), (hw, hh, -hd), (hw, hh, hd)], (d, h)),
        ((-1, 0, 0), (0, 0, 1), [(-hw, -hh, -hd), (-hw, -hh, hd), (-hw, hh, hd), (-hw, hh, -hd)], (d, h)),
        ((0, 1, 0), (1, 0, 0), [(-hw, hh, hd), (hw, hh, hd), (hw, hh, -hd), (-hw, hh, -hd)], (w, d)),
        ((0, -1, 0), (1, 0, 0), [(-hw, -hh, -hd), (hw, -hh, -hd), (hw, -hh, hd), (-hw, -hh, hd)], (w, d)),
    ]
    pos, nrm, uv, tan, idx = [], [], [], [], []
    for fi, (n, t, corners, (fw, fh)) in enumerate(faces):
        base = fi * 4
        pos += corners
        nrm += [n] * 4
        tan += [tuple(t) + (1.0,)] * 4
        uv += [(0, 0), (fw * uv_scale, 0), (fw * uv_scale, fh * uv_scale), (0, fh * uv_scale)]
        idx += [base, base + 1, base + 2, base, base + 2, base + 3]
    return (np.array(pos, np.float32), np.array(nrm, np.float32),
            np.array(uv, np.float32), np.array(tan, np.float32),
            np.array(idx, np.uint32))


def gable_roof(w, d, rise, overhang):
    """Gable roof: ridge along X. Origin at wall-top plane."""
    hw, hd = w / 2 + overhang, d / 2 + overhang
    ridge = rise
    pos, nrm, uv, tan, idx = [], [], [], [], []

    def quad(a, b, c, dd):
        base = len(pos)
        pts = [a, b, c, dd]
        v1 = np.subtract(b, a)
        v2 = np.subtract(dd, a)
        n = np.cross(v1, v2)
        n = n / np.linalg.norm(n)
        pos.extend(pts)
        nrm.extend([tuple(n)] * 4)
        t = v1 / np.linalg.norm(v1)
        tan.extend([tuple(t) + (1.0,)] * 4)
        uv.extend([(0, 0), (2, 0), (2, 2), (0, 2)])
        idx.extend([base, base + 1, base + 2, base, base + 2, base + 3])

    quad((-hw, 0, hd), (hw, 0, hd), (hw, ridge, 0), (-hw, ridge, 0))
    quad((hw, 0, -hd), (-hw, 0, -hd), (-hw, ridge, 0), (hw, ridge, 0))

    def tri(a, bb, c, n):
        base = len(pos)
        pos.extend([a, bb, c])
        nrm.extend([n] * 3)
        tan.extend([(0, 0, n[0] and -n[0] or 1, 1.0)] * 3)
        uv.extend([(0, 0), (1, 0), (0.5, 1)])
        idx.extend([base, base + 1, base + 2])

    # gable end caps so the roof reads as a solid from the side
    tri((hw, 0, hd), (hw, 0, -hd), (hw, ridge, 0), (1, 0, 0))
    tri((-hw, 0, -hd), (-hw, 0, hd), (-hw, ridge, 0), (-1, 0, 0))
    return (np.array(pos, np.float32), np.array(nrm, np.float32),
            np.array(uv, np.float32), np.array(tan, np.float32),
            np.array(idx, np.uint32))


# ---------------------------------------------------------------------------
# Scene builder
# ---------------------------------------------------------------------------

class Builder:
    def __init__(self):
        self.gltf = {
            "asset": {"version": "2.0", "generator": "gen_env_scenes.py"},
            "scene": 0,
            "scenes": [{"name": "Scene", "nodes": []}],
            "nodes": [], "meshes": [], "materials": [], "accessors": [],
            "bufferViews": [], "buffers": [{"byteLength": 0}],
            "images": [], "textures": [],
            "samplers": [{"magFilter": LINEAR, "minFilter": LINEAR_MIPMAP_LINEAR,
                          "wrapS": REPEAT, "wrapT": REPEAT}],
            "extensionsUsed": [],
        }
        self.bin = bytearray()
        self._tex_cache = {}

    def _pad(self):
        self.bin.extend(b"\x00" * (-len(self.bin) % 4))

    def add_view(self, data, target=None):
        self._pad()
        view = {"buffer": 0, "byteOffset": len(self.bin), "byteLength": len(data)}
        if target:
            view["target"] = target
        self.bin.extend(data)
        self.gltf["bufferViews"].append(view)
        return len(self.gltf["bufferViews"]) - 1

    def add_accessor(self, arr, acc_type, component, target, with_minmax=False):
        view = self.add_view(arr.tobytes(), target)
        acc = {"bufferView": view, "componentType": component,
               "count": len(arr), "type": acc_type}
        if with_minmax:
            acc["min"] = [float(v) for v in arr.min(axis=0)]
            acc["max"] = [float(v) for v in arr.max(axis=0)]
        self.gltf["accessors"].append(acc)
        return len(self.gltf["accessors"]) - 1

    def add_image_texture(self, png_bytes):
        view = self.add_view(png_bytes)
        self.gltf["images"].append({"bufferView": view, "mimeType": "image/png"})
        self.gltf["textures"].append({"sampler": 0, "source": len(self.gltf["images"]) - 1})
        return len(self.gltf["textures"]) - 1

    def use_ext(self, name):
        if name not in self.gltf["extensionsUsed"]:
            self.gltf["extensionsUsed"].append(name)

    def add_pbr_material(self, name, tex_kind=None, base_color=None, metallic=0.0,
                         roughness=0.9, extensions=None, double_sided=False,
                         emissive=None):
        pbr = {"metallicFactor": metallic, "roughnessFactor": roughness}
        mat = {"name": name, "pbrMetallicRoughness": pbr}
        if tex_kind:
            if tex_kind not in self._tex_cache:
                texset = make_texture_set(tex_kind)
                self._tex_cache[tex_kind] = {
                    k: self.add_image_texture(v) for k, v in texset.items()}
            t = self._tex_cache[tex_kind]
            pbr["baseColorTexture"] = {"index": t["albedo"]}
            pbr["metallicRoughnessTexture"] = {"index": t["mr"]}
            mat["normalTexture"] = {"index": t["normal"], "scale": 1.0}
        if base_color is not None:
            pbr["baseColorFactor"] = list(base_color)
        if emissive is not None:
            mat["emissiveFactor"] = list(emissive)
        if double_sided:
            mat["doubleSided"] = True
        if extensions:
            mat["extensions"] = extensions
            for e in extensions:
                self.use_ext(e)
        self.gltf["materials"].append(mat)
        return len(self.gltf["materials"]) - 1

    def add_mesh_node(self, name, mesh_data, material, translation=None,
                      rotation_y=0.0, parent=None):
        pos, nrm, uv, tan, idx = mesh_data
        prim = {
            "attributes": {
                "POSITION": self.add_accessor(pos, "VEC3", FLOAT, ARRAY_BUFFER, True),
                "NORMAL": self.add_accessor(nrm, "VEC3", FLOAT, ARRAY_BUFFER),
                "TEXCOORD_0": self.add_accessor(uv, "VEC2", FLOAT, ARRAY_BUFFER),
                "TANGENT": self.add_accessor(tan, "VEC4", FLOAT, ARRAY_BUFFER),
            },
            "indices": self.add_accessor(idx, "SCALAR", UINT32, ELEMENT_ARRAY_BUFFER),
            "material": material, "mode": 4,
        }
        self.gltf["meshes"].append({"name": name, "primitives": [prim]})
        node = {"name": name, "mesh": len(self.gltf["meshes"]) - 1}
        if translation:
            node["translation"] = [float(x) for x in translation]
        if rotation_y:
            h = math.radians(rotation_y) / 2
            node["rotation"] = [0.0, math.sin(h), 0.0, math.cos(h)]
        self.gltf["nodes"].append(node)
        ni = len(self.gltf["nodes"]) - 1
        if parent is None:
            self.gltf["scenes"][0]["nodes"].append(ni)
        else:
            self.gltf["nodes"][parent].setdefault("children", []).append(ni)
        return ni

    def add_group(self, name, translation=None, rotation_y=0.0, scale=None):
        node = {"name": name}
        if translation:
            node["translation"] = [float(x) for x in translation]
        if rotation_y:
            h = math.radians(rotation_y) / 2
            node["rotation"] = [0.0, math.sin(h), 0.0, math.cos(h)]
        if scale is not None:
            node["scale"] = [float(scale)] * 3
        self.gltf["nodes"].append(node)
        ni = len(self.gltf["nodes"]) - 1
        self.gltf["scenes"][0]["nodes"].append(ni)
        return ni

    # -- merging ------------------------------------------------------------

    def merge_model(self, path, name, target_size, size_axis, position, yaw_deg,
                    ground_y):
        """Merge an external GLB under a wrapper node, scaled so its bbox
        measures target_size along size_axis ('y' = height, 'xz' = longest
        horizontal), seated with its bbox bottom at ground_y."""
        src, sbin = load_glb(path)
        for ext in src.get("extensionsUsed", []):
            if ext not in ("KHR_materials_transmission", "KHR_materials_ior",
                           "KHR_materials_volume", "KHR_materials_dispersion",
                           "KHR_materials_emissive_strength", "KHR_lights_punctual",
                           "KHR_materials_specular"):
                print(f"  [warn] {Path(path).name}: extension {ext} may not be supported")

        self._pad()
        bin_base = len(self.bin)
        self.bin.extend(sbin)
        bv_base = len(self.gltf["bufferViews"])
        acc_base = len(self.gltf["accessors"])
        img_base = len(self.gltf["images"])
        tex_base = len(self.gltf["textures"])
        mat_base = len(self.gltf["materials"])
        mesh_base = len(self.gltf["meshes"])
        node_base = len(self.gltf["nodes"])
        smp_base = len(self.gltf["samplers"])

        for bv in src.get("bufferViews", []):
            nbv = dict(bv)
            nbv["buffer"] = 0
            nbv["byteOffset"] = bv.get("byteOffset", 0) + bin_base
            self.gltf["bufferViews"].append(nbv)
        for acc in src.get("accessors", []):
            nacc = dict(acc)
            if "sparse" in nacc:
                print(f"  [warn] sparse accessor in {name}; dropping sparse data")
                del nacc["sparse"]
            if "bufferView" in nacc:
                nacc["bufferView"] += bv_base
            self.gltf["accessors"].append(nacc)
        src_dir = Path(path).resolve().parent
        for img in src.get("images", []):
            nimg = dict(img)
            if "bufferView" in nimg:
                nimg["bufferView"] += bv_base
            elif "uri" in nimg and not nimg["uri"].startswith("data:"):
                rel = Path(src_dir / nimg["uri"]).resolve()
                nimg["uri"] = rel.relative_to(OUT_DIR.resolve().parent.parent).as_posix()
                nimg["uri"] = "../../" + nimg["uri"]
                print(f"  [warn] external image {img['uri']} -> {nimg['uri']}")
            self.gltf["images"].append(nimg)
        for smp in src.get("samplers", []):
            self.gltf["samplers"].append(dict(smp))
        for tex in src.get("textures", []):
            ntex = dict(tex)
            if "source" in ntex:
                ntex["source"] += img_base
            ntex["sampler"] = ntex["sampler"] + smp_base if "sampler" in ntex else 0
            self.gltf["textures"].append(ntex)

        def remap_textures(obj):
            if isinstance(obj, dict):
                for k, v in obj.items():
                    if isinstance(v, dict) and "index" in v and "exture" in k:
                        v["index"] += tex_base
                    else:
                        remap_textures(v)
            elif isinstance(obj, list):
                for v in obj:
                    remap_textures(v)

        for mat in src.get("materials", []):
            nmat = json.loads(json.dumps(mat))
            nmat["name"] = f"{name}_{nmat.get('name', 'mat')}"
            remap_textures(nmat)
            for e in nmat.get("extensions", {}):
                self.use_ext(e)
            self.gltf["materials"].append(nmat)
        for mesh in src.get("meshes", []):
            nmesh = json.loads(json.dumps(mesh))
            nmesh.pop("weights", None)
            for prim in nmesh.get("primitives", []):
                prim.pop("targets", None)
                prim["attributes"] = {k: v + acc_base
                                      for k, v in prim["attributes"].items()
                                      if not k.startswith(("JOINTS", "WEIGHTS"))}
                if "indices" in prim:
                    prim["indices"] += acc_base
                if "material" in prim:
                    prim["material"] += mat_base
                prim.pop("extensions", None)
            self.gltf["meshes"].append(nmesh)
        for node in src.get("nodes", []):
            nnode = json.loads(json.dumps(node))
            for key in ("skin", "camera", "extensions", "extras"):
                nnode.pop(key, None)
            if "mesh" in nnode:
                nnode["mesh"] += mesh_base
            if "children" in nnode:
                nnode["children"] = [c + node_base for c in nnode["children"]]
            self.gltf["nodes"].append(nnode)

        roots = [r + node_base for r in
                 src["scenes"][src.get("scene", 0)]["nodes"]]

        lo, hi = self._scene_bbox(src, sbin)
        ext = hi - lo
        if size_axis == "y":
            cur = ext[1]
        else:
            cur = max(ext[0], ext[2])
        scale = target_size / cur if cur > 0 else 1.0
        cx, cz = (lo[0] + hi[0]) / 2 * scale, (lo[2] + hi[2]) / 2 * scale
        ty = ground_y - lo[1] * scale
        print(f"  {name}: raw size {ext.round(2)}, scale x{scale:.4f} "
              f"-> {(ext * scale).round(2)} m")

        wrapper = {"name": name, "children": roots,
                   "translation": [position[0] - cx, ty, position[1] - cz],
                   "scale": [scale] * 3}
        if yaw_deg:
            h = math.radians(yaw_deg) / 2
            wrapper["rotation"] = [0.0, math.sin(h), 0.0, math.cos(h)]
        self.gltf["nodes"].append(wrapper)
        self.gltf["scenes"][0]["nodes"].append(len(self.gltf["nodes"]) - 1)

    def _scene_bbox(self, src, sbin):
        lo = np.array([np.inf] * 3)
        hi = -lo.copy()

        def acc_minmax(ai):
            acc = src["accessors"][ai]
            if "min" in acc and "max" in acc:
                return np.array(acc["min"], float), np.array(acc["max"], float)
            bv = src["bufferViews"][acc["bufferView"]]
            off = bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
            stride = bv.get("byteStride", 12)
            data = np.frombuffer(sbin, np.uint8,
                                 count=stride * acc["count"], offset=off)
            pts = np.lib.stride_tricks.as_strided(
                data.view(np.float32),
                shape=(acc["count"], 3), strides=(stride, 4)).astype(float)
            return pts.min(axis=0), pts.max(axis=0)

        def walk(ni, mtx):
            node = src["nodes"][ni]
            local = np.eye(4)
            if "matrix" in node:
                local = np.array(node["matrix"], float).reshape(4, 4).T
            else:
                t = node.get("translation", [0, 0, 0])
                r = node.get("rotation", [0, 0, 0, 1])
                s = node.get("scale", [1, 1, 1])
                x, y, z, w = r
                R = np.array([
                    [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                    [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                    [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
                local[:3, :3] = R * np.array(s, float)[None, :]
                local[:3, 3] = t
            m = mtx @ local
            if "mesh" in node:
                for prim in src["meshes"][node["mesh"]]["primitives"]:
                    if "POSITION" not in prim["attributes"]:
                        continue
                    amin, amax = acc_minmax(prim["attributes"]["POSITION"])
                    corners = np.array([[amin[0], amin[1], amin[2]],
                                        [amin[0], amin[1], amax[2]],
                                        [amin[0], amax[1], amin[2]],
                                        [amin[0], amax[1], amax[2]],
                                        [amax[0], amin[1], amin[2]],
                                        [amax[0], amin[1], amax[2]],
                                        [amax[0], amax[1], amin[2]],
                                        [amax[0], amax[1], amax[2]]])
                    wc = (m[:3, :3] @ corners.T).T + m[:3, 3]
                    nonlocal lo, hi
                    lo = np.minimum(lo, wc.min(axis=0))
                    hi = np.maximum(hi, wc.max(axis=0))
            for c in node.get("children", []):
                walk(c, m)

        for r in src["scenes"][src.get("scene", 0)]["nodes"]:
            walk(r, np.eye(4))
        return lo, hi

    def save(self, path):
        self.gltf["buffers"][0]["byteLength"] = len(self.bin)
        if not self.gltf["extensionsUsed"]:
            del self.gltf["extensionsUsed"]
        save_glb(path, self.gltf, self.bin)
        print(f"  wrote {path} ({(12 + len(self.bin)) / 1e6:.1f} MB)")


# ---------------------------------------------------------------------------
# Shared scene pieces
# ---------------------------------------------------------------------------

GLASS_EXT = {
    "KHR_materials_transmission": {"transmissionFactor": 1.0},
    "KHR_materials_ior": {"ior": 1.5},
    "KHR_materials_volume": {"thicknessFactor": 0.35,
                             "attenuationColor": [0.92, 0.98, 0.95],
                             "attenuationDistance": 1.0},
}

WATER_EXT = {
    "KHR_materials_transmission": {"transmissionFactor": 1.0},
    "KHR_materials_ior": {"ior": 1.33},
    "KHR_materials_volume": {"thicknessFactor": 3.0,
                             "attenuationColor": [0.12, 0.42, 0.45],
                             "attenuationDistance": 3.0},
}


def rock_displace(seed):
    rng = np.random.default_rng(seed)
    grid = rng.random((5, 5, 5)).astype(np.float32)

    def f(dirs):
        p = (dirs * 0.5 + 0.5) * 3.999
        i = np.floor(p).astype(int)
        t = p - i
        t = t * t * (3 - 2 * t)
        acc = np.zeros(len(p), np.float32)
        for dx in (0, 1):
            for dy in (0, 1):
                for dz in (0, 1):
                    w = (np.where(dx, t[:, 0], 1 - t[:, 0]) *
                         np.where(dy, t[:, 1], 1 - t[:, 1]) *
                         np.where(dz, t[:, 2], 1 - t[:, 2]))
                    acc += w * grid[i[:, 0] + dx, i[:, 1] + dy, i[:, 2] + dz]
        return (acc - 0.5) * 0.55
    return f


def add_spheres(b, ground_h, gx, gz, mx, mz):
    """Standard look-dev pair: glass + chrome sphere, 0.35 m radius."""
    glass_mat = b.add_pbr_material("GlassSphere", base_color=[1, 1, 1, 1],
                                   metallic=0.0, roughness=0.0,
                                   extensions=GLASS_EXT)
    chrome_mat = b.add_pbr_material("ChromeSphere", base_color=[0.95, 0.95, 0.96, 1],
                                    metallic=1.0, roughness=0.12)
    sph = uv_sphere(0.35)
    b.add_mesh_node("GlassSphere", sph, glass_mat, [gx, ground_h(gx, gz) + 0.35, gz])
    b.add_mesh_node("ChromeSphere", sph, chrome_mat, [mx, ground_h(mx, mz) + 0.35, mz])


def add_rocks(b, ground_h, spots, mat=None):
    if mat is None:
        mat = b.add_pbr_material("Rock", tex_kind="rock", roughness=0.95)
    for i, (x, z, r) in enumerate(spots):
        rock = uv_sphere(r, 32, 24, displace=rock_displace(900 + i))
        y = ground_h(x, z) + r * 0.35
        b.add_mesh_node(f"Rock{i + 1}", rock, mat, [x, y, z], rotation_y=i * 47.0)
    return mat


def add_house(b, ground_h, x, z, yaw):
    grp = b.add_group("House", [x, ground_h(x, z), z], rotation_y=yaw)
    wall = b.add_pbr_material("HouseWall", tex_kind="plaster", roughness=0.85)
    roof = b.add_pbr_material("HouseRoof", tex_kind="roof", roughness=0.8)
    wood = b.add_pbr_material("HouseDoor", tex_kind="wood", roughness=0.6)
    glass = b.add_pbr_material("WindowGlass", base_color=[1, 1, 1, 1], metallic=0.0,
                               roughness=0.02, extensions=GLASS_EXT,
                               double_sided=True)
    walls = box_mesh(6.4, 2.8, 4.6, uv_scale=0.5)
    b.add_mesh_node("HouseWalls", walls, wall, [0, 1.4, 0], parent=grp)
    b.add_mesh_node("HouseRoofMesh", gable_roof(6.4, 4.6, 1.6, 0.35), roof,
                    [0, 2.8, 0], parent=grp)
    b.add_mesh_node("HouseDoorMesh", box_mesh(0.95, 2.05, 0.08, uv_scale=0.5),
                    wood, [0.9, 1.03, 2.33], parent=grp)
    for i, wx in enumerate((-1.6, -0.2)):
        b.add_mesh_node(f"HouseWindow{i + 1}", box_mesh(1.1, 1.2, 0.05), glass,
                        [wx, 1.5, 2.33], parent=grp)
    return grp


def add_man(b, ground_h, x, z, yaw):
    b.merge_model(MODELS / "glTF-Sample-Assets/Models/CesiumMan/glTF-Binary/CesiumMan.glb",
                  "Man", target_size=1.76, size_axis="y",
                  position=(x, z), yaw_deg=yaw, ground_y=ground_h(x, z))


# ---------------------------------------------------------------------------
# The four scenes
# ---------------------------------------------------------------------------

def scene_grassland():
    b = Builder()
    hills = np.random.default_rng(7).random((9, 9)).astype(np.float32)

    def ground_h(x, z):
        x = np.asarray(x, np.float32)
        z = np.asarray(z, np.float32)
        h = (np.sin(x * 0.08 + 1.0) * np.cos(z * 0.06) * 0.5 +
             np.sin(x * 0.021 + z * 0.017) * 0.7)
        # flatten the centre where the objects stand
        damp = np.clip((np.sqrt(x * x + z * z) - 6.0) / 18.0, 0, 1)
        return h * damp * 0.9

    gmat = b.add_pbr_material("GrassGround", tex_kind="grass", roughness=0.9)
    b.add_mesh_node("Ground", grid_mesh(120, 160, ground_h, uv_repeat=28), gmat)

    b.merge_model(MODELS / "su7/source/SU7.glb", "Car", target_size=5.0,
                  size_axis="xz", position=(0.0, 0.0), yaw_deg=25,
                  ground_y=float(ground_h(0.0, 0.0)))
    add_man(b, ground_h, 2.6, 1.8, -30)
    add_house(b, ground_h, -9.0, -11.0, 155)
    add_rocks(b, ground_h, [(-4.5, -3.0, 0.9), (5.5, -4.5, 0.6), (-2.0, 3.5, 0.4)])
    add_spheres(b, ground_h, 1.0, 4.0, 3.1, 3.2)
    b.save(OUT_DIR / "grassland.glb")


def scene_desert():
    b = Builder()

    def ground_h(x, z):
        x = np.asarray(x, np.float32)
        z = np.asarray(z, np.float32)
        dunes = (np.sin(x * 0.045 + z * 0.02) * 1.1 +
                 np.sin(x * 0.013 - z * 0.031 + 2.0) * 1.6 +
                 np.sin((x + z) * 0.09) * 0.35)
        damp = np.clip((np.sqrt(x * x + z * z) - 7.0) / 20.0, 0, 1)
        return dunes * damp * 0.85

    smat = b.add_pbr_material("DesertSand", tex_kind="sand", roughness=0.95)
    b.add_mesh_node("Ground", grid_mesh(120, 200, ground_h, uv_repeat=48), smat)

    b.merge_model(MODELS / "su7/source/SU7.glb", "Car", target_size=5.0,
                  size_axis="xz", position=(0.0, 0.0), yaw_deg=-40,
                  ground_y=float(ground_h(0.0, 0.0)))
    add_man(b, ground_h, -2.8, 2.0, 40)
    add_rocks(b, ground_h, [(-6.0, -4.0, 1.1), (4.5, -6.0, 0.8), (7.0, 2.0, 0.5),
                            (-3.5, -8.0, 0.6), (2.0, 5.0, 0.35)])
    add_spheres(b, ground_h, 2.2, 3.0, 3.2, 3.3)
    b.save(OUT_DIR / "desert.glb")


def scene_seaside():
    b = Builder()

    def sig(t):
        return 1.0 / (1.0 + np.exp(-t))

    def ground_h(x, z):
        x = np.asarray(x, np.float32)
        z = np.asarray(z, np.float32)
        # shoreline at z ~ 0: land (z>0) rises to +1.6 m, seabed drops to -3 m
        h = 1.7 * sig((z - 4.0) / 5.0) - 3.2 * sig(-(z - 1.0) / 6.0) + 0.75
        h = h + np.sin(x * 0.05 + 1.3) * 0.2 * sig((z - 4.0) / 5.0)
        return h

    def water_h(x, z):
        x = np.asarray(x, np.float32)
        z = np.asarray(z, np.float32)
        return (0.05 * np.sin(0.9 * x + 0.4 * z) +
                0.035 * np.sin(0.5 * x - 0.9 * z + 1.7) +
                0.02 * np.sin(2.2 * x + 1.4 * z + 0.6) +
                0.015 * np.sin(3.6 * z + 2.9))

    sand = b.add_pbr_material("BeachSand", tex_kind="sand", roughness=0.9)
    b.add_mesh_node("Ground", grid_mesh(140, 200, ground_h, uv_repeat=56), sand)

    water = b.add_pbr_material("SeaWater", base_color=[1, 1, 1, 1], metallic=0.0,
                               roughness=0.02, extensions=WATER_EXT,
                               double_sided=True)
    b.add_mesh_node("Water", grid_mesh(140, 280, water_h, uv_repeat=1), water)

    b.merge_model(MODELS / "su7/source/SU7.glb", "Car", target_size=5.0,
                  size_axis="xz", position=(0.0, 11.0), yaw_deg=120,
                  ground_y=float(ground_h(0.0, 11.0)))
    add_man(b, ground_h, -1.0, 5.0, 170)
    add_house(b, ground_h, -9.0, 10.0, 210)
    add_rocks(b, ground_h, [(-4.0, 1.0, 1.0), (-6.5, -1.5, 0.7),
                            (5.0, 0.0, 0.55), (6.5, -2.5, 0.8)])
    add_spheres(b, ground_h, 4.0, 7.5, 5.6, 8.2)
    b.save(OUT_DIR / "seaside.glb")


def scene_concrete():
    b = Builder()

    def ground_h(x, z):
        return np.zeros_like(np.asarray(x, np.float32))

    cmat = b.add_pbr_material("ConcreteGround", tex_kind="concrete", roughness=0.8)
    # concrete texture tile = 4 m (joints every 2 m)
    b.add_mesh_node("Ground", grid_mesh(120, 40, ground_h, uv_repeat=30), cmat)

    b.merge_model(MODELS / "su7/source/SU7.glb", "Car", target_size=5.0,
                  size_axis="xz", position=(1.2, -0.8), yaw_deg=-15, ground_y=0.0)
    add_man(b, ground_h, -1.6, 2.4, 15)
    add_house(b, ground_h, -10.0, -12.0, 140)

    block = b.add_pbr_material("ConcreteBlock", tex_kind="concrete", roughness=0.8)
    for i, (x, z) in enumerate([(6.0, 3.0), (7.4, 3.0), (6.7, 4.0)]):
        b.add_mesh_node(f"Block{i + 1}", box_mesh(1.2, 0.8, 0.6, uv_scale=0.5),
                        block, [x, 0.4, z], rotation_y=i * 30.0)
    add_spheres(b, ground_h, 1.8, 3.4, 2.9, 3.7)
    b.save(OUT_DIR / "concrete.glb")


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for fn in (scene_grassland, scene_desert, scene_seaside, scene_concrete):
        print(f"[{fn.__name__}]")
        fn()
    print("done")


if __name__ == "__main__":
    main()
