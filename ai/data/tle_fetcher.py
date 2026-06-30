"""
openorbit.data.tle_fetcher
==========================
Celestrak TLE (Two-Line Element set) fetcher and parser.

No API key is required — Celestrak publishes all data publicly.

    https://celestrak.org/

Catalogs
--------
``active``      — all active satellites (~5 k)
``stations``    — ISS, Tiangong, etc.
``starlink``    — SpaceX Starlink constellation
``gps-ops``     — operational GPS satellites
``galileo``     — Galileo navigation satellites
``visual``      — brightest satellites (naked-eye visible)
``weather``     — NOAA/MetOp/Suomi-NPP weather sats
``science``     — HST, Chandra, XMM-Newton, etc.
"""

from __future__ import annotations

import math
from typing import Any

from .space_client import get_json

_CELESTRAK_BASE = "https://celestrak.org/SOCRATES/query.php"
_CELESTRAK_GP   = "https://celestrak.org/SOCRATES/query.php"
_CELESTRAK_GP_URL = "https://celestrak.org/satcat/records.php"

# Use the Celestrak GP data endpoint (JSON format, no parsing needed)
_GP_URL = "https://celestrak.org/GP/GP.php"

_CATALOG_NAMES: dict[str, str] = {
    "active":   "active",
    "stations": "stations",
    "starlink": "starlink",
    "gps-ops":  "GPS-OPS",
    "galileo":  "Galileo",
    "visual":   "visual",
    "weather":  "weather",
    "science":  "science",
    "amateur":  "amateur",
    "cubesat":  "cubesat",
}

# Gravitational parameter for Earth (km³/s²)
_GM_EARTH = 398600.4418
# Earth radius (km)
_RE = 6378.137


def get_gp_data(catalog: str = "active") -> list[dict[str, Any]]:
    """
    Fetch GP (General Perturbations) orbital element sets from Celestrak in JSON.

    Parameters
    ----------
    catalog : Catalog name (see module docstring for options).

    Returns
    -------
    List of dicts, one per satellite, with keys such as:
    ``OBJECT_NAME``, ``NORAD_CAT_ID``, ``EPOCH``,
    ``MEAN_MOTION``, ``ECCENTRICITY``, ``INCLINATION``,
    ``RA_OF_ASC_NODE``, ``ARG_OF_PERICENTER``, ``MEAN_ANOMALY``,
    ``BSTAR``, ``OBJECT_ID``, ``SEMIMAJOR_AXIS`` (computed), etc.

    Example
    -------
    >>> sats = get_gp_data("stations")
    >>> iss = next(s for s in sats if "ISS" in s["OBJECT_NAME"])
    >>> print(iss["INCLINATION"])
    """
    group = _CATALOG_NAMES.get(catalog.lower(), catalog)
    return get_json(
        _GP_URL,
        params={"GROUP": group, "FORMAT": "JSON"},
        ttl_s=3600,
    )


def get_tles(catalog: str = "active") -> list[dict[str, str]]:
    """
    Fetch TLE data as structured dicts from Celestrak.

    Returns
    -------
    List of dicts with keys ``name``, ``line1``, ``line2``.

    Example
    -------
    >>> tles = get_tles("stations")
    >>> for t in tles:
    ...     print(t["name"], t["line1"][18:32])  # epoch field
    """
    gp_records = get_gp_data(catalog)
    result: list[dict[str, str]] = []
    for rec in gp_records:
        name  = rec.get("OBJECT_NAME", "UNKNOWN").strip()
        line1 = rec.get("TLE_LINE1", "")
        line2 = rec.get("TLE_LINE2", "")
        if line1 and line2:
            result.append({"name": name, "line1": line1, "line2": line2})
    return result


def parse_tle(tle: dict[str, str]) -> dict[str, Any]:
    """
    Parse a TLE dict (``name``, ``line1``, ``line2``) into orbital elements.

    Returns
    -------
    dict with keys:

    * ``name``         — satellite name
    * ``norad_id``     — NORAD catalogue number (int)
    * ``epoch_year``   — two-digit year (int)
    * ``epoch_day``    — day of year + fraction (float)
    * ``bstar``        — drag term (float)
    * ``inclination``  — degrees (float)
    * ``raan``         — Right Ascension of Ascending Node, degrees (float)
    * ``eccentricity`` — unitless 0–1 (float)
    * ``arg_perigee``  — argument of perigee, degrees (float)
    * ``mean_anomaly`` — degrees (float)
    * ``mean_motion``  — revolutions/day (float)
    * ``rev_number``   — revolution number at epoch (int)
    * ``semi_major_axis_km`` — computed from mean motion (float)
    * ``period_min``   — orbital period in minutes (float)
    * ``apogee_km``    — altitude above equatorial radius (float)
    * ``perigee_km``   — altitude above equatorial radius (float)

    Example
    -------
    >>> tles = get_tles("stations")
    >>> elem = parse_tle(tles[0])
    >>> print(f"{elem['name']}: {elem['apogee_km']:.0f} km apogee")
    """
    line1: str = tle["line1"]
    line2: str = tle["line2"]

    # Line 1 fields (TLE column offsets)
    norad_id   = int(line1[2:7])
    epoch_year = int(line1[18:20])
    epoch_day  = float(line1[20:32])
    bstar_raw  = line1[53:61].strip()
    bstar      = _parse_decimal_point(bstar_raw)

    # Line 2 fields
    inclination  = float(line2[8:16])
    raan         = float(line2[17:25])
    ecc_str      = line2[26:33].strip()
    eccentricity = float("0." + ecc_str)
    arg_perigee  = float(line2[34:42])
    mean_anomaly = float(line2[43:51])
    mean_motion  = float(line2[52:63])
    rev_number   = int(line2[63:68])

    # Derived quantities
    n_rad_s = mean_motion * 2.0 * math.pi / 86400.0      # rad/s
    a_km    = (_GM_EARTH / (n_rad_s ** 2)) ** (1.0 / 3.0)  # semi-major axis
    period_min = 1440.0 / mean_motion
    apogee_km  = a_km * (1.0 + eccentricity) - _RE
    perigee_km = a_km * (1.0 - eccentricity) - _RE

    return {
        "name":               tle["name"],
        "norad_id":           norad_id,
        "epoch_year":         epoch_year,
        "epoch_day":          epoch_day,
        "bstar":              bstar,
        "inclination":        inclination,
        "raan":               raan,
        "eccentricity":       eccentricity,
        "arg_perigee":        arg_perigee,
        "mean_anomaly":       mean_anomaly,
        "mean_motion":        mean_motion,
        "rev_number":         rev_number,
        "semi_major_axis_km": a_km,
        "period_min":         period_min,
        "apogee_km":          apogee_km,
        "perigee_km":         perigee_km,
    }


def _parse_decimal_point(s: str) -> float:
    """Parse TLE assumed-decimal notation like ``-11606-4`` → -0.11606e-4."""
    s = s.strip()
    if not s or s in ("-", "+"):
        return 0.0
    # Format: [±]NNNNN±E  (no decimal point, last two chars = exponent)
    try:
        if "e" in s.lower():
            return float(s)
        sign    = -1.0 if s[0] == "-" else 1.0
        body    = s.lstrip("+-")
        mantissa = float("0." + body[:-2]) if len(body) > 2 else 0.0
        exp_part = body[-2:].replace(" ", "0")
        exp_sign = -1 if exp_part[0] == "-" else 1
        exp_val  = int(exp_part[1]) if len(exp_part) > 1 else 0
        return sign * mantissa * (10 ** (exp_sign * exp_val))
    except (ValueError, IndexError):
        return 0.0


def find_satellite(name_fragment: str, catalog: str = "active") -> list[dict[str, Any]]:
    """
    Search the catalog for satellites whose name contains *name_fragment*.

    Returns parsed element dicts for all matches.

    Example
    -------
    >>> results = find_satellite("ISS", "stations")
    >>> print(results[0]["name"], results[0]["period_min"])
    """
    tles = get_tles(catalog)
    frag = name_fragment.upper()
    return [
        parse_tle(t)
        for t in tles
        if frag in t["name"].upper()
    ]
