/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MAP_PICTURES_RENDER_H
#define MAP_PICTURES_RENDER_H
#include <stddef.h>
#include <stdint.h>

/* Disc import or debug.map_pictures_render: at launch, before the main menu is used, each stock map is loaded,
framed by photo mode's scripted capture and its picture written to the cache (map_pictures_render.c).
Called once a main-loop pass on the game thread. */
void map_pictures_render_update(void);
/* 1 while a map is loaded for its picture (loading to the readback): the level is drawn with the original
textures, never upscaled ones, so a picture never waits on the upscaler or changes with it (xbox_textures.c) */
int map_pictures_render_active(void);
/* main.c's map load reached a stage (0 cache ready, 1 tags loaded, 2 setup done, 3 shader warm done);
the time is logged with the picture, for a map loaded for its picture only */
void map_pictures_render_stage(int stage);
/* 1 when the next rendered frame is the picture (d3d8_device.c reads it then) */
int map_pictures_render_wants_frame(void);
/* the rendered frame's BGRA pixels, rows from the top */
void map_pictures_render_frame(const uint8_t *bgra, uint32_t width, uint32_t height, size_t stride);
/* the stock map's rendered menu picture (a BMP, by the map's base name, "a10" or "bloodgulch"), in memory
the caller frees with map_pictures_render_picture_free(); 0 when no cached picture exists */
int map_pictures_render_picture_read(const char *base_name, uint8_t **data, uint32_t *size);
void map_pictures_render_picture_free(uint8_t *data);
#endif
