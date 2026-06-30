"""openorbit.data — Space data API clients with disk caching."""

from .space_client import get_json, get_bytes, clear_cache
from .nasa_api import (
    get_apod,
    download_apod_image,
    get_body_state,
    parse_horizons_vectors,
    get_earth_image,
    get_epic_images,
    download_epic_image,
    get_neo_feed,
)
from .tle_fetcher import (
    get_gp_data,
    get_tles,
    parse_tle,
    find_satellite,
)
from .spacex_api import (
    get_latest_launch,
    get_next_launch,
    get_upcoming_launches,
    get_past_launches,
    get_launch,
    get_rockets,
    get_rocket,
    get_launchpads,
    get_landpads,
    get_crew,
    get_company,
    get_launch_summary,
)
from .google_maps import (
    get_map_image,
    get_orbital_overview,
    get_ground_track_image,
    get_launch_site_map,
    is_available as google_maps_available,
    MissingApiKeyError,
)

__all__ = [
    # space_client
    "get_json",
    "get_bytes",
    "clear_cache",
    # nasa_api
    "get_apod",
    "download_apod_image",
    "get_body_state",
    "parse_horizons_vectors",
    "get_earth_image",
    "get_epic_images",
    "download_epic_image",
    "get_neo_feed",
    # tle_fetcher
    "get_gp_data",
    "get_tles",
    "parse_tle",
    "find_satellite",
    # spacex_api
    "get_latest_launch",
    "get_next_launch",
    "get_upcoming_launches",
    "get_past_launches",
    "get_launch",
    "get_rockets",
    "get_rocket",
    "get_launchpads",
    "get_landpads",
    "get_crew",
    "get_company",
    "get_launch_summary",
    # google_maps
    "get_map_image",
    "get_orbital_overview",
    "get_ground_track_image",
    "get_launch_site_map",
    "google_maps_available",
    "MissingApiKeyError",
]
