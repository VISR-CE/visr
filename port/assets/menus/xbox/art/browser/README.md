# Server browser art

Regenerate these PNGs with `python3 tools/browser_assets.py` and
`rsvg-convert`. Each PNG has four pixels per menu unit. The generator builds
SVG documents for the backgrounds and rings; `icons/` holds the supplied CC0
gametype redraws from the approved browser mockup.

The art contains no text. The game supplies captions, player counts, and
detail values. Map pictures come from the player's own game data.

| Frames | Menu units | Meaning |
| --- | --- | --- |
| `row__0`, `row__1` | 270 × 54 | Normal, focused |
| `option__0`, `option__1` | 532 × 54 | Normal, focused filter row |
| `chip_all__0` through `__3` | 78 × 54 | Unselected, selected, focused, selected and focused |
| `chip_multiplayer__0` through `__3` | 144 × 54 | Same states |
| `chip_coop__0` through `__3` | 100 × 54 | Same states |
| `toggle__0` through `__3` | 174 × 54 | Same states |
| `preview_none` | 310 × 75 | Transparent map-picture fallback |
| `detail` | 312 × 339 | Detail panel |
| `big` | 294 × 20 | Capacity warning band |
| `badge` | 120 × 18 | Outlined badge |
| `join__0`, `join__1` | 294 × 53 | Normal, focused |
| `gametype__0` through `__5` | 36 × 36 | Co-op, CTF, Slayer, Oddball, King, Race |
| `gauge__00` through `__32` | 42 × 42 | Occupancy from empty to full in 32 steps |

Chip and toggle art starts at y=22; the transparent band above extends the
widget's target into the title band without making the visible chip taller.
The blue and black icon shapes retain the approved SVGs' colors and aspect
ratios.
