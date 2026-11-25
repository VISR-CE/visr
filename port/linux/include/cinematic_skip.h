#ifndef CINEMATIC_SKIP_H
#define CINEMATIC_SKIP_H

/* A request to skip the cinematic in progress (port/linux/game/cinematic_skip.c), made the one way the player's skip
   button makes it: offered when this machine may skip (a local or host game's skippable cinematic, or a co-op client's
   vote offer), refused to the player while a replay owns skips, a vote in network co-op (the host waits for the skip
   save and the vote's cooldown), and the skip itself otherwise. Used by the skip button (player_control.c), the script
   console's visr_cinematic skip (console_config.c) and a replay reaching the tick its recording skipped
   (input_replay.c). */
enum cinematic_skip_source
{
	CINEMATIC_SKIP_PLAYER, /* the skip button or the console */
	CINEMATIC_SKIP_REPLAY  /* a replay that owns skips */
};

enum cinematic_skip_result
{
	CINEMATIC_SKIP_REQUESTED,     /* the game skips at the end of this frame (main_skip_cinematic) */
	CINEMATIC_SKIP_VOTED,         /* network co-op: this machine's vote is cast */
	CINEMATIC_SKIP_NO_CINEMATIC,
	CINEMATIC_SKIP_NOT_SKIPPABLE, /* not yet, or never, or the co-op vote is not offered */
	CINEMATIC_SKIP_REPLAY_OWNS,   /* a replay skips at its recording's tick instead */
	CINEMATIC_SKIP_PHOTO_MODE     /* photo mode blocks the skip */
};

enum cinematic_skip_result cinematic_skip_request(enum cinematic_skip_source source);

/* whether a cinematic is in progress (1 or 0), and whether this machine may ask to skip it (*skippable) */
int cinematic_skip_state(int *skippable);

#endif
