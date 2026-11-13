/* The network versions this build plays with (halo_network_version.h): which hosts a
client joins, what each version turns on, and the datagram stamp window. */
#include "../../linux/include/halo_network_version.h"
#include "../../linux/include/halo_network_content.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
	unsigned version;

	assert(HALO_PORT_NETWORK_VERSION == 38 && HALO_PORT_NETWORK_OLDEST_JOINABLE == 29);
	/* a build of 32 joins hosts of 29, 30, 31 and 32, and no others */
	for (version = 0; version < 40; version++)
	{
		int expected = version >= 29 && version <= 32;

		assert(halo_network_version_joinable(32, version) == expected);
	}
	/* a build of 38 joins hosts of 29 to 32, 34 to 36, 38 to 41, 43 and 44, and no others
	   (no host of 33, 37 or 42 was released to join) */
	for (version = 0; version < 0x10000; version++)
		assert(halo_network_version_joinable(38, version) ==
			((version >= 29 && version <= 32) || (version >= 34 && version <= 36) || (version >= 38 && version <= 41) ||
				version == 43 || version == 44));
	/* Other clients still refuse newer hosts. */
	for (unsigned ours = 0; ours <= 41; ours++)
		for (version = ours + 1; version <= 42; version++)
			assert(halo_network_version_joinable(ours, version) ==
				(ours == 38 && version >= 39 && version <= 41));
	/* 44 (MCC map sharing) has the 43 layout; a 43 build still refuses it, and 44 builds are not joined by older ones */
	assert(halo_network_version_joinable(38, 44) && !halo_network_version_joinable(32, 44) && !halo_network_version_joinable(41, 44));
	assert(halo_network_version_is_shared_release(43) && halo_network_version_is_shared_release(44) &&
		!halo_network_version_is_shared_release(42) && !halo_network_version_is_shared_release(45));
	/* a build of 29 (a VISR before the range) joins only its own */
	assert(halo_network_version_joinable(29, 29) && !halo_network_version_joinable(29, 30));
	for (version = 0; version < 0x10000; version++)
		assert(halo_network_version_has_join_pass_field(version) == (version >= 40));
	/* message 90 comes from hosts of 32 */
	assert(!halo_network_version_has_aim_policy(29) && !halo_network_version_has_aim_policy(31));
	assert(halo_network_version_has_aim_policy(32) && halo_network_version_has_aim_policy(33));
	/* what each host version sends and takes */
	for (version = 29; version <= 41; version++)
	{
		unsigned long selector;

		assert(halo_network_version_has_aim_policy(version) == (version >= 32));
		assert(halo_network_version_has_ally_switch(version) == (version >= 33));
		assert(halo_network_version_has_static_shields(version) == (version >= 34));
		/* 33 and 34 keep the 13,120-byte record with the 28-byte options block */
		assert(halo_network_version_has_mode_options(version) == (version >= 35));
		/* 35 sends no message 91; 36 and 37 send it without selector 6 */
		assert(halo_network_version_has_shield_policy(version) == (version >= 36));
		for (selector = 0; selector <= 8; selector++)
			assert(halo_network_version_shield_selector_valid(version, selector) ==
				(version >= 36 && (selector <= 5 || (selector == 6 && version >= 38))));
		assert(!halo_network_version_shield_selector_valid(version, 0xFFFFFFFFul));
		assert(halo_network_version_has_game_kind(version) == (version >= 37));
		assert(halo_network_version_has_dedicated(version) == (version >= 39));
	}
	/* the shared contract from 42, riding from 43 */
	for (version = 0; version < 0x10000; version++)
	{
		int contract = version >= 42;

		assert(halo_network_version_has_join_version_field(version) == contract);
		assert(halo_network_version_has_settings_digests(version) == contract);
		assert(halo_network_version_has_checkpoint_epoch(version) == contract);
		assert(halo_network_version_has_scripted_poses(version) == contract);
		assert(halo_network_version_has_shared_sim_fixes(version) == contract);
		assert(halo_network_version_has_riding(version) == (version >= 43));
	}
	/* a version joinable only through a signed row speaks the 41 codec; 43 keeps the shared contract */
	for (version = 0; version < 0x10000; version++)
	{
		unsigned contract = halo_network_version_wire_contract(version);

		assert(contract == (version > 41 && version != 43 && version != 44 ? 41u : version));
		if (version > 41 && version != 43 && version != 44)
			assert(halo_network_version_has_join_pass_field(contract) &&
				!halo_network_version_has_join_version_field(contract) &&
				!halo_network_version_has_checkpoint_epoch(contract) &&
				!halo_network_version_has_scripted_poses(contract) &&
				!halo_network_version_has_shared_sim_fixes(contract));
	}
	assert(halo_network_version_has_join_version_field(halo_network_version_wire_contract(43)));
	/* 44 keeps its number on the wire and has exactly the 43 layout */
	assert(halo_network_version_wire_contract(44) == 44);
	assert(halo_network_version_has_join_version_field(44) && halo_network_version_has_settings_digests(44) &&
		halo_network_version_has_checkpoint_epoch(44) && halo_network_version_has_scripted_poses(44) &&
		halo_network_version_has_shared_sim_fixes(44) && halo_network_version_has_riding(44));
	/* a 44 host's MCC map (nonzero settings digests) is refused with a message; Xbox and Custom Edition maps are not */
	{
		unsigned char digests[HALO_NETWORK_SETTINGS_DIGESTS_SIZE] = { 0 };

		assert(!halo_network_settings_digests_reason(digests));
		digests[HALO_NETWORK_SETTINGS_DIGESTS_SIZE - 1] = 1;
		assert(halo_network_settings_digests_reason(digests));
	}
	/* a stamp within 450 ticks of the latest either way is taken */
	assert(halo_network_stamp_in_window(1000, 1000) && halo_network_stamp_in_window(1000, 1450));
	assert(halo_network_stamp_in_window(1000, 550));
	assert(!halo_network_stamp_in_window(1000, 1451) && !halo_network_stamp_in_window(1000, 549));
	assert(!halo_network_stamp_in_window(0, 100000));
	/* a datagram out of the window is refused until the latest is forgotten (the clock was set: it jumped
	   600 ticks, as after a long stall), or its sender's stream moves the latest; a stream stamp is always taken */
	assert(!halo_network_stamp_accepts(1000, 1600, 0));
	assert(halo_network_stamp_accepts(-1, 1600, 0));
	assert(halo_network_stamp_accepts(1000, 1600, 1));
	assert(halo_network_stamp_accepts(1000, 1400, 0));
	puts("PASS: version range, per-version capabilities, stamp window");
	return 0;
}
