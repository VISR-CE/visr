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
/* The up axis for a camera rolled by roll radians about forward, the way the flying camera
turns it (positive tilts the top of the view to the right), and the roll an up axis carries. */
void photo_roll_up(const float forward[3], float roll, float up[3]);
float photo_roll_from_up(const float forward[3], const float up[3]);
int photo_stock_scenario(const char *name);
int photo_map_key(const char *scenario, int custom, const unsigned char *hash, char *key, size_t size);
int photo_shot_valid(const struct photo_shot *shot);
int photo_fragment_read(const char *path, const char *key, struct photo_shot *shot, int *revision);
int photo_fragment_write(const char *path, const char *key, const struct photo_shot *shot);
int photo_camera_read(const char *path, struct photo_shot *shot);

/* World units per second the keyboard flies the photo camera at speed step 0. */
#define PHOTO_FLY_SPEED 1.5f
/* Key bits as halo_keyboard_actions reports them (port/linux/include/halo_keyboard.h). */
enum { PHOTO_FLY_FORWARD = 1 << 0, PHOTO_FLY_BACKWARD = 1 << 1, PHOTO_FLY_LEFT = 1 << 2,
	PHOTO_FLY_RIGHT = 1 << 3, PHOTO_FLY_UP = 1 << 4, PHOTO_FLY_DOWN = 1 << 5 };
/* The flying camera's position change for one frame of held keys: i forward, j left, k up. */
void photo_fly_step(unsigned long keys, float seconds, int speed_step, float delta[3]);
/* Radians per second Q and E roll the photo camera at speed step 0. */
#define PHOTO_ROLL_SPEED 0.5235988f
/* The roll after one frame: direction -1 (Q, left), 0 or 1 (E, right); kept within plus or minus pi. */
float photo_roll_step(float roll, int direction, float seconds, int speed_step);

#endif
