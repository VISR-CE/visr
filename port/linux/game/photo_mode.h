#ifndef PHOTO_MODE_H
#define PHOTO_MODE_H
struct director;
/* Key actions are posted by the Mac-only SDL hook and applied on the game thread. */
enum photo_action { PHOTO_TOGGLE = 1, PHOTO_GUIDES, PHOTO_SAVE, PHOTO_LOAD,
	PHOTO_RENDER_CHECK, PHOTO_SLOWER, PHOTO_FASTER, PHOTO_FOV_DOWN, PHOTO_FOV_UP };
int photo_mode_key(int action);
int photo_mode_active(void);
void photo_mode_update(struct director *director, float *translation, float *roll_delta);
void photo_mode_end(void);
#endif
