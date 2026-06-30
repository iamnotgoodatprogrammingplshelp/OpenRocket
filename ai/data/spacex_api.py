"""
openorbit.data.spacex_api
==========================
Wrappers for the public r-spacex REST API (v4).

No API key required.

    https://github.com/r-spacex/SpaceX-API

Endpoints covered
-----------------
* Launches  — upcoming, latest, past, all
* Rockets   — Falcon 9, Falcon Heavy, Starship, etc.
* Launchpads & Landpads
* Crew members
* Payloads
* Capsules (Dragon)
* Company info
"""

from __future__ import annotations

from typing import Any

from .space_client import get_json

_BASE = "https://api.spacexdata.com/v4"


# ── Launches ──────────────────────────────────────────────────────────────────

def get_latest_launch() -> dict[str, Any]:
    """
    Return the most recent launch record.

    Example
    -------
    >>> launch = get_latest_launch()
    >>> print(launch["name"], launch["date_utc"])
    """
    return get_json(f"{_BASE}/launches/latest", ttl_s=300)


def get_next_launch() -> dict[str, Any]:
    """
    Return the next scheduled launch record.

    Example
    -------
    >>> nxt = get_next_launch()
    >>> print(nxt["name"], nxt["date_utc"])
    """
    return get_json(f"{_BASE}/launches/next", ttl_s=300)


def get_upcoming_launches() -> list[dict[str, Any]]:
    """
    Return all upcoming launch records (chronological order).

    Example
    -------
    >>> upcoming = get_upcoming_launches()
    >>> for lnch in upcoming[:3]:
    ...     print(lnch["name"], lnch["date_utc"])
    """
    return get_json(f"{_BASE}/launches/upcoming", ttl_s=600)


def get_past_launches(limit: int | None = None) -> list[dict[str, Any]]:
    """
    Return past launch records (most-recent first).

    Parameters
    ----------
    limit : If given, return at most this many records.

    Example
    -------
    >>> past = get_past_launches(10)
    >>> print(len(past), "recent launches")
    """
    data: list[dict[str, Any]] = get_json(f"{_BASE}/launches/past", ttl_s=3600)
    data.sort(key=lambda x: x.get("date_unix", 0), reverse=True)
    return data[:limit] if limit is not None else data


def get_launch(launch_id: str) -> dict[str, Any]:
    """
    Return a specific launch by its SpaceX API ID.

    Example
    -------
    >>> launch = get_launch("5eb87d47ffd86e000604b38a")  # CRS-20
    >>> print(launch["name"])
    """
    return get_json(f"{_BASE}/launches/{launch_id}", ttl_s=3600)


# ── Rockets ───────────────────────────────────────────────────────────────────

def get_rockets() -> list[dict[str, Any]]:
    """
    Return all rocket specifications.

    Useful fields per rocket: ``name``, ``type``, ``active``, ``stages``,
    ``boosters``, ``cost_per_launch``, ``mass`` (kg), ``first_flight``,
    ``description``, ``engines`` (isp, thrust_sea_level, thrust_vacuum,
    number, type, version, layout).

    Example
    -------
    >>> for r in get_rockets():
    ...     print(r["name"], r["engines"]["number"], "engines")
    """
    return get_json(f"{_BASE}/rockets", ttl_s=86400)


def get_rocket(rocket_id: str) -> dict[str, Any]:
    """
    Return a specific rocket by ID.

    Common IDs: ``"5e9d0d95eda69973a809d1ec"`` (Falcon 9),
    ``"5e9d0d95eda69974db09d1ed"`` (Falcon Heavy),
    ``"5e9d0d96eda699382d09d1ee"`` (Starship).

    Example
    -------
    >>> f9 = get_rocket("5e9d0d95eda69973a809d1ec")
    >>> print(f9["name"], f9["engines"]["isp"]["vacuum"], "s Isp")
    """
    return get_json(f"{_BASE}/rockets/{rocket_id}", ttl_s=86400)


# ── Launchpads ────────────────────────────────────────────────────────────────

def get_launchpads() -> list[dict[str, Any]]:
    """
    Return all launchpad records.

    Useful fields: ``name``, ``full_name``, ``locality``, ``region``,
    ``latitude``, ``longitude``, ``launch_successes``, ``launch_attempts``,
    ``status`` (active / retired / etc.).

    Example
    -------
    >>> for pad in get_launchpads():
    ...     print(f"{pad['name']:10s}  {pad['latitude']:.2f}°N  {pad['longitude']:.2f}°E")
    """
    return get_json(f"{_BASE}/launchpads", ttl_s=86400)


def get_launchpad(pad_id: str) -> dict[str, Any]:
    """Return a specific launchpad by ID."""
    return get_json(f"{_BASE}/launchpads/{pad_id}", ttl_s=86400)


# ── Landpads ──────────────────────────────────────────────────────────────────

def get_landpads() -> list[dict[str, Any]]:
    """
    Return all landpad (drone ship / LZ) records.

    Useful fields: ``name``, ``full_name``, ``type``, ``locality``, ``region``,
    ``latitude``, ``longitude``, ``landing_successes``, ``landing_attempts``,
    ``status``.

    Example
    -------
    >>> for lp in get_landpads():
    ...     print(lp["full_name"], lp["landing_successes"])
    """
    return get_json(f"{_BASE}/landpads", ttl_s=86400)


def get_landpad(landpad_id: str) -> dict[str, Any]:
    """Return a specific landpad by ID."""
    return get_json(f"{_BASE}/landpads/{landpad_id}", ttl_s=86400)


# ── Payloads ──────────────────────────────────────────────────────────────────

def get_payloads() -> list[dict[str, Any]]:
    """
    Return all payload records.

    Useful fields: ``name``, ``type``, ``reused``, ``launch``,
    ``customers``, ``nationalities``, ``manufacturers``,
    ``mass_kg``, ``orbit``, ``apoapsis_km``, ``periapsis_km``,
    ``inclination_deg``, ``period_min``.

    Example
    -------
    >>> payloads = get_payloads()
    >>> starlink = [p for p in payloads if "Starlink" in (p.get("name") or "")]
    >>> print(len(starlink), "Starlink payloads")
    """
    return get_json(f"{_BASE}/payloads", ttl_s=3600)


def get_payload(payload_id: str) -> dict[str, Any]:
    """Return a specific payload by ID."""
    return get_json(f"{_BASE}/payloads/{payload_id}", ttl_s=3600)


# ── Capsules (Dragon) ─────────────────────────────────────────────────────────

def get_capsules() -> list[dict[str, Any]]:
    """
    Return all Dragon capsule records.

    Useful fields: ``serial``, ``type``, ``status``, ``reuse_count``,
    ``water_landings``, ``land_landings``, ``last_update``.

    Example
    -------
    >>> active = [c for c in get_capsules() if c["status"] == "active"]
    >>> print(len(active), "active Dragon capsules")
    """
    return get_json(f"{_BASE}/capsules", ttl_s=3600)


# ── Crew ──────────────────────────────────────────────────────────────────────

def get_crew() -> list[dict[str, Any]]:
    """
    Return all crew member records.

    Useful fields: ``name``, ``agency``, ``image``, ``wikipedia``,
    ``status``, ``launches``.

    Example
    -------
    >>> active_crew = [c for c in get_crew() if c["status"] == "active"]
    >>> for m in active_crew[:5]:
    ...     print(m["name"], m["agency"])
    """
    return get_json(f"{_BASE}/crew", ttl_s=3600)


# ── Company info ──────────────────────────────────────────────────────────────

def get_company() -> dict[str, Any]:
    """
    Return SpaceX company overview.

    Includes ``name``, ``founder``, ``founded``, ``employees``,
    ``vehicles``, ``launch_sites``, ``test_sites``,
    ``ceo``, ``cto``, ``coo``, ``cto_propulsion``,
    ``valuation``, ``headquarters``, ``summary``.

    Example
    -------
    >>> co = get_company()
    >>> print(co["summary"][:200])
    """
    return get_json(f"{_BASE}/company", ttl_s=86400)


# ── Convenience ───────────────────────────────────────────────────────────────

def get_launch_summary(launch: dict[str, Any]) -> dict[str, Any]:
    """
    Extract the most useful fields from a raw launch record.

    Returns a slimmed-down dict with:
    ``id``, ``name``, ``date_utc``, ``date_unix``, ``success``,
    ``details``, ``rocket``, ``launchpad``, ``payloads``,
    ``cores`` (list of booster serial / landing info).

    Example
    -------
    >>> launch = get_latest_launch()
    >>> summary = get_launch_summary(launch)
    >>> print(summary["name"], "success:", summary["success"])
    """
    cores_raw = launch.get("cores", [])
    cores = [
        {
            "serial":           c.get("core"),
            "flight":           c.get("flight"),
            "reused":           c.get("reused"),
            "landing_attempt":  c.get("landing_attempt"),
            "landing_success":  c.get("landing_success"),
            "landing_type":     c.get("landing_type"),
            "landpad":          c.get("landpad"),
        }
        for c in cores_raw
    ]
    return {
        "id":        launch.get("id"),
        "name":      launch.get("name"),
        "date_utc":  launch.get("date_utc"),
        "date_unix": launch.get("date_unix"),
        "success":   launch.get("success"),
        "details":   launch.get("details"),
        "rocket":    launch.get("rocket"),
        "launchpad": launch.get("launchpad"),
        "payloads":  launch.get("payloads", []),
        "cores":     cores,
        "links": {
            "webcast":    launch.get("links", {}).get("webcast"),
            "wikipedia":  launch.get("links", {}).get("wikipedia"),
            "article":    launch.get("links", {}).get("article"),
            "patch_small": launch.get("links", {}).get("patch", {}).get("small"),
        },
    }
