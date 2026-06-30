"""
openorbit.data.nasa_api
========================
Wrappers for public NASA APIs used by OpenOrbit.

APIs covered
------------
* **APOD** — Astronomy Picture of the Day
* **JPL Horizons** — Solar-system ephemeris (positions + velocities)
* **NASA Earth Imagery** — Landsat/Sentinel satellite imagery tiles
* **EPIC** — Earth Polychromatic Imaging Camera (full-disk Earth images)
* **NeoWs** — Near-Earth Object web service

Most endpoints accept ``DEMO_KEY`` as the API key (30 req/hour limit).
Set the environment variable ``NASA_API_KEY`` to a personal key for higher limits.

    https://api.nasa.gov/  — register for free
"""

from __future__ import annotations

import os
from datetime import date, datetime, timezone
from typing import Any

from .space_client import get_bytes, get_json

_NASA_KEY = os.environ.get("NASA_API_KEY", "DEMO_KEY")
_NASA_BASE = "https://api.nasa.gov"
_HORIZONS  = "https://ssd.jpl.nasa.gov/api/horizons.api"


# ── APOD ─────────────────────────────────────────────────────────────────────

def get_apod(query_date: date | None = None) -> dict[str, Any]:
    """
    Fetch the Astronomy Picture of the Day.

    Parameters
    ----------
    query_date : Date to fetch (defaults to today).

    Returns
    -------
    dict with keys: ``title``, ``explanation``, ``url``, ``media_type``,
    ``date``, ``hdurl`` (if available).

    Example
    -------
    >>> info = get_apod()
    >>> print(info["title"])
    """
    params: dict[str, str] = {"api_key": _NASA_KEY}
    if query_date:
        params["date"] = query_date.isoformat()
    return get_json(f"{_NASA_BASE}/planetary/apod", params=params, ttl_s=3600)


def download_apod_image(query_date: date | None = None) -> bytes:
    """Download the APOD image (JPEG/PNG) as raw bytes."""
    info = get_apod(query_date)
    if info.get("media_type") != "image":
        raise ValueError(f"APOD for {query_date} is not an image: {info.get('media_type')}")
    url = info.get("hdurl") or info["url"]
    suffix = ".jpg" if url.endswith(".jpg") else ".png"
    return get_bytes(url, ttl_s=86400 * 7, suffix=suffix)


# ── JPL Horizons Ephemeris ────────────────────────────────────────────────────

def get_body_state(
    target: str,
    observer: str = "500@399",  # geocenter
    start_time: datetime | None = None,
    stop_time: datetime | None = None,
    step: str = "1d",
) -> dict[str, Any]:
    """
    Query the JPL Horizons REST API for state vectors.

    Parameters
    ----------
    target    : Horizons target body string (e.g. ``"499"`` for Mars,
                ``"Ceres"`` for dwarf planets, ``"2023 BU"`` for small bodies).
    observer  : Observer location code (default: geocentre ``"500@399"``).
    start_time: Epoch start (defaults to now − 1 day).
    stop_time : Epoch end   (defaults to now).
    step      : Time step   (e.g. ``"1d"``, ``"1h"``, ``"10m"``).

    Returns
    -------
    Raw Horizons JSON response (``signature``, ``result`` text fields).

    Example
    -------
    >>> r = get_body_state("499")   # Mars
    >>> print(r["result"][:500])
    """
    if start_time is None:
        start_time = datetime.now(tz=timezone.utc).replace(
            hour=0, minute=0, second=0, microsecond=0
        )
    if stop_time is None:
        stop_time = start_time

    fmt = "%Y-%m-%d %H:%M"
    params = {
        "format":       "json",
        "COMMAND":      f"'{target}'",
        "OBJ_DATA":     "YES",
        "MAKE_EPHEM":   "YES",
        "EPHEM_TYPE":   "VECTORS",
        "CENTER":       f"'{observer}'",
        "START_TIME":   f"'{start_time.strftime(fmt)}'",
        "STOP_TIME":    f"'{stop_time.strftime(fmt)}'",
        "STEP_SIZE":    f"'{step}'",
        "VEC_TABLE":    "2",   # position + velocity
        "REF_PLANE":    "ECLIPTIC",
        "REF_SYSTEM":   "J2000",
        "VECT_CORR":    "NONE",
        "OUT_UNITS":    "KM-S",  # km and km/s
        "VEC_LABELS":   "YES",
        "VEC_DELTA_T":  "NO",
        "CSV_FORMAT":   "YES",
    }
    return get_json(_HORIZONS, params=params, ttl_s=3600)


def parse_horizons_vectors(horizons_result: dict) -> list[dict[str, float]]:
    """
    Parse JPL Horizons CSV VECTORS output into a list of state records.

    Each record: ``{"t_jd": float, "x": float, "y": float, "z": float,
                    "vx": float, "vy": float, "vz": float}``
    All units in km / (km/s).
    """
    text: str = horizons_result.get("result", "")
    records: list[dict[str, float]] = []

    in_data = False
    for line in text.splitlines():
        if line.strip() == "$$SOE":
            in_data = True
            continue
        if line.strip() == "$$EOE":
            break
        if not in_data:
            continue
        # CSV lines: JDTDB, CalendarDate, X, Y, Z, VX, VY, VZ, ...
        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 8:
            continue
        try:
            records.append({
                "t_jd": float(parts[0]),
                "x":    float(parts[2]),
                "y":    float(parts[3]),
                "z":    float(parts[4]),
                "vx":   float(parts[5]),
                "vy":   float(parts[6]),
                "vz":   float(parts[7]),
            })
        except (ValueError, IndexError):
            continue
    return records


# ── Earth Imagery ─────────────────────────────────────────────────────────────

def get_earth_image(
    lat: float,
    lon: float,
    dim: float = 0.025,
    imagery_date: date | None = None,
) -> bytes:
    """
    Download a Landsat 8 satellite image centered on (lat, lon).

    Parameters
    ----------
    lat  : Latitude  [deg] (−90 to +90)
    lon  : Longitude [deg] (−180 to +180)
    dim  : Width/height of the bounding box in degrees (default 0.025 ≈ 2.5 km)
    imagery_date : Approximate date of imagery (NASA picks the nearest pass).

    Returns
    -------
    JPEG image as raw bytes.

    Example
    -------
    >>> img = get_earth_image(28.573, -80.648)  # Kennedy Space Center
    >>> open("ksc.jpg", "wb").write(img)
    """
    params: dict[str, str] = {
        "lat":     str(lat),
        "lon":     str(lon),
        "dim":     str(dim),
        "api_key": _NASA_KEY,
    }
    if imagery_date:
        params["date"] = imagery_date.isoformat()

    return get_bytes(
        f"{_NASA_BASE}/planetary/earth/imagery",
        params=params,
        ttl_s=86400 * 30,
        suffix=".jpg",
    )


# ── EPIC ─────────────────────────────────────────────────────────────────────

def get_epic_images(collection: str = "natural") -> list[dict[str, Any]]:
    """
    List EPIC full-disk Earth images from the most recent available day.

    Parameters
    ----------
    collection : ``"natural"`` (true colour) or ``"enhanced"`` (IR false colour)

    Returns
    -------
    List of image metadata dicts, each with: ``identifier``, ``caption``,
    ``centroid_coordinates``, ``date``.
    """
    return get_json(
        f"{_NASA_BASE}/EPIC/api/{collection}",
        params={"api_key": _NASA_KEY},
        ttl_s=3600,
    )


def download_epic_image(identifier: str, collection: str = "natural") -> bytes:
    """
    Download one EPIC full-disk Earth PNG image by its identifier.

    Parameters
    ----------
    identifier : Image identifier string from ``get_epic_images()``
    collection : ``"natural"`` or ``"enhanced"``
    """
    date_str  = identifier[:10].replace("-", "/")  # YYYY/MM/DD
    image_url = (
        f"{_NASA_BASE}/EPIC/archive/{collection}/{date_str}/png/{identifier}.png"
        f"?api_key={_NASA_KEY}"
    )
    return get_bytes(image_url, ttl_s=86400 * 30, suffix=".png")


# ── Near Earth Objects ────────────────────────────────────────────────────────

def get_neo_feed(
    start_date: date | None = None,
    end_date:   date | None = None,
) -> dict[str, Any]:
    """
    Fetch NeoWs NEO (near-Earth object) close-approach data for a 7-day window.

    Returns
    -------
    dict with keys: ``element_count``, ``near_earth_objects`` (keyed by date).
    """
    if start_date is None:
        start_date = date.today()
    if end_date is None:
        end_date = start_date
    return get_json(
        f"{_NASA_BASE}/neo/rest/v1/feed",
        params={
            "start_date": start_date.isoformat(),
            "end_date":   end_date.isoformat(),
            "api_key":    _NASA_KEY,
        },
        ttl_s=3600,
    )
