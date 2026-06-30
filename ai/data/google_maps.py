"""
openorbit.data.google_maps
===========================
Google Maps Static API wrapper for Earth map overlays.

Requires a Google Maps Platform API key with the **Maps Static API** enabled.
Set the environment variable ``GOOGLE_MAPS_API_KEY`` before use.

    https://developers.google.com/maps/documentation/maps-static/overview

If no key is set the module raises ``MissingApiKeyError`` (a subclass of
``RuntimeError``) so callers can fall back gracefully to the NASA Earth
Imagery API or a simple blue-marble texture.

Map types
---------
* ``roadmap``   — standard road map (default)
* ``satellite`` — Landsat/DigitalGlobe satellite imagery
* ``hybrid``    — satellite + road overlay
* ``terrain``   — terrain relief map
"""

from __future__ import annotations

import os
from typing import Literal

from .space_client import get_bytes

_GMAPS_KEY  = os.environ.get("GOOGLE_MAPS_API_KEY", "")
_STATIC_URL = "https://maps.googleapis.com/maps/api/staticmap"

MapType = Literal["roadmap", "satellite", "hybrid", "terrain"]


class MissingApiKeyError(RuntimeError):
    """Raised when ``GOOGLE_MAPS_API_KEY`` is not set."""


def _require_key() -> str:
    if not _GMAPS_KEY:
        raise MissingApiKeyError(
            "GOOGLE_MAPS_API_KEY environment variable is not set. "
            "Get a free key at https://console.cloud.google.com/"
        )
    return _GMAPS_KEY


# ── Core tile fetch ───────────────────────────────────────────────────────────

def get_map_image(
    lat: float,
    lon: float,
    zoom: int = 3,
    width: int = 640,
    height: int = 640,
    map_type: MapType = "satellite",
    scale: int = 1,
) -> bytes:
    """
    Fetch a static map image centred on (lat, lon).

    Parameters
    ----------
    lat      : Centre latitude  [deg]  (−90 to +90)
    lon      : Centre longitude [deg]  (−180 to +180)
    zoom     : Google Maps zoom level  (0=whole Earth, 21=building level).
               Good values for orbital view: 2–5.
    width    : Image width  in pixels (max 640 with free tier, 2048 with premium)
    height   : Image height in pixels
    map_type : One of ``roadmap``, ``satellite``, ``hybrid``, ``terrain``.
    scale    : 1 (standard) or 2 (retina/HiDPI, doubles pixel count).

    Returns
    -------
    PNG image as raw bytes.

    Raises
    ------
    MissingApiKeyError if ``GOOGLE_MAPS_API_KEY`` is not set.

    Example
    -------
    >>> img = get_map_image(28.573, -80.648, zoom=10, map_type="hybrid")
    >>> open("ksc_map.png", "wb").write(img)
    """
    key = _require_key()
    params: dict[str, str] = {
        "center":   f"{lat},{lon}",
        "zoom":     str(zoom),
        "size":     f"{width}x{height}",
        "maptype":  map_type,
        "scale":    str(scale),
        "key":      key,
    }
    return get_bytes(_STATIC_URL, params=params, ttl_s=86400 * 30, suffix=".png")


def get_orbital_overview(
    lat: float = 0.0,
    lon: float = 0.0,
    zoom: int = 2,
) -> bytes:
    """
    Convenience wrapper: 640×640 satellite overview for the orbital map panel.

    Suitable as a background texture for the OrbitalMap ground-track view.
    """
    return get_map_image(lat, lon, zoom=zoom, map_type="satellite")


# ── Path / marker overlays ────────────────────────────────────────────────────

def get_ground_track_image(
    track_points: list[tuple[float, float]],
    width: int = 640,
    height: int = 640,
    zoom: int = 1,
    path_color: str = "0xff4444ff",
    path_weight: int = 2,
) -> bytes:
    """
    Render a satellite ground track as a polyline on a satellite map.

    Parameters
    ----------
    track_points : List of (lat, lon) tuples defining the ground track.
    width, height: Image dimensions in pixels.
    zoom         : Map zoom (1–2 recommended for full-Earth ground tracks).
    path_color   : ARGB hex colour string for the path (Google Maps format).
    path_weight  : Line width in pixels.

    Returns
    -------
    PNG image as raw bytes.

    Raises
    ------
    MissingApiKeyError if ``GOOGLE_MAPS_API_KEY`` is not set.
    ValueError if ``track_points`` is empty.

    Example
    -------
    >>> points = [(lat, lon) for lat, lon in iss_ground_track]
    >>> img = get_ground_track_image(points)
    """
    if not track_points:
        raise ValueError("track_points must not be empty")

    key = _require_key()

    # Encode polyline path
    path_encoded = "|".join(f"{lat},{lon}" for lat, lon in track_points[:500])
    path_param = f"color:{path_color}|weight:{path_weight}|{path_encoded}"

    # Centre on midpoint of track
    mid = track_points[len(track_points) // 2]

    params: dict[str, str] = {
        "center":  f"{mid[0]},{mid[1]}",
        "zoom":    str(zoom),
        "size":    f"{width}x{height}",
        "maptype": "hybrid",
        "path":    path_param,
        "key":     key,
    }
    return get_bytes(_STATIC_URL, params=params, ttl_s=3600, suffix=".png")


def get_launch_site_map(
    lat: float,
    lon: float,
    site_name: str = "",
    zoom: int = 12,
) -> bytes:
    """
    Satellite image of a launch site with an optional marker.

    Parameters
    ----------
    lat, lon   : Launch site coordinates.
    site_name  : Label for the map marker (empty → no label).
    zoom       : Map zoom level (8–14 works well for launch sites).

    Returns
    -------
    PNG image as raw bytes.

    Example
    -------
    >>> img = get_launch_site_map(28.573, -80.648, "KSC", zoom=11)
    """
    key = _require_key()
    label = site_name[:1].upper() if site_name else "L"
    params: dict[str, str] = {
        "center":  f"{lat},{lon}",
        "zoom":    str(zoom),
        "size":    "640x640",
        "maptype": "hybrid",
        "markers": f"color:red|label:{label}|{lat},{lon}",
        "key":     key,
    }
    return get_bytes(_STATIC_URL, params=params, ttl_s=86400 * 7, suffix=".png")


# ── Key availability check ────────────────────────────────────────────────────

def is_available() -> bool:
    """Return True if a Google Maps API key is configured."""
    return bool(_GMAPS_KEY)
