#ifndef PHOTO_FRAMING_H
#define PHOTO_FRAMING_H

#include <stddef.h>

struct photo_shot
{
	float position[3], forward[3], up[3], fov;
	int bsp, settle_ticks, erase_ai;
	float focus[2], safe[4];
	char variant[64];
};

/* These helpers have no engine dependencies; tests use synthetic camera values. */
float photo_fov_step(float fov, int steps);
void photo_level_up(const float forward[3], float up[3]);
int photo_stock_scenario(const char *name);
int photo_map_key(const char *scenario, int custom, const unsigned char *hash, char *key, size_t size);
int photo_shot_valid(const struct photo_shot *shot);
int photo_fragment_read(const char *path, const char *key, struct photo_shot *shot, int *revision);
int photo_fragment_write(const char *path, const char *key, const struct photo_shot *shot);
int photo_camera_read(const char *path, struct photo_shot *shot);

#endif
