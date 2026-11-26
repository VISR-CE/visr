/*
CINEMATIC_SKIP.C

The one place a request to skip a cinematic is made (cinematic_skip.h): the
player's skip button, the script console's visr_cinematic skip and a replay
all come here, so the co-op vote, the replay's ownership of skips and photo
mode treat them alike.
*/

#include "cseries.h"
#include "cutscene/cinematics.h"
#include "main/main.h"
#include "cinematic_skip.h"
#include "network_coop.h"

extern int photo_mode_active(void);
extern unsigned char input_replay_skips_cinematics(void);

/* this machine may ask: a local or host game's skippable cinematic, or a co-op client's offered vote (the skip
button's test before there was one function) */
static boolean cinematic_skip_offered(
	void)
{
	return cinematic_can_be_skipped() || network_coop_skip_offered();
}

int cinematic_skip_state(
	int *skippable)
{
	*skippable = cinematic_skip_offered() ? 1 : 0;
	return cinematic_in_progress() ? 1 : 0;
}

enum cinematic_skip_result cinematic_skip_request(
	enum cinematic_skip_source source)
{
	if (source == CINEMATIC_SKIP_PLAYER && input_replay_skips_cinematics())
		return CINEMATIC_SKIP_REPLAY_OWNS;
	if (!cinematic_skip_offered())
		return cinematic_in_progress() ? CINEMATIC_SKIP_NOT_SKIPPABLE : CINEMATIC_SKIP_NO_CINEMATIC;
	if (photo_mode_active())
		return CINEMATIC_SKIP_PHOTO_MODE;
	/* in network co-op the players vote to skip (network_coop.c) */
	if (network_coop_vote_skip())
		return CINEMATIC_SKIP_VOTED;
	main_skip_cinematic();
	return CINEMATIC_SKIP_REQUESTED;
}
