#!/usr/bin/env python3
"""Reconstruct USGS/ECOSTRESS/RII spectra from the NMF basis and report colour.

Mirrors SpectralBasisLoader::ReconstructCurve so a `spectral_material_ref`
can be checked -- reflectance level, and what it looks like under D65 -- before
committing it to a scene config.

    python3 scripts/spectral_preview.py usgs "Cheatgrass ANPC1 field calib"
    python3 scripts/spectral_preview.py --search usgs grass
"""

import json
import struct
import sys
from pathlib import Path

import numpy as np

SPEC = Path(__file__).resolve().parents[1] / "assets" / "spectral"
LUTS = Path(__file__).resolve().parents[1] / "assets" / "luts"


def load_basis(path):
    data = path.read_bytes()
    assert data[:4] == b"QBAS", "not a QBAS basis file"
    version, num_bands = struct.unpack_from("<II", data, 4)
    assert version == 3, f"unsupported basis version {version}"
    off, bands = 64, {}
    names = ["VIS", "NIR", "SWIR", "MWIR", "LWIR"]
    for bi in range(num_bands):
        start_um, end_um, n_samp, n_basis = struct.unpack_from("<ffII", data, off)
        off += 16
        arr = np.frombuffer(data, np.float32, count=n_basis * n_samp, offset=off)
        off += n_basis * n_samp * 4
        bands[names[bi]] = {
            "lambda": np.linspace(start_um, end_um, n_samp) * 1000.0,
            "basis": arr.reshape(n_basis, n_samp),
        }
    return bands


def load_cmf():
    """CIE 1931 2-deg CMFs, from the LUT the renderer itself uses."""
    rows = []
    for line in (LUTS / "CIE_xyz_1931_2deg.csv").read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = [p for p in line.replace(",", " ").split() if p]
        try:
            rows.append([float(p) for p in parts[:4]])
        except ValueError:
            continue
    a = np.array(rows)
    return a[:, 0], a[:, 1:4]


def load_d65():
    """Global-tilt solar spectrum from ASTM G-173 as the viewing illuminant."""
    rows = []
    for line in (LUTS / "astmg173.csv").read_text().splitlines():
        parts = line.split(",")
        if len(parts) < 4:
            continue
        try:
            rows.append((float(parts[0]), float(parts[2])))
        except ValueError:
            continue
    a = np.array(rows)
    return a[:, 0], a[:, 1]


XYZ_TO_RGB = np.array([[3.2406, -1.5372, -0.4986],
                       [-0.9689, 1.8758, 0.0415],
                       [0.0557, -0.2040, 1.0570]])


def curve_to_srgb(lam, refl):
    cl, cmf = load_cmf()
    il, iv = load_d65()
    grid = np.linspace(400, 700, 61)
    r = np.interp(grid, lam, refl, left=refl[0], right=refl[-1])
    c = np.stack([np.interp(grid, cl, cmf[:, i]) for i in range(3)], axis=1)
    e = np.interp(grid, il, iv)
    xyz = (r * e)[:, None] * c
    white = e[:, None] * c
    xyz = xyz.sum(0) / white.sum(0)[1]
    rgb = XYZ_TO_RGB @ xyz
    rgb = np.clip(rgb, 0, None)
    srgb = np.where(rgb <= 0.0031308, 12.92 * rgb, 1.055 * rgb ** (1 / 2.4) - 0.055)
    return np.clip(srgb, 0, 1)


def reconstruct(db, name, band="VIS"):
    bands = load_basis(SPEC / f"quantiloom_basis_v3_{db}.qlbin")
    mats = json.loads((SPEC / f"quantiloom_materials_{db}.json").read_text())["materials"]
    if name not in mats:
        raise KeyError(name)
    entry = mats[name]["bands"].get(band)
    if entry is None:
        raise KeyError(f"{name} has no {band} band")
    w = np.array(entry["basis_weights"], np.float32)
    b = bands[band]
    return b["lambda"], np.clip(w @ b["basis"], 0.0, 1.0), entry


def describe(db, name, band="VIS"):
    lam, refl, entry = reconstruct(db, name, band)
    rgb = curve_to_srgb(lam, refl)
    vis = (lam >= 400) & (lam <= 700)
    blue = refl[(lam >= 430) & (lam <= 490)].mean()
    green = refl[(lam >= 520) & (lam <= 580)].mean()
    red = refl[(lam >= 620) & (lam <= 680)].mean()
    r, g, b_ = (rgb * 255).astype(int)
    swatch = f"\x1b[48;2;{r};{g};{b_}m      \x1b[0m"
    print(f"{swatch} [{db}] {name!r}")
    print(f"     mean rho(400-700) = {refl[vis].mean():.3f}   "
          f"B={blue:.3f} G={green:.3f} R={red:.3f}   "
          f"sRGB=({r},{g},{b_})   coverage={entry.get('coverage')}")
    return refl[vis].mean(), rgb


def search(db, pattern, band="VIS", limit=14):
    import re
    mats = json.loads((SPEC / f"quantiloom_materials_{db}.json").read_text())["materials"]
    p = re.compile(pattern, re.I)
    hits = [n for n in mats
            if p.search(n) and (mats[n]["bands"].get(band, {}).get("coverage") or 0) >= 0.99]
    hits.sort(key=lambda n: mats[n]["bands"][band].get("rmse", 9))
    for n in hits[:limit]:
        try:
            describe(db, n, band)
        except Exception as exc:  # noqa: BLE001
            print(f"     !! {n}: {exc}")


if __name__ == "__main__":
    args = sys.argv[1:]
    if args and args[0] == "--search":
        search(args[1], args[2], *(args[3:4] or []))
    else:
        for name in args[1:]:
            describe(args[0], name)
