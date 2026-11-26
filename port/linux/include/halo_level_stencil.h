#ifndef HALO_LEVEL_STENCIL_H
#define HALO_LEVEL_STENCIL_H

/* The stencil bits the port reads to tell the level's pixels from others (ambient occlusion, soft particle edges,
traced shadows). The game's own marks: GAME is the first-person weapon's (its REJECT mode tests it, value 1 clear)
and OBJECT is the bit its object passes write. LEVEL is the port's: the lightmap pass writes it where it draws
(rasterizer_xbox_environment.c, when halo_level_pixels_mark(TRUE) says a feature wants it), and no mask or reference
the game or the presenter uses reads it. A pass that wants "the level, not the weapon" tests LEVEL_ONLY_READ for
equality with LEVEL. _MSL is the same value as a shader literal. */
#define HALO_STENCIL_GAME 0x01
#define HALO_STENCIL_OBJECT 0x04
#define HALO_STENCIL_LEVEL 0x10
#define HALO_STENCIL_LEVEL_MSL "0x10u"
#define HALO_STENCIL_LEVEL_ONLY_READ (HALO_STENCIL_LEVEL | HALO_STENCIL_GAME)

#endif
