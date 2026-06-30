"""
openorbit.data.space_client
============================
Thin HTTP utility layer used by all space-data API modules.

Uses only the stdlib (urllib) — no extra dependencies required.
Results are cached on disk under ~/.cache/openorbit/ so that repeated
calls don't hit external servers.
"""

from __future__ import annotations

import hashlib
import json
import os
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any

# ── Cache directory ───────────────────────────────────────────────────────────

_CACHE_DIR = Path(os.environ.get("OPENORBIT_CACHE", Path.home() / ".cache" / "openorbit"))
_CACHE_DIR.mkdir(parents=True, exist_ok=True)


def _cache_path(url: str) -> Path:
    key = hashlib.sha1(url.encode()).hexdigest()
    return _CACHE_DIR / f"{key}.json"


def get_json(
    url: str,
    params: dict[str, str] | None = None,
    headers: dict[str, str] | None = None,
    ttl_s: float = 3600.0,
    timeout_s: float = 10.0,
) -> Any:
    """
    Fetch *url* as JSON, caching on disk for *ttl_s* seconds.

    Parameters
    ----------
    url       : Full URL (query-string params handled separately).
    params    : Optional URL query parameters (appended as ?k=v&...).
    headers   : Optional HTTP headers (e.g. ``{"X-Api-Key": "..."}``.
    ttl_s     : Cache time-to-live in seconds (default 1 hour).
    timeout_s : Network timeout (default 10 s).

    Returns
    -------
    Parsed JSON (dict, list, etc.)

    Raises
    ------
    urllib.error.URLError  on network failure when cache is empty/expired.
    json.JSONDecodeError   if the response is not valid JSON.
    """
    if params:
        url = url + "?" + urllib.parse.urlencode(params)

    cache = _cache_path(url)
    if cache.exists():
        age = time.time() - cache.stat().st_mtime
        if age < ttl_s:
            return json.loads(cache.read_text())

    req = urllib.request.Request(url, headers=headers or {})
    with urllib.request.urlopen(req, timeout=timeout_s) as resp:
        raw = resp.read().decode()

    data = json.loads(raw)
    cache.write_text(json.dumps(data))
    return data


def get_bytes(
    url: str,
    params: dict[str, str] | None = None,
    headers: dict[str, str] | None = None,
    ttl_s: float = 86400.0,
    timeout_s: float = 15.0,
    suffix: str = ".bin",
) -> bytes:
    """
    Fetch binary data (images, TLE text, etc.) with disk caching.

    Returns raw bytes.
    """
    if params:
        url = url + "?" + urllib.parse.urlencode(params)

    key = hashlib.sha1(url.encode()).hexdigest()
    cache = _CACHE_DIR / f"{key}{suffix}"

    if cache.exists():
        age = time.time() - cache.stat().st_mtime
        if age < ttl_s:
            return cache.read_bytes()

    req = urllib.request.Request(url, headers=headers or {})
    with urllib.request.urlopen(req, timeout=timeout_s) as resp:
        data = resp.read()

    cache.write_bytes(data)
    return data


def clear_cache() -> None:
    """Delete all cached responses."""
    for f in _CACHE_DIR.glob("*"):
        f.unlink(missing_ok=True)
