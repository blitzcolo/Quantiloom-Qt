#!/usr/bin/env python3
"""Diurnal forcing files for the env_* scenes.

The thermal solver reads a CSV of what the outside world is doing hour by
hour; this writes one per environment, into assets/forcing/.

The point of generating them rather than typing them is the sun. Each scene
already has a `[lighting] sun_direction` that its camera framing and its
shadows were composed around, and the solver takes its own sun from this file
-- so unless the two agree at the hour being rendered, the surface that is
warm is not the surface that is lit. Here the scene's direction is the input:
the script solves for the latitude and declination that put the sun exactly
there at that scene's hour, then walks the whole day along that same path.

Everything else is a climate profile per scene: how far the air swings, how
dry it is, how much haze stands between the ground and the sun.

    python3 scripts/gen_env_forcing.py [--check]

--check verifies the generated path reproduces each scene's sun direction and
writes nothing.
"""

import argparse
import math
import os

# --- the correlations the renderer itself uses -----------------------------
# Magnus, then Berdahl-Fromberg, matching core/SkyThermal.cpp. Written out
# again rather than approximated so the sky column here is the same sky the
# clear-sky model would derive from this air.
MAGNUS_A = 17.625
MAGNUS_B = 243.04


def dew_point_c(air_c, rh_percent):
    rh = min(max(rh_percent, 1.0), 100.0)
    gamma = math.log(rh / 100.0) + MAGNUS_A * air_c / (MAGNUS_B + air_c)
    return MAGNUS_B * gamma / (MAGNUS_A - gamma)


def clear_sky_emissivity(td_c):
    t = td_c / 100.0
    return min(max(0.711 + 0.56 * t + 0.73 * t * t, 0.0), 1.0)


def sky_temperature_k(air_k, rh_percent):
    eps = clear_sky_emissivity(dew_point_c(air_k - 273.15, rh_percent))
    return air_k * eps**0.25


# --- solar geometry --------------------------------------------------------
# The scene's world is the horizon frame the loader assumes: +x east, +y up,
# +z south. LoadForcingCsv rebuilds the direction as
#     (cos(el) sin(az), sin(el), -cos(el) cos(az))
# so east/up/north come straight off the vector.


def enu(direction):
    x, y, z = direction
    n = math.sqrt(x * x + y * y + z * z)
    return x / n, y / n, -z / n  # east, up, north


OBLIQUITY_RAD = math.radians(23.44)  # the declination cannot leave this band


def solve_latitude_and_bearing(direction, declination, hour):
    """Latitude, and the compass bearing of the scene's -z axis.

    Three quantities decide where the sun is -- latitude, season, hour -- and
    a direction only constrains two of them, so something has to give. What
    gives is the assumption that a scene's z axis points north: it has no
    reason to. These worlds were modelled around a camera, not a compass.

    So the season and the hour are stated, the latitude is solved from the
    elevation, and the leftover azimuth becomes the bearing the scene is built
    at. That keeps the sun a real one -- a declination this planet has, at an
    hour the scene was composed for -- and still lands it exactly on the
    direction the scene is lit from.
    """
    east, up, north = enu(direction)
    elevation = math.asin(min(max(up, -1.0), 1.0))
    scene_azimuth = math.atan2(east, north)

    h = math.radians(15.0 * (hour - 12.0))
    a = math.sin(declination)
    b = math.cos(declination) * math.cos(h)
    radius = math.hypot(a, b)
    if radius < 1e-9 or abs(math.sin(elevation)) > radius:
        # Worth spelling out: this is a real limit, not a solver failure. The
        # sun cannot be arbitrarily high arbitrarily far from noon, at any
        # latitude, so a scene lit from high up has to be rendered near noon.
        best = math.hypot(math.sin(OBLIQUITY_RAD), math.cos(OBLIQUITY_RAD) * math.cos(h))
        raise SystemExit(
            "  %s: no latitude reaches %.1f deg elevation at %.1f h -- the most "
            "any season manages that far from noon is %.1f deg. Move the hour "
            "toward noon."
            % ("scene", math.degrees(elevation), hour,
               math.degrees(math.asin(min(best, 1.0)))))

    phase = math.atan2(b, a)
    base = math.asin(math.sin(elevation) / radius)
    candidates = [base - phase, math.pi - base - phase]
    # Both branches put the sun at the right height; the temperate one is the
    # one a reader will recognise as a place.
    latitude = min((c for c in candidates if abs(c) <= math.radians(72.0)),
                   key=abs, default=None)
    if latitude is None:
        raise SystemExit("  only polar latitudes fit that sun")

    compass_azimuth, _ = sun_at(latitude, declination, hour)
    bearing = math.radians(compass_azimuth) - scene_azimuth
    return latitude, bearing


def sun_at(latitude, declination, hour):
    """Azimuth from north (degrees) and elevation (degrees)."""
    h = math.radians(15.0 * (hour - 12.0))
    sin_lat, cos_lat = math.sin(latitude), math.cos(latitude)
    sin_dec, cos_dec = math.sin(declination), math.cos(declination)

    east = -cos_dec * math.sin(h)
    north = sin_dec * cos_lat - cos_dec * sin_lat * math.cos(h)
    up = sin_dec * sin_lat + cos_dec * cos_lat * math.cos(h)

    elevation = math.degrees(math.asin(min(max(up, -1.0), 1.0)))
    azimuth = math.degrees(math.atan2(east, north)) % 360.0
    return azimuth, elevation


# --- the scenes ------------------------------------------------------------
# hour is the hour the scene's own [lighting] sun_direction was composed for,
# and declination_deg is the season that makes that sun a real one. Together
# they fix the latitude and the compass bearing the scene sits at.
#
# air_min/air_max are the daily extremes, reached at 03:00 and 15:00 -- the
# lag that comes of the ground heating the air rather than the other way
# round. turbidity is the ASHRAE beam exponent: the larger it is the more the
# haze takes out of the direct beam and hands to the sky, which is what
# diffuse_ratio then puts back.
SCENES = [
    dict(
        name="concrete",
        sun_direction=(-0.45, 0.79, 0.42),
        hour=14.0, declination_deg=20.0,
        air_min=297.0, air_max=309.0,
        rh_mean=42.0, rh_swing=12.0,
        turbidity=0.24, diffuse_ratio=0.26,
        note="Urban pad: the city holds its heat overnight and the haze puts a "
             "quarter of the beam into the sky.",
    ),
    dict(
        name="desert",
        sun_direction=(0.35, 0.88, 0.2),
        hour=11.0, declination_deg=23.0,
        air_min=289.0, air_max=320.0,
        rh_mean=15.0, rh_swing=8.0,
        turbidity=0.15, diffuse_ratio=0.09,
        note="Thirty kelvin between night and afternoon, and air too dry to "
             "stop the ground radiating: the widest swing of the four.",
    ),
    dict(
        name="grassland",
        sun_direction=(0.45, 0.71, 0.32),
        hour=10.0, declination_deg=8.0,
        air_min=285.0, air_max=298.0,
        rh_mean=58.0, rh_swing=15.0,
        turbidity=0.19, diffuse_ratio=0.15,
        note="Temperate meadow. The grass evaporates, so its own surface never "
             "reaches the air's afternoon peak.",
    ),
    dict(
        name="seaside",
        sun_direction=(0.28, 0.36, -0.89),
        hour=16.0, declination_deg=14.0,
        air_min=292.0, air_max=300.0,
        rh_mean=76.0, rh_swing=8.0,
        turbidity=0.20, diffuse_ratio=0.18,
        note="Maritime air: humid enough to keep the sky warm and the swing "
             "small, which is most of why a coast is mild.",
    ),
]


def air_temperature(scene, hour):
    mean = 0.5 * (scene["air_min"] + scene["air_max"])
    amplitude = 0.5 * (scene["air_max"] - scene["air_min"])
    return mean + amplitude * math.cos(2.0 * math.pi * (hour - 15.0) / 24.0)


def humidity(scene, hour):
    # Anti-correlated with the air: the same water in warmer air reads drier.
    value = scene["rh_mean"] - scene["rh_swing"] * math.cos(
        2.0 * math.pi * (hour - 15.0) / 24.0)
    return min(max(value, 3.0), 100.0)


def irradiance(scene, elevation_deg):
    """Direct normal and diffuse horizontal, ASHRAE clear-sky."""
    if elevation_deg <= 0.5:
        return 0.0, 0.0
    air_mass = 1.0 / math.sin(math.radians(elevation_deg))
    dni = 1158.0 * math.exp(-scene["turbidity"] * air_mass)
    # Diffuse is quoted on the horizontal, which is what the solver's sky
    # fraction multiplies; the sine turns the beam figure into one.
    dhi = scene["diffuse_ratio"] * dni * math.sin(math.radians(elevation_deg))
    return dni, dhi


HEADER = """\
# {title}
#
# {note}
#
# Written by scripts/gen_env_forcing.py -- edit that, not this. The sun path is
# solved from env_{name}.toml's own [lighting] sun_direction: at {hour:.1f} h it
# points exactly where that scene is lit from, so the surface the solver warms
# is the surface the render shows lit. Latitude {lat:.1f} deg, declination
# {dec:.1f} deg.
#
# Columns, in the order ThermalSolver::LoadForcingCsv reads them. The last two
# are optional and were added with the diffuse-sky and evaporation terms; a
# file without them still reads.
#
#   time_h, air_temperature_k, sun_irradiance_w_m2,
#   sun_azimuth_deg, sun_elevation_deg, sky_temperature_k,
#   diffuse_irradiance_w_m2, relative_humidity
#
# Azimuth runs from north through east; elevation from the horizon. Below the
# horizon the beam is zero, which is what lets the ground cool. The sky
# temperature is Berdahl-Fromberg on this hour's air and humidity, the same
# correlation the renderer's clear-sky model uses.
#
# The column line is commented: LoadForcingCsv only tolerates a header on the
# very first line, and this file has a preamble.
#time_h,air_temperature_k,sun_irradiance_w_m2,sun_azimuth_deg,sun_elevation_deg,sky_temperature_k,diffuse_irradiance_w_m2,relative_humidity
"""


def generate(scene, out_dir, check_only):
    declination = math.radians(scene["declination_deg"])
    hour = scene["hour"]
    latitude, bearing = solve_latitude_and_bearing(
        scene["sun_direction"], declination, hour)

    def scene_azimuth_at(t):
        compass, elev = sun_at(latitude, declination, t)
        return math.degrees(math.radians(compass) - bearing) % 360.0, elev

    # The whole reason for solving: verify it lands back on the scene's vector.
    azimuth, elevation = scene_azimuth_at(hour)
    a, e = math.radians(azimuth), math.radians(elevation)
    rebuilt = (math.cos(e) * math.sin(a), math.sin(e), -math.cos(e) * math.cos(a))
    x, y, z = scene["sun_direction"]
    length = math.sqrt(x * x + y * y + z * z)
    want = (x / length, y / length, z / length)
    error = max(abs(rebuilt[i] - want[i]) for i in range(3))
    print("  %-10s lat %+6.1f  dec %+5.1f  bearing %+6.1f  ->  %.1f h, "
          "az %5.1f, el %4.1f   error %.1e"
          % (scene["name"], math.degrees(latitude), scene["declination_deg"],
             math.degrees(bearing) % 360.0, hour, azimuth, elevation, error))
    if error > 1e-6:
        raise SystemExit("  %s: solved path misses its own sun direction" % scene["name"])
    if check_only:
        return

    rows = []
    for step in range(25):
        t = float(step)
        azimuth, elevation = scene_azimuth_at(t)
        air = air_temperature(scene, t)
        rh = humidity(scene, t)
        dni, dhi = irradiance(scene, elevation)
        rows.append("%.1f,%.2f,%.1f,%.1f,%.1f,%.2f,%.1f,%.1f"
                    % (t, air, dni, azimuth, elevation,
                       sky_temperature_k(air, rh), dhi, rh))

    title = "Quantiloom - %s: a day's forcing for the surface energy balance" % (
        scene["name"].capitalize())
    path = os.path.join(out_dir, "%s_day.csv" % scene["name"])
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(HEADER.format(title=title, note=scene["note"],
                                   name=scene["name"], hour=scene["hour"],
                                   lat=math.degrees(latitude),
                                   dec=math.degrees(declination)))
        handle.write("\n".join(rows) + "\n")
    print("             wrote %s" % path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="verify the sun paths and write nothing")
    args = parser.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out_dir = os.path.join(root, "assets", "forcing")
    if not args.check:
        os.makedirs(out_dir, exist_ok=True)

    print("Solving each scene's sun path from its own lighting:")
    for scene in SCENES:
        generate(scene, out_dir, args.check)


if __name__ == "__main__":
    main()
