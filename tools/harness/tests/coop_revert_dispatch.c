#include "harness.h"
enum { _error_silent, _coop_event_reverted };
static boolean host;
static long tick;
static int repaired, refreshed, reconciled;
static boolean coop_host(void) { return host; }
static long game_time_get(void) { return tick; }
static void game_time_set_distributed(long now) { tick = now; }
static void update_queues_reset_and_fill_with_lies(void) { repaired++; }
static void game_state_port_restamp_revert_time(void) { }
static void hs_runtime_port_shift_sleep_times(long ticks) { CHECK(ticks == 200, "wrong clock shift"); }
static void coop_enemies_reset(void) { }
#define error(...) ((void)0)
static void event_new(int kind) { }
static void coop_reconcile_departures(void)
{
	CHECK(tick == 300, "departure applied before clock repair"); reconciled++;
}
static void network_objects_coop_reverted(void)
{
	CHECK(tick == 300, "object state sent before clock repair");
	CHECK(reconciled == refreshed + 1, "restored units sent before departure handling"); refreshed++;
}
#include "under_test.inc"
int main(int argc, char **argv)
{
	tick = 100; host = FALSE; network_coop_reverted(300);
	CHECK(!refreshed && tick == 100, "client or local game changed");
	host = TRUE; network_coop_reverted(300);
	CHECK(refreshed == 1 && repaired == 1, "host did not refresh restored objects");
	network_coop_reverted(300);
	CHECK(refreshed == 2 && repaired == 1, "same-tick revert omitted refresh");
	return 0;
}
