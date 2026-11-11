#ifndef PHOTO_MODE_H
#define PHOTO_MODE_H
struct director;
/* Key actions are posted by the Mac-only SDL hook and applied on the game thread. */
enum photo_action { PHOTO_TOGGLE = 1, PHOTO_GUIDES, PHOTO_SAVE, PHOTO_LOAD,
	PHOTO_RENDER_CHECK, PHOTO_SLOWER, PHOTO_FASTER, PHOTO_FOV_DOWN, PHOTO_FOV_UP, PHOTO_TELEPORT,
	PHOTO_ROLL_LEFT, PHOTO_ROLL_RIGHT, PHOTO_ROLL_RESET };
void photo_mode_trace_key(int scancode, int action);
int photo_mode_key(int action);
/* Releases a held key: PHOTO_ROLL_LEFT and PHOTO_ROLL_RIGHT roll while held. */
void photo_mode_key_up(int action);
int photo_mode_active(void);
/* facing is yaw, pitch, roll; translation is forward, left, up (struct camera_control). */
void photo_mode_update(struct director *director, unsigned char *controls_active, float seconds, float facing[3], float translation[3]);
void photo_mode_end(void);
#endif
