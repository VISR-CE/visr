#ifndef PHOTO_MODE_H
#define PHOTO_MODE_H
struct director;
/* Key actions are posted by the Mac-only SDL hook and applied on the game thread. */
enum photo_action { PHOTO_TOGGLE = 1, PHOTO_GUIDES, PHOTO_SAVE, PHOTO_LOAD,
	PHOTO_RENDER_CHECK, PHOTO_SLOWER, PHOTO_FASTER, PHOTO_FOV_DOWN, PHOTO_FOV_UP, PHOTO_TELEPORT,
	PHOTO_ROLL_LEFT, PHOTO_ROLL_RIGHT, PHOTO_ROLL_RESET, PHOTO_LOAD_CANDIDATE };
void photo_mode_trace_key(int scancode, int action);
int photo_mode_key(int action);
/* Releases a held key: PHOTO_ROLL_LEFT and PHOTO_ROLL_RIGHT roll while held. */
void photo_mode_key_up(int action);
int photo_mode_active(void);
/* facing is yaw, pitch, roll; translation is forward, left, up (struct camera_control). */
void photo_mode_update(struct director *director, unsigned char *controls_active, float seconds, float facing[3], float translation[3]);
void photo_mode_end(void);
/* Scripted capture for map pictures: enters photo mode on the next director update without
debug.photo_mode, places the camera from the map's saved framing or an automatic camera, settles
and freezes. State: 1 frozen and ready (framed is 1 when a saved framing was used, revision is its
revision), 0 still settling, -1 failed. photo_mode_end() leaves it. */
void photo_mode_capture_begin(void);
/* Sets the gametype the next map load starts with from the map's framing (see photo_mode.c). */
void photo_mode_prepare_variant(const char *key);
void photo_mode_clear_variant(void);
int photo_mode_capture_state(int *framed, int *revision);
#endif
