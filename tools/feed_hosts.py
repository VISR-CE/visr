"""The two hosts that serve VISR's signed data-pack feeds.

Builds from 0.7.0 read their feeds from `FEEDS_BASE`. Earlier builds read the same files from `LEGACY_BASE`, which
keeps serving them directly (released apps refuse redirects). Both are static GitHub Pages sites deployed from the
same commit by `.github/workflows/pages.yml`, so a feed has the same path under `/packs/` on each; only the legacy
site carries a `/visr` prefix. See docs/data-packs.md, "Feed hosts".
"""

from __future__ import annotations

import urllib.parse

FEEDS_HOST = "feeds.visr.gg"
FEEDS_BASE = "https://" + FEEDS_HOST
LEGACY_HOST = "code.steverice.org"
LEGACY_BASE = "https://" + LEGACY_HOST + "/visr"
PACKS = "/packs/"


def legacy_url(url: str) -> str | None:
    """The legacy-host URL that must serve the same bytes as a feeds-host URL, or None for any other URL."""
    parts = urllib.parse.urlsplit(url)
    if parts.scheme != "https" or (parts.hostname or "").rstrip(".").lower() != FEEDS_HOST:
        return None
    if parts.port not in (None, 443) or parts.username or parts.password or parts.fragment:
        return None
    if not parts.path.startswith(PACKS):
        return None
    return LEGACY_BASE + parts.path + ("?" + parts.query if parts.query else "")
