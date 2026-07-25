# ============================================================================
# Spectral asset drift check
# ============================================================================
# assets/spectral/ is a hand-made copy of the same directory in Quantiloom-dev.
# The SDK installs assets/atmos_models/ but not this, so nothing propagates a
# re-bake: the copy here sat five months out of date -- still carrying a USGS
# basis whose MWIR and LWIR bands were edge-clamp extrapolations rather than
# measurements -- and nothing anywhere reported it.
#
# MANIFEST.sha256 pins what both sides are supposed to contain. This module
# compares against it at configure time and names which side drifted, so a
# re-bake on either side surfaces on the next build instead of on inspection.
# Run scripts/sync_spectral_assets.sh to re-copy from dev and re-pin.
#
# Warnings only: these assets are not loaded by the GUI (nothing here reads
# spectral.basis_file -- that key is consumed by the CLI in Quantiloom-dev), so
# drift is a correctness problem for anyone feeding these configs to the CLI,
# not a reason to block a build.

function(quantiloom_check_spectral_assets local_dir)
    set(manifest "${local_dir}/MANIFEST.sha256")
    if(NOT EXISTS "${manifest}")
        message(WARNING "Spectral assets: ${manifest} is missing, cannot check for drift.")
        return()
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${manifest}")

    # Locate the source repository, if this machine has one. Overridable for a
    # checkout that does not sit beside this one.
    set(dev_dir "${QUANTILOOM_DEV_ROOT}/assets/spectral")
    set(have_dev FALSE)
    if(QUANTILOOM_DEV_ROOT AND IS_DIRECTORY "${dev_dir}")
        set(have_dev TRUE)
    endif()

    file(STRINGS "${manifest}" lines)
    set(local_drift "")
    set(local_missing "")
    set(dev_drift "")

    foreach(line IN LISTS lines)
        # sha256sum format: "<64 hex>  <name>"
        if(NOT line MATCHES "^([0-9a-fA-F]+) [ *](.+)$")
            continue()
        endif()
        set(expected "${CMAKE_MATCH_1}")
        set(name "${CMAKE_MATCH_2}")

        if(NOT EXISTS "${local_dir}/${name}")
            list(APPEND local_missing "${name}")
        else()
            file(SHA256 "${local_dir}/${name}" actual)
            if(NOT actual STREQUAL expected)
                list(APPEND local_drift "${name}")
            endif()
        endif()

        if(have_dev AND EXISTS "${dev_dir}/${name}")
            file(SHA256 "${dev_dir}/${name}" dev_actual)
            if(NOT dev_actual STREQUAL expected)
                list(APPEND dev_drift "${name}")
            endif()
        endif()
    endforeach()

    if(local_missing)
        string(REPLACE ";" "\n    " pretty "${local_missing}")
        message(WARNING
            "Spectral assets missing from ${local_dir}:\n    ${pretty}\n"
            "Run scripts/sync_spectral_assets.sh to restore them.")
    endif()

    if(local_drift AND dev_drift)
        # Both sides moved away from the pin: the manifest itself is stale.
        message(WARNING
            "Spectral assets: this repo and Quantiloom-dev have both diverged "
            "from MANIFEST.sha256. Re-pin deliberately with "
            "scripts/sync_spectral_assets.sh --sync.")
    elseif(local_drift)
        string(REPLACE ";" "\n    " pretty "${local_drift}")
        message(WARNING
            "Spectral assets in this repo differ from MANIFEST.sha256:\n    ${pretty}\n"
            "Run scripts/sync_spectral_assets.sh to re-copy from Quantiloom-dev.")
    elseif(dev_drift)
        string(REPLACE ";" "\n    " pretty "${dev_drift}")
        message(WARNING
            "Quantiloom-dev has re-baked spectral assets that this repo has not "
            "picked up:\n    ${pretty}\n"
            "Run scripts/sync_spectral_assets.sh --sync to update this copy.")
    elseif(have_dev)
        message(STATUS "Spectral assets: in sync with ${dev_dir}")
    else()
        message(STATUS "Spectral assets: match MANIFEST.sha256 (Quantiloom-dev not present, "
                       "so the source side was not checked)")
    endif()
endfunction()
