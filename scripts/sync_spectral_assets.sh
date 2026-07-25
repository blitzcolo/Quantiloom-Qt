#!/usr/bin/env bash
# Keep assets/spectral/ in step with the same directory in Quantiloom-dev.
#
# These files are baked in Quantiloom-dev and copied here by hand -- the SDK
# installs assets/atmos_models/ but not assets/spectral/, so a re-bake there
# reaches this repo only if somebody carries it across. That is how this copy
# came to be five months stale while still being the version anyone reading the
# repo would assume was current.
#
#   ./scripts/sync_spectral_assets.sh          # check only (default)
#   ./scripts/sync_spectral_assets.sh --sync   # copy from dev, re-pin manifest
#
# The check also runs at CMake configure time (cmake/CheckSpectralAssets.cmake);
# this script is the manual entry point and the only way to re-pin.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
local_dir="$repo_root/assets/spectral"
dev_dir="${QUANTILOOM_DEV_ROOT:-$repo_root/../Quantiloom-dev}/assets/spectral"
manifest="$local_dir/MANIFEST.sha256"

mode="check"
case "${1:-}" in
    --sync)  mode="sync" ;;
    --check|"") ;;
    *) echo "usage: $(basename "$0") [--check|--sync]" >&2; exit 2 ;;
esac

if [[ ! -f $manifest ]]; then
    echo "error: $manifest not found" >&2
    exit 1
fi

# The manifest is the list of files as well as their hashes: anything not named
# there is not part of the synced set (ecospeclib-all/, for instance, is raw
# input that lives only in dev).
mapfile -t files < <(awk '{ sub(/^\*/, "", $2); print $2 }' "$manifest")

if [[ $mode == sync ]]; then
    if [[ ! -d $dev_dir ]]; then
        echo "error: $dev_dir not found; set QUANTILOOM_DEV_ROOT" >&2
        exit 1
    fi
    for f in "${files[@]}"; do
        if [[ ! -f "$dev_dir/$f" ]]; then
            echo "error: $dev_dir/$f is missing; refusing a partial sync" >&2
            exit 1
        fi
    done
    for f in "${files[@]}"; do
        cp -f "$dev_dir/$f" "$local_dir/$f"
        echo "  copied $f"
    done
    ( cd "$local_dir" && sha256sum "${files[@]}" > MANIFEST.sha256 )
    echo "re-pinned $manifest against $dev_dir"
    exit 0
fi

# --- check ---------------------------------------------------------------
status=0

echo "this repo vs MANIFEST.sha256:"
if ( cd "$local_dir" && sha256sum -c MANIFEST.sha256 --quiet ); then
    echo "  all ${#files[@]} files match"
else
    echo "  ^ run '$(basename "$0") --sync' to re-copy from Quantiloom-dev"
    status=1
fi

if [[ -d $dev_dir ]]; then
    echo "Quantiloom-dev vs MANIFEST.sha256:"
    if ( cd "$dev_dir" && sha256sum -c "$manifest" --quiet ); then
        echo "  all ${#files[@]} files match"
    else
        echo "  ^ dev has re-baked; run '$(basename "$0") --sync' to pick it up"
        status=1
    fi
else
    echo "Quantiloom-dev not found at $dev_dir; source side not checked."
fi

exit $status
