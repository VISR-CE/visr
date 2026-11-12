/*
HALO_NETWORK_VERSION.H

The current agreement is 38. A client also joins hosts of 29 to 32 and 34 to
36, and 39 to 41 (33 and 37 were never released). Hosts of 29 to 34 send the 13,120-byte
settings record with the 28-byte options block, which the receiver widens;
hosts of 35 to 37 send the 13,136-byte record. Each
later rule is taken only from a host whose version has it (the helpers below);
callers ask a helper rather than compare version numbers. Join requests to
hosts of 42 and later end with the client's version (the shared contract from
GulchCE); earlier hosts do not learn it. Signed table rows may
extend discovery numbers without changing the game codec.
*/

#ifndef __HALO_NETWORK_VERSION_H
#define __HALO_NETWORK_VERSION_H

#include "halo_port_limits.h"
#include "halo_delta_legacy.h"

/* the oldest version a client joins (29 is the last whose rules the later
ones add to) */
#define HALO_PORT_NETWORK_OLDEST_JOINABLE 29

/* what each version added */
#define HALO_NETWORK_VERSION_ELEVATOR_RIDERS 30
#define HALO_NETWORK_VERSION_STREAM_RULES 31
#define HALO_NETWORK_VERSION_AIM_POLICY 32
/* the host's co-op ally switch (settings cooperative flags bit 1) */
#define HALO_NETWORK_VERSION_ALLY_SWITCH 33
/* shared damage for shield-only scenery and devices, with the host's say over
their own destruction */
#define HALO_NETWORK_VERSION_STATIC_SHIELDS 34
/* the 44-byte options block (13,136-byte settings record) and its modes */
#define HALO_NETWORK_VERSION_MODE_OPTIONS 35
/* the host's shield-effects policy (distributed message 91), selectors 0 to 5 */
#define HALO_NETWORK_VERSION_SHIELD_POLICY 36
/* the game kind in the LAN advertisement */
#define HALO_NETWORK_VERSION_GAME_KIND 37
/* shield-effects selector 6 (PLAYERS' OWN) */
#define HALO_NETWORK_VERSION_PLAYERS_OWN_SHIELDS 38
/* dedicated-server discovery metadata; the join codec stays at 38 */
#define HALO_NETWORK_VERSION_DEDICATED 39
/* the join request adds an empty pass and proof field */
#define HALO_NETWORK_VERSION_JOIN_PASS_FIELD 40
/* The built-in ceiling; signed tables may extend it. */
#define HALO_NETWORK_VERSION_NEWEST_JOINABLE 41
/* the shared contract (chester's, GulchCE): the join request ends with the
client's network version, the settings record carries two 32-byte digests
(zero for Xbox maps), the distributed header grows to 12 bytes with a
checkpoint epoch, message 96 carries checkpoints, scripted player poses, and
simulation fixes every peer must apply. No OpenCE release used 42; 43 is the
first. */
#define HALO_NETWORK_VERSION_SHARED_CONTRACT 42
/* the host's vehicle riding switch (OpenCE PR #637) */
#define HALO_NETWORK_VERSION_RIDING 43

/* the older versions a client of this build joins, one bit per version from
the oldest: 29 to 32, and 34 to 36, each checked by joining a host of that
version. No host of 33 or 37 was released, so neither is joined. */
#define HALO_NETWORK_JOINABLE_OLDER_MASK 0xEFul

/* whether a client of version `ours` joins a host of version `theirs` */
static inline int halo_network_version_joinable(unsigned ours, unsigned theirs)
{
	/* Signed rows extend only the newer side; the historical mask stays exact. */
	if (ours == HALO_PORT_NETWORK_VERSION && theirs > HALO_NETWORK_VERSION_NEWEST_JOINABLE &&
		delta_legacy_joinable(theirs))
		return 1;
	/* port: newer hosts keep the game codec; 40 and 41 need join padding. */
	if (ours == HALO_NETWORK_VERSION_PLAYERS_OWN_SHIELDS &&
		theirs >= HALO_NETWORK_VERSION_DEDICATED && theirs <= HALO_NETWORK_VERSION_NEWEST_JOINABLE)
		return 1;
	/* port: 43 hosts use the shared contract (352-byte join, 12-byte header,
	settings digests); 42 was never released and joins only through a signed row */
	if (ours == HALO_NETWORK_VERSION_PLAYERS_OWN_SHIELDS && theirs == HALO_NETWORK_VERSION_RIDING)
		return 1;
	if (theirs == ours)
		return 1;
	if (ours < theirs || theirs < HALO_PORT_NETWORK_OLDEST_JOINABLE ||
		theirs - HALO_PORT_NETWORK_OLDEST_JOINABLE >= 32u)
		return 0;
	return (HALO_NETWORK_JOINABLE_OLDER_MASK >> (theirs - HALO_PORT_NETWORK_OLDEST_JOINABLE)) & 1u;
}

/* the version whose wire contract this build speaks to a joinable host of
version `theirs`. 43 is the shared contract. A newer version that only a signed
row makes joinable is presumed additive over the 41 codec: it keeps 41's join
padding and none of the shared contract (no version word, no digests, no epoch
header, no scripted poses, no shared simulation fixes). */
static inline unsigned halo_network_version_wire_contract(unsigned theirs)
{
	if (theirs > HALO_NETWORK_VERSION_NEWEST_JOINABLE && theirs != HALO_NETWORK_VERSION_RIDING)
		return HALO_NETWORK_VERSION_NEWEST_JOINABLE;
	return theirs;
}

/* whether a host of the version sends the aim-assist policy (message 90);
a client of an older host plays with the mouse's magnetism as set */
static inline int halo_network_version_has_aim_policy(unsigned version)
{
	return version >= HALO_NETWORK_VERSION_AIM_POLICY;
}

/* whether a host of the version applies its co-op ally switch */
static inline int halo_network_version_has_ally_switch(unsigned version)
{
	return version >= HALO_NETWORK_VERSION_ALLY_SWITCH;
}

/* whether a host of the version shares damage to shield-only static objects */
static inline int halo_network_version_has_static_shields(unsigned version)
{
	return version >= HALO_NETWORK_VERSION_STATIC_SHIELDS;
}

/* whether a host of the version sends the 13,136-byte settings record with
the 44-byte options block; older hosts send the 28-byte block */
static inline int halo_network_version_has_mode_options(unsigned version)
{
	return version >= HALO_NETWORK_VERSION_MODE_OPTIONS;
}

/* whether a host of the version sends its shield-effects policy (message 91) */
static inline int halo_network_version_has_shield_policy(unsigned version)
{
	return version >= HALO_NETWORK_VERSION_SHIELD_POLICY;
}

/* whether a host of the version sends this shield-effects selector: 0 to 5
from 36, and 6 (PLAYERS' OWN) from 38 */
static inline int halo_network_version_shield_selector_valid(unsigned version, unsigned long selector)
{
	if (!halo_network_version_has_shield_policy(version))
		return 0;
	return selector <= (version >= HALO_NETWORK_VERSION_PLAYERS_OWN_SHIELDS ? 6ul : 5ul);
}

/* whether a host of the version advertises its game kind on the LAN */
static inline int halo_network_version_has_game_kind(unsigned version)
{
	return version >= HALO_NETWORK_VERSION_GAME_KIND;
}

/* whether OpenCE discovery fields can mark a host as dedicated */
static inline int halo_network_version_has_dedicated(unsigned version)
{
	return version >= HALO_NETWORK_VERSION_DEDICATED;
}

/* whether a host requires the pass and proof field in the join request */
static inline int halo_network_version_has_join_pass_field(unsigned version)
{
	return version >= HALO_NETWORK_VERSION_JOIN_PASS_FIELD;
}

/* whether a host of the version requires the client's network version at the
end of the join request (a big-endian word after the pass and proof field) */
static inline int halo_network_version_has_join_version_field(unsigned version)
{
	return version >= HALO_NETWORK_VERSION_SHARED_CONTRACT;
}

/* whether the settings record carries the two 32-byte map digests */
static inline int halo_network_version_has_settings_digests(unsigned version)
{
	return version >= HALO_NETWORK_VERSION_SHARED_CONTRACT;
}

/* whether distributed messages carry the 12-byte header with the checkpoint
epoch, and message 96 carries checkpoints */
static inline int halo_network_version_has_checkpoint_epoch(unsigned version)
{
	return version >= HALO_NETWORK_VERSION_SHARED_CONTRACT;
}

/* whether unit updates may carry the scripted pose part and suspended flag */
static inline int halo_network_version_has_scripted_poses(unsigned version)
{
	return version >= HALO_NETWORK_VERSION_SHARED_CONTRACT;
}

/* whether a game of the version runs the shared simulation fixes (powered
boat controls zeroed before use, and the others the contract lists); games
and recordings of older versions keep the original simulation */
static inline int halo_network_version_has_shared_sim_fixes(unsigned version)
{
	return version >= HALO_NETWORK_VERSION_SHARED_CONTRACT;
}

/* whether a host of the version may turn vehicle riding on */
static inline int halo_network_version_has_riding(unsigned version)
{
	return version >= HALO_NETWORK_VERSION_RIDING;
}

/* how far a datagram's tick may be from the latest the sender's others
carried, in ticks (15 seconds); over its stream any is taken */
#define HALO_NETWORK_STAMP_WINDOW_TICKS 450

static inline int halo_network_stamp_in_window(long latest, long tick)
{
	long difference = tick - latest;

	return difference <= HALO_NETWORK_STAMP_WINDOW_TICKS && difference >= -HALO_NETWORK_STAMP_WINDOW_TICKS;
}

/* whether a message stamped tick is taken, given the latest tick taken from its
sender (-1: none yet) and whether it came over the sender's stream */
static inline int halo_network_stamp_accepts(long latest, long tick, int stream)
{
	return stream || latest == -1 || halo_network_stamp_in_window(latest, tick);
}

#endif
