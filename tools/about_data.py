"""What the About screen (Settings > ABOUT) says: the website, the credit line and the licenses of the code the Apple
builds contain. tools/port_settings.py writes the screen from this; tools/test_about_screen.py checks each entry
against its license file and against NOTICE.md.

Each license entry: the name shown, the license shown, the copyright line (a short one: the screen shows three
lines), the license file in this repository that holds it (None: downloaded at build time by tools/ios_build.py,
which copies its notice into the app's Licenses folder), and the word the file must contain, which the test looks
for (the copyright holder).
"""

# the site's public address (README.md, docs/site): shown without the scheme
WEBSITE = "visr.gg"
WEBSITE_URL = "https://visr.gg/"
# (the row's label is the address, in the capitals of the other labels; it opens the site except on the Apple TV)
WEBSITE_LABEL = WEBSITE.upper()
WEBSITE_HELP = "Visit VISR on the web."

CREDIT = ("VISR builds on the community's work decompiling Halo: Combat Evolved and porting it: "
          "punpckhdq, bnunu, OpenCE, NicholasDominici and pfista.")

TAGLINE = "An unofficial port of Halo: Combat Evolved"
BUILD_HELP = "The build number of this app."

# the SEND LOG row (three lines of the help at most): it uploads the game's log to iCloud and shows a code
SEND_LOG_HELP = "Sends this game's log to the VISR team through iCloud and shows a code to give them."

LICENSES_HELP = "The full license texts are in the app's Licenses folder and at github.com/steverice/visr."

# (name, license, copyright, file, holder)
LICENSES = [
    ("VISR", "APACHE-2.0", "Licensed under the Apache License, Version 2.0.", "LICENSE", "Apache License"),
    ("ORIGIN PROJECTS", "CC0 1.0",
     "punpckhdq/halo, bnunu/halo-1, OpenCE, NicholasDominici/halo-ce-ios and pfista/halo-og. Public domain "
     "dedication.", "LICENSES/CC0-1.0.txt", "CC0 1.0"),
    ("SDL", "ZLIB", "Copyright (C) 1997-2026 Sam Lantinga.", None, "Sam Lantinga"),
    ("MUSL", "MIT", "Copyright (c) 2005-2020 Rich Felker, et al.", None, "Rich Felker"),
    ("MBED TLS", "APACHE-2.0", "Copyright The Mbed TLS Contributors.", "port/third_party/mbedtls/LICENSE",
     "Apache License"),
    ("FFMPEG", "LGPL-2.1+", "Copyright the FFmpeg developers. Bink decoding only, linked statically; its source is "
     "at github.com/steverice/visr.", "port/third_party/ffmpeg-bink/COPYING.LGPLv2.1", "GNU LESSER GENERAL PUBLIC"),
    ("EXPAT", "MIT", "Copyright (c) 1998-2000 Thai Open Source Software Center Ltd and Clark Cooper; "
     "2001-2025 Expat maintainers.", "port/third_party/expat/COPYING", "Clark Cooper"),
    ("ZLIB", "ZLIB", "Copyright (C) 1995-2026 Jean-loup Gailly and Mark Adler.", "port/third_party/zlib/LICENSE",
     "Jean-loup Gailly"),
    ("KCP", "MIT", "Copyright (c) 2017 Lin Wei.", "port/third_party/kcp/LICENSE", "Lin Wei"),
    ("TOMLC17", "MIT", "Copyright (c) 2024-2026 CK Tan.", "port/third_party/tomlc17/LICENSE", "CK Tan"),
    ("STB_TRUETYPE", "MIT OR PUBLIC DOMAIN", "Copyright (c) 2017 Sean Barrett.", "port/third_party/stb/LICENSE",
     "Sean Barrett"),
    ("MONOCYPHER", "BSD-2-CLAUSE OR CC0", "Copyright (c) 2017-2023 Loup Vaillant, Michael Savage and Fabio Scotoni.",
     "port/third_party/monocypher/LICENCE.md", "Loup Vaillant"),
    ("XXHASH", "BSD-2-CLAUSE", "Copyright (c) 2012-2021 Yann Collet.", "port/third_party/xxhash/LICENSE",
     "Yann Collet"),
    ("BC7ENC", "MIT OR PUBLIC DOMAIN", "Copyright (c) 2018-2021 Richard Geldreich, Jr.",
     "port/third_party/bc7enc/LICENSE", "Richard Geldreich"),
    ("BC7 DECODER", "MIT, PUBLIC DOMAIN, ISC", "Copyright (c) 2018 Richard Geldreich, Jr.; (c) 2015 Harm "
     "Hanemaaijer.", "port/third_party/bc7enc16/LICENSE", "Harm Hanemaaijer"),
    ("ASTC ENCODER", "APACHE-2.0", "Copyright Arm Limited.", "port/third_party/astcenc/LICENSE.txt",
     "Apache License"),
    ("SMAA", "MIT", "Copyright (C) 2013 Jorge Jimenez, Jose I. Echevarria, Belen Masia, Fernando Navarro and "
     "Diego Gutierrez.", "port/third_party/smaa/LICENSE", "Jorge Jimenez"),
    ("PBRIFY UPSCALER", "CC0 1.0", "Kim2091's 4x-PBRify_UpscalerSPANV4 weights, converted to Core ML. Public "
     "domain dedication.", "port/ios/Licenses/PBRify-CC0-1.0.txt", "CC0 1.0"),
    ("STREAMER WORDS", "MIT", "Copyright (c) Surge AI. The profanity list streamer mode's words derive from.",
     "port/assets/text_filter/README.md", "Surge AI"),
    ("ENGLISH WORD LIST", "UNLICENSE", "Public domain. Used to review the streamer words.",
     "port/third_party/english-words/LICENSE.md", "public domain"),
    ("KHRONOS HEADERS", "MIT, APACHE-2.0", "Copyright The Khronos Group Inc. OpenGL ES and EGL headers.", None,
     "Khronos"),
    ("OVERPASS", "SIL OFL 1.1", "Copyright 2021 The Overpass Project Authors.", "port/assets/fonts/Overpass-OFL.txt",
     "Overpass Project Authors"),
    ("OPENCE FONT", "SIL OFL 1.1", "Copyright 2026 The OpenCE Project Authors. Newtown with respaced glyphs.",
     "port/assets/fonts/OpenCE-OFL.txt", "OpenCE Project Authors"),
    ("NEWTOWN", "PUBLIC DOMAIN", "Roger White, 1994, released by its author into the public domain.",
     "port/assets/fonts/Newtown-LICENSE.txt", "Roger White"),
    ("ORBITRON", "SIL OFL 1.1", "Copyright 2018 The Orbitron Project Authors.", "port/assets/brand/OFL-Orbitron.txt",
     "Orbitron Project Authors"),
]
