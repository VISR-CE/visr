#!/usr/bin/env python3
"""Regenerate the site and marked README blocks from shared public facts."""

from __future__ import annotations

import argparse
import ast
import json
from html import escape
from pathlib import Path
import math
import re
import struct
from string import Template
import tomllib

from markdown_it import MarkdownIt
import yaml

ROOT = Path(__file__).resolve().parents[1]
DEFAULTS = {
    "title": "VISR",
    "description": "A native port of Halo: Combat Evolved for Apple Vision Pro, iPhone, iPad, Apple TV and Mac, optimized for Metal and Apple Silicon.",
    "og_title": "VISR: Halo: Combat Evolved on Apple devices",
    "path": "",
}


def deployment_versions(source: str, mac_source: str | None = None) -> dict[str, str]:
    """Read literal build targets without importing the build script's dependencies."""
    tree = ast.parse(source)
    versions = {}
    for node in ast.walk(tree):
        if (isinstance(node, ast.Assign)
                and any(isinstance(target, ast.Tuple)
                        and [getattr(item, "id", None) for item in target.elts] == ["system", "deployment"]
                        for target in node.targets)
                and isinstance(node.value, ast.Subscript)):
            targets = ast.literal_eval(node.value.value)
            versions.update({key: value[1] for key, value in targets.items()})
    # ios_build.py's --mac target is Catalyst; the native AppKit app has its own target.
    if mac_source is None:
        mac_source = (ROOT / "tools/macos_build.py").read_text(encoding="utf-8")
    for node in ast.walk(ast.parse(mac_source)):
        if isinstance(node, ast.Constant) and isinstance(node.value, str):
            match = re.fullmatch(r"-DCMAKE_OSX_DEPLOYMENT_TARGET=(\d+(?:\.\d+)*)", node.value)
            if match:
                versions["macos"] = match[1]
    if set(versions) != {"ios", "tvos", "visionos", "macos"}:
        raise ValueError("Cannot read deployment targets from tools/ios_build.py")
    return versions


def load_facts() -> dict:
    facts = tomllib.loads((ROOT / "docs/features.toml").read_text(encoding="utf-8"))
    versions = deployment_versions((ROOT / "tools/ios_build.py").read_text(encoding="utf-8"))
    for platform in facts["platforms"].values():
        platform["minimum_os"] = versions[platform["deployment"]]
    return facts


def fact_block(kind: str, name: str, facts: dict) -> str:
    if kind == "features" and name in facts["platforms"]:
        versions = deployment_versions((ROOT / "tools/ios_build.py").read_text(encoding="utf-8"))
        values = {f"{system}_minimum": version.removesuffix(".0") for system, version in versions.items()}
        return "\n".join("- " + bullet.format(**values) for bullet in facts["platforms"][name]["bullets"])
    if kind == "shared" and name == "every-device":
        # Site-relative links (the site page `benchmarks.html`) need the site's address in the README.
        return "\n".join(f"- **{item['title']}:** " + item["text"].replace("](benchmarks.html)", f"]({SITE_URL}/benchmarks.html)")
                         for item in facts["every-device"]["items"])
    if kind == "shared":
        key = name.replace("-", "_")
        if key in facts["shared"]:
            return facts["shared"][key]
    raise ValueError(f"Unknown facts block: {kind}: {name}")


SITE_URL = "https://code.steverice.org/visr"
PAGES = ("index",)

NAV_ICONS = {"Get VISR": "download", "Vision Pro": "vision-pro", "iPhone": "iphone", "iPad": "ipad", "Apple TV": "apple-tv",
             "Mac": "mac", "GitHub": "github"}


SECTION_ICONS = {"apple-vision-pro": ["vision-pro"], "iphone": ["iphone"], "ipad": ["ipad"],
                 "apple-tv": ["apple-tv"], "mac": ["mac"]}

# The line above the wordmark, and the part of the tagline set in the accent color (the tagline itself is fixed copy).
KICKER = "Apple Vision Pro · iPhone · iPad · Apple TV · Mac"
HIGHLIGHT = "whole new dimension"
# The visor shape, the same curve as `visorPath` in `docs/site/look.js`, on a 200x100 canvas.
def visor_segments():
    """The bottom has no flats: each side eases straight into the nose notch."""
    w, h, cr, a, cx = 200, 100, 32, 5, 100
    return [((cr, a), (w * .33, 0), (w * .67, 0), (w - cr, a)),
            ((w - cr, a), (w - cr * .4, a * 1.6), (w, .16 * h), (w, .42 * h)),
            ((w, .42 * h), (w, .76 * h), (.89 * w, h), (.75 * w, h)),
            ((.75 * w, h), (.63 * w, h), (.61 * w, .79 * h), (cx, .79 * h)),
            ((cx, .79 * h), (.39 * w, .79 * h), (.37 * w, h), (.25 * w, h)),
            ((.25 * w, h), (.11 * w, h), (0, .76 * h), (0, .42 * h)),
            ((0, .42 * h), (0, .16 * h), (cr * .4, a * 1.6), (cr, a))]


def visor_path():
    start = visor_segments()[0][0]
    curves = " ".join(f"C{b[0]:.1f} {b[1]:.1f} {c[0]:.1f} {c[1]:.1f} {d[0]:.1f} {d[1]:.1f}" for _, b, c, d in visor_segments())
    return f"M{start[0]:.1f} {start[1]:.1f} {curves}Z"


def visor_outline_points(per_segment=8):
    """Points around the visor, centered on the origin."""
    points = []
    for p0, p1, p2, p3 in visor_segments():
        for step in range(per_segment):
            t = step / per_segment
            u = 1 - t
            points.append(tuple(u ** 3 * p0[i] + 3 * u * u * t * p1[i] + 3 * u * t * t * p2[i] + t ** 3 * p3[i] for i in (0, 1)))
    return [(x - 100, y - 50) for x, y in points]


# The hero's tilt. `.hero-media .visor-face` applies the same transform, so the wireframe stays aligned with the glass.
WIRE_PERSPECTIVE, WIRE_YAW, WIRE_PITCH = 440, -32, -10


def wire_project(x, y, z):
    yaw, pitch = math.radians(WIRE_YAW), math.radians(WIRE_PITCH)
    x, z = x * math.cos(yaw) + z * math.sin(yaw), -x * math.sin(yaw) + z * math.cos(yaw)
    y, z = y * math.cos(pitch) - z * math.sin(pitch), y * math.sin(pitch) + z * math.cos(pitch)
    scale = WIRE_PERSPECTIVE / (WIRE_PERSPECTIVE - z)
    return f"{x * scale:.1f} {y * scale:.1f}"


def wire_line(points, close=False):
    return "M" + "L".join(wire_project(*point) for point in points) + ("Z" if close else "")


def headset_wireframe():
    """An abstract headset drawn from the visor shape: shell rings, a seal and a head strap."""
    outline = visor_outline_points()
    rings = [(-8, 1), (-22, 1), (-36, .97), (-50, .9), (-62, .84)]
    paths = [wire_line([(x * s, y * s, z) for x, y in outline], True) for z, s in rings]
    for x, y in outline[::4]:
        paths.append(wire_line([(x, y, 0)] + [(x * s, y * s, z) for z, s in rings]))
    # The strap leaves the sides of the seal and loops around the back of the head.
    loop = [(84 * math.cos(math.radians(a)), -62 - 95 * math.sin(math.radians(a))) for a in range(0, 181, 12)]
    for height in (-15, 0, 15):
        paths.append(wire_line([(x, height, z) for x, z in loop]))
    for x, z in loop[::2]:
        paths.append(wire_line([(x, -15, z), (x, 15, z)]))
    return ('<svg class="visor-wire" viewBox="-200 -100 400 200" aria-hidden="true"><path d="' + "".join(paths) + '"/></svg>')


def visor_slot(slot):
    """Media in the visor shape, under glass, with the gold rim."""
    path = visor_path()
    return (f'<div class="visor-face">{slot}<div class="visor-glass" aria-hidden="true"><svg viewBox="0 0 200 100" preserveAspectRatio="none">'
            f'<path d="{path}"/></svg></div><svg class="visor-rim" viewBox="0 0 200 100" preserveAspectRatio="none" aria-hidden="true">'
            f'<path d="{path}"/></svg></div>')


# `.visor-face` media points its `clip-path` at this, in objectBoundingBox units.
HERO_CLIP = ('<svg class="clip-defs" width="0" height="0" aria-hidden="true"><defs>'
             '<clipPath id="vp-clip" clipPathUnits="objectBoundingBox"><path transform="scale(.005 .01)" '
             f'd="{visor_path()}"/></clipPath></defs></svg>')
# The hero video's stand-in while it loads: the game's loading screen as a 6-column sprite sheet of 36 frames at 30 per second (`visor.js`).
HERO_LOADING = ("loading-hero.webp", 6, 36, 30)
MEDIA_ANNOTATION = re.compile(r"\{(?:device=(iphone|ipad|tv|mac|vision)|(hero))(?: video=([\w-]+))?\}")
# Clips live in the `visr-media` R2 bucket: a slot's `video=NAME` names `NAME-hevc.mp4`, `NAME-web.mp4` (H.264) and `NAME-poster.jpg`.
CLIP_ASSET_BASE = "https://media.visr.gg/clips/"
# RFC 6381 codec string of each clip's `-hevc.mp4`, read from its `hvcC` box (profile, tier, level, constraint flags).
# Chrome and Firefox reject a bare `hvc1` in a `<source type>`, so each clip names its own encode. Update the entry when
# a clip is re-encoded.
CLIP_HEVC_CODECS = {
    "hero": "hvc1.1.6.L120.90",
    "vision": "hvc1.1.6.L123.90",
    "iphone": "hvc1.1.6.L123.90",
    "ipad": "hvc1.1.6.L123.90",
    "tv": "hvc1.1.6.L123.90",
    "mac": "hvc1.1.6.L150.90",
}


def glyph(name: str) -> str:
    """a platform glyph from docs/site/icons/nav/, inlined so it takes the text color"""
    icon = (ROOT / "docs/site/icons/nav" / f"{name}.svg").read_text(encoding="utf-8").strip()
    return f'<span class="nav-icon">{icon}</span>'


def nav_with_icons(html: str) -> str:
    """prefix each platform link with its glyph"""
    def add_icon(match: re.Match) -> str:
        name = NAV_ICONS.get(match[2])
        return match[0] if name is None else f'{match[1]}{glyph(name)}{match[2]}</a>'
    return re.sub(r'(<a href="[^"]*">)([^<]+)</a>', add_icon, html)


def heading_with_icons(html: str, identifier: str) -> str:
    """prefix a platform section's heading with its glyphs"""
    icons = "".join(glyph(name) for name in SECTION_ICONS.get(identifier, []))
    return html.replace(f'<h2 id="{identifier}">', f'<h2 id="{identifier}">{icons}', 1) if icons else html


INLINE = MarkdownIt("commonmark", {"html": False})
EVERY_DEVICE_MARKER = "VISR-EVERY-DEVICE-COLUMNS"


def every_device_columns(facts: dict) -> str:
    """the every-device items as centered columns, each icon inlined so it takes the theme's color"""
    columns = []
    for item in facts["every-device"]["items"]:
        icon = (ROOT / "docs/site/icons" / f"{item['icon']}.svg").read_text(encoding="utf-8").strip()
        columns.append(f'      <li><span class="pillar-icon">{icon}</span>'
                       f'<h3>{escape(item["title"])}</h3><p>{INLINE.renderInline(item["text"]).strip()}</p></li>')
    return "\n".join(['    <ul class="pillars">', *columns, '    </ul>'])


def media_element(image_html: str, image, video: str | None) -> str:
    """A media slot: a muted, looping `<video>` for the clip `video` on the media host (HEVC first, then H.264) with its
    poster, or the image alone when the slot names no clip. `site.js` plays the clip only while it is on screen and
    motion is allowed; the poster (`data-poster`) is shown only when the clip will not play, so a playing slot stays
    black until its first frame instead of flashing the poster."""
    if not video:
        return image_html
    if video not in CLIP_HEVC_CODECS:
        raise ValueError(f"clip {video!r} has no CLIP_HEVC_CODECS entry; read its codec string from {video}-hevc.mp4")
    base = escape(CLIP_ASSET_BASE + video, quote=True)
    return (f'<video class="media-video" muted loop playsinline preload="none" crossorigin="anonymous" data-poster="{base}-poster.jpg" '
            f'width="{image.attrGet("width")}" height="{image.attrGet("height")}" role="img" '
            f'aria-label="{escape(image.content, quote=True)}">'
            f'<source src="{base}-hevc.mp4" type=\'video/mp4; codecs="{CLIP_HEVC_CODECS[video]}"\'>'
            f'<source src="{base}-web.mp4" type="video/mp4"></video>')


def expand_includes(source: str, facts: dict) -> str:
    return re.sub(r"\{\{\s*([\w-]+):\s*([\w-]+)\s*\}\}",
                  lambda match: fact_block(*match.groups(), facts), source)


def render_readme(source: str, facts: dict) -> str:
    if source.count("<!-- generated:") != source.count("<!-- end generated -->"):
        raise ValueError("Unbalanced generated README markers")
    pattern = r"(<!-- generated: ([\w-]+): ([\w-]+) -->)\n.*?\n?(<!-- end generated -->)"
    return re.sub(pattern, lambda match: (match[1] + "\n" + fact_block(match[2], match[3], facts)
                                          + "\n" + match[4]), source, flags=re.S)


def write_outputs(outputs: dict[Path, str], *, check: bool) -> int:
    stale = [path for path, expected in outputs.items()
             if not path.is_file() or path.read_text(encoding="utf-8") != expected]
    if check:
        for path in stale:
            print(f"Stale generated file: {path.name}; run python3 tools/build_site.py")
        return int(bool(stale))
    for path in stale:
        path.write_text(outputs[path], encoding="utf-8")
    return 0


def front_matter(source: str) -> tuple[dict[str, str], str]:
    metadata = dict(DEFAULTS)
    lines = source.splitlines(keepends=True)
    if lines and lines[0].strip() == "---":
        end = next((i for i in range(1, len(lines)) if lines[i].strip() == "---"), None)
        if end is None:
            raise ValueError("Front matter needs a closing --- line")
        values = yaml.safe_load("".join(lines[1:end])) or {}
        if not isinstance(values, dict) or values.keys() - DEFAULTS.keys():
            raise ValueError("Front matter supports title, description, og_title and path")
        if any(not isinstance(value, str) for value in values.values()):
            raise ValueError("Front matter values must be strings")
        metadata.update(values)
        source = "".join(lines[end + 1:])
    return metadata, source


def image_size(path: Path) -> tuple[int, int]:
    """Read web image dimensions without adding an image decoder to the build."""
    with path.open("rb") as image:
        header = image.read(24)
        if header.startswith(b"\x89PNG\r\n\x1a\n"):
            return struct.unpack(">II", header[16:24])
        if header[:2] == b"\xff\xd8":
            image.seek(2)
            while image.read(1) == b"\xff":
                marker = image.read(1)
                while marker == b"\xff":
                    marker = image.read(1)
                length = int.from_bytes(image.read(2), "big")
                if marker in (b"\xc0", b"\xc1", b"\xc2"):
                    _, height, width = struct.unpack(">BHH", image.read(5))
                    return width, height
                if length < 2:
                    break
                image.seek(length - 2, 1)
    raise ValueError(f"Unsupported site image: {path.name}")


def head_tags(metadata: dict[str, str]) -> dict[str, str]:
    """The icon and social-card tags every site page shares (tools/site_head_icons.html and site_head_social.html),
    filled from the page's description, og_title and path. The benchmarks page generator reads the same files."""
    icons = (ROOT / "tools/site_head_icons.html").read_text(encoding="utf-8").rstrip("\n")
    social = Template((ROOT / "tools/site_head_social.html").read_text(encoding="utf-8")).substitute(
        {key: escape(value, quote=True) for key, value in metadata.items()}).rstrip("\n")
    return {"head_icons": icons, "head_social": social}


LOOK_ASSET_BASE = "https://media.visr.gg/look/"
# Codec string of the scenes' shape-av1.mp4 (Main profile, level 5.1, 8-bit), which look.js tries before shape.mp4.
LOOK_AV1 = "av01.0.13M.08"
LOOK_SCENES = [
    {"folder": "lifeboat", "av1": LOOK_AV1, "label": "Lifeboats over Halo", "yaw": 12},
    {"folder": "b30-battle", "av1": LOOK_AV1, "label": "The battle on the beach of The Silent Cartographer", "yaw": 125, "pitch": 2, "videoBand": 1, "drift": {"yaw": 10, "pitch": 0}},
    {"folder": "b40-battlefield", "av1": LOOK_AV1, "label": "The snowfield on Assault on the Control Room", "yaw": 315, "pitch": 60, "videoBand": 1252 / 2048, "drift": {"yaw": 2, "pitch": 0}},
    {"folder": "a50-grav-lift", "av1": LOOK_AV1, "label": "The gravity lift on The Truth and Reconciliation", "yaw": 176, "pitch": 60, "videoBand": 1348 / 2048, "drift": {"yaw": 0, "pitch": -2, "stop": -15}},
    {"folder": "damnation", "av1": LOOK_AV1, "label": "The waterfalls of Damnation", "yaw": 150, "pitch": -20, "videoBand": 1488 / 2048},
    {"folder": "control-room", "av1": LOOK_AV1, "label": "The Control Room on Assault on the Control Room", "wide": 1.4, "yaw": 85, "pitch": 2, "videoBand": 1820 / 2048, "drift": {"yaw": 6, "pitch": 0}},
    {"folder": "battle-creek", "av1": LOOK_AV1, "label": "Capture the Flag on Battle Creek", "wide": 1.26, "yaw": 290, "pitch": 5, "videoBand": 1, "drift": {"yaw": 11.8033, "pitch": 0}},
    {"folder": "blood-gulch", "av1": LOOK_AV1, "label": "Capture the Flag on Blood Gulch", "yaw": 128, "pitch": 2, "videoBand": 1904 / 2048, "drift": {"yaw": 28.8, "pitch": 0}},
]


CURRENT = ' aria-current="page"'


def site_header(page: str) -> str:
    """The brand and site navigation every page shares (tools/site_header.html); `page` is "home" or "benchmarks", the
    link that gets aria-current. The benchmarks page generator reads the same file."""
    return Template((ROOT / "tools/site_header.html").read_text(encoding="utf-8")).substitute(
        home_current=CURRENT if page == "home" else "", bench_current=CURRENT if page == "benchmarks" else "").rstrip("\n")


def render_site(source: str, template: str | None = None, *, asset_root: Path | None = None) -> str:
    facts = load_facts()
    metadata, source = front_matter(source)
    source = re.sub(r"\{\{\s*shared:\s*every-device\s*\}\}", EVERY_DEVICE_MARKER, source)
    source = expand_includes(source, facts)
    asset_root = (asset_root or ROOT / "docs/site").resolve()
    md = MarkdownIt("commonmark", {"html": False})
    tokens = md.parse(source)
    # Work on top-level blocks so nested paragraphs retain their Markdown layout.
    blocks = []
    start = 0
    depth = 0
    for i, token in enumerate(tokens):
        depth += token.nesting
        if depth == 0:
            blocks.append(tokens[start:i + 1])
            start = i + 1

    def inline(block):
        return block[1].children or []

    def local_image_size(image, required=False):
        path = (asset_root / image.attrGet("src")).resolve()
        if not path.is_relative_to(asset_root) or not path.is_file():
            if required:
                raise ValueError("A device image must be a local site image")
            return
        width, height = image_size(path)
        image.attrSet("width", str(width))
        image.attrSet("height", str(height))

    device_blocks = {}
    video_names = {}
    hero_block = None
    hero_loading = False
    for index, block in enumerate(blocks[3:], 3):
        if block[0].type != "paragraph_open":
            continue
        children = inline(block)
        if not children or children[0].type != "image":
            continue
        annotation = "".join(child.content for child in children[1:]).strip()
        image = children[0]
        if annotation.startswith(("{device=", "{hero")):
            match = MEDIA_ANNOTATION.fullmatch(annotation)
            if not match:
                raise ValueError("Use device=iphone, ipad, tv, mac or vision (or hero), with an optional video=NAME, on a standalone image")
            if match.group(2):
                hero_block = index
            else:
                device_blocks[index] = match.group(1)
            video_names[index] = match.group(3)
            local_image_size(image, required=True)
            if match.group(2):  # the hero is the first thing on screen
                image.attrSet("fetchpriority", "high")
            else:
                image.attrSet("loading", "lazy")
            image.attrSet("decoding", "async")
        elif len(children) == 1:
            local_image_size(image)

    if (len(blocks) < 3 or blocks[0][0].type != "paragraph_open"
            or len(inline(blocks[0])) != 1 or inline(blocks[0])[0].type != "image"
            or blocks[1][0].type != "heading_open" or blocks[1][0].tag != "h1"
            or blocks[1][1].content != "VISR"
            or blocks[2][0].type != "heading_open" or blocks[2][0].tag != "h2"):
        raise ValueError("Start with a logo image, # VISR and a ## tagline")

    image = inline(blocks[0])[0]
    image.attrSet("class", "icon")
    image.attrSet("width", "180")
    image.attrSet("height", "180")
    logo = md.renderer.render([image], md.options, {})
    output = ['  <header class="head">', '    <div class="hero-floor" aria-hidden="true"></div>', f'    {HERO_CLIP}',
              '    <div class="hero-copy">', f'      <p class="kicker"><span class="pulse" aria-hidden="true"></span>{KICKER}</p>',
              '    <div class="brand">', f'      {logo}', '      <h1 class="wordmark">VISR</h1>', '    </div>']
    blocks[2][0].tag = blocks[2][-1].tag = "h1"
    tagline = md.renderer.render(blocks[2], md.options, {}).rstrip()
    output.append(tagline.replace(HIGHLIGHT, f'<span class="accent">{HIGHLIGHT.rsplit(" ", 1)[0]} <span class="stereo">{HIGHLIGHT.rsplit(" ", 1)[1]}</span></span>', 1))
    hero_media = []
    section = False
    copy = []
    media = []

    def finish_section():
        if media:
            output.extend(['    <div class="platform-layout">', '      <div class="platform-copy">',
                           *copy, '      </div>', '      <div class="platform-media">',
                           *media, '      </div>', '    </div>'])
        else:
            output.extend(copy)
        output.append("  </section>")
        copy.clear()
        media.clear()

    identifiers = set()
    head_quote = []
    platforms_open = False

    def close_head():
        """End the hero: the copy column, the media beside it, then the quote strip."""
        output.append("    </div>")
        output.extend(hero_media)
        output.extend(head_quote)
        output.append("  </header>")

    for index, block in enumerate(blocks[3:], 3):
        first = block[0]
        if first.type == "heading_open" and first.tag == "h2":
            if section:
                finish_section()
            else:
                close_head()
            text = md.renderer.renderInlineAsText(inline(block), md.options, {})
            base = re.sub(r"[^\w-]+", "-", text.lower()).strip("-") or "section"
            identifier = base
            suffix = 2
            while identifier in identifiers:
                identifier = f"{base}-{suffix}"
                suffix += 1
            identifiers.add(identifier)
            first.attrSet("id", identifier)
            if identifier in SECTION_ICONS and not platforms_open:
                output.extend(['  <div class="platforms" data-tabs>', '  <h2 class="platforms-title">Play on whichever platform you choose</h2>'])
                platforms_open = True
            elif identifier not in SECTION_ICONS and platforms_open:
                output.append("  </div>")
                platforms_open = False
            section_class = ' class="look-around"' if identifier in ("stand-inside-the-level", "behind-the-visor-like-never-before") else ""
            output.append(f'  <section{section_class} aria-labelledby="{escape(identifier, quote=True)}">')
            section = True
            copy.append(heading_with_icons(md.renderer.render(block, md.options, {}).rstrip(), identifier))
            continue
        elif not section and first.type == "paragraph_open":
            children = inline(block)
            links = [child for child in children if child.type == "link_open"]
            if len(links) > 1 and all(child.attrGet("href").startswith(("#", "https://github.com/")) for child in links):
                for child in children:
                    if child.type == "text" and child.content.strip() == "•":
                        child.content = " "
                output.extend(['    <nav class="platform-nav" aria-label="Platforms">',
                               nav_with_icons(md.renderer.render(children, md.options, {}).rstrip()), '    </nav>'])
                continue
        elif not section and first.type == "blockquote_open":
            head_quote.append(md.renderer.render(block, md.options, {}).rstrip())
            continue
        if not section and first.type == "paragraph_open":
            children = inline(block)
            lone_link = (children and children[0].type == "link_open"
                         and children[-1].type == "link_close"
                         and sum(child.type == "link_open" for child in children) == 1)
            first.attrSet("class", "links" if lone_link else "lede")
        if index == hero_block:
            if section:
                raise ValueError("Place the hero image before the first section")
            token = inline(block)[0]
            slot = media_element(md.renderer.render([token], md.options, {}), token, video_names[index])
            if slot.startswith("<video") and (asset_root / HERO_LOADING[0]).is_file():
                slot = slot.replace("<video ", f'<video data-loading="{" ".join(map(str, HERO_LOADING))}" ', 1)
                hero_loading = True
            hero_media.append(f'    <figure class="hero-media">{headset_wireframe()}{visor_slot(slot)}</figure>')
        elif index in device_blocks:
            if not section:
                raise ValueError("Place a device image inside a section")
            token = inline(block)[0]
            image = media_element(md.renderer.render([token], md.options, {}), token, video_names[index])
            if device_blocks[index] == "vision":
                image = visor_slot(image)
            media.append(f'        <figure class="device device-{device_blocks[index]}">{image}</figure>')
        else:
            (copy if section else output).append(md.renderer.render(block, md.options, {}).rstrip())
    if section:
        finish_section()
    else:
        close_head()
    if platforms_open:
        output.append("  </div>")
    if template is None:
        template = (ROOT / "tools/site_template.html").read_text(encoding="utf-8")
    page = Template(template).substitute(
        **head_tags(metadata),
        site_header=site_header("home"),
        **{key: escape(value, quote=True) for key, value in metadata.items()},
        content="\n".join(output).replace(f"<p>{EVERY_DEVICE_MARKER}</p>", every_device_columns(facts)),
        legal=md.render(facts["shared"]["legal"]).rstrip(),
    )
    if hero_loading:
        page = page.replace("</head>", f'<link rel="preload" as="image" href="{HERO_LOADING[0]}" type="image/webp" fetchpriority="high">\n</head>', 1)
    if "{{ look-around }}" in source:
        widget = Template((ROOT / "tools/site_look.html").read_text(encoding="utf-8").rstrip()).substitute(
            scenes=escape(json.dumps(LOOK_SCENES), quote=True),
            asset_base=escape(LOOK_ASSET_BASE, quote=True),
            folder=escape("look/" + LOOK_SCENES[0]["folder"], quote=True),
            label=escape(LOOK_SCENES[0]["label"], quote=True))
        page = page.replace("<p>{{ look-around }}</p>", widget)
        page = page.replace("</head>", '<link rel="stylesheet" href="look.css">\n</head>')
        page = page.replace('<script src="site.js" defer></script>', '<script src="site.js" defer></script>\n<script src="look.js" defer></script>\n<script src="visor.js" defer></script>')
    return page


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="report stale generated files without writing")
    args = parser.parse_args()
    site = ROOT / "docs/site"
    readme = ROOT / "README.md"
    return write_outputs({
        **{site / f"{page}.html": render_site((site / f"{page}.md").read_text(encoding="utf-8")) for page in PAGES},
        readme: render_readme(readme.read_text(encoding="utf-8"), load_facts()),
    }, check=args.check)


if __name__ == "__main__":
    raise SystemExit(main())
