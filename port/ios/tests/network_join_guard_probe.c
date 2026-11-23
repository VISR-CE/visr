/* The join guard names the first failing clause, in the assertion's order. */
#include "../../linux/include/halo_network_join_guard.h"
#include <assert.h>
#include <string.h>

int main(void)
{
	assert(halo_network_join_refusal(1, 1, 1, 1, 1, 0, 1) == HALO_NETWORK_JOIN_OK);
	assert(halo_network_join_refusal(0, 1, 1, 1, 1, 0, 1) == HALO_NETWORK_JOIN_NO_CLIENT);
	/* the second JOIN while joining: the state moved on from searching */
	assert(halo_network_join_refusal(1, 0, 1, 1, 1, 0, 1) == HALO_NETWORK_JOIN_NOT_SEARCHING);
	assert(halo_network_join_refusal(1, 1, 0, 1, 1, 0, 1) == HALO_NETWORK_JOIN_NO_GAME);
	assert(halo_network_join_refusal(1, 1, 1, 0, 1, 0, 1) == HALO_NETWORK_JOIN_NO_PARAMETERS);
	assert(halo_network_join_refusal(1, 1, 1, 1, 0, 0, 1) == HALO_NETWORK_JOIN_NO_CONNECTION);
	assert(halo_network_join_refusal(1, 1, 1, 1, 1, 1, 1) == HALO_NETWORK_JOIN_CONNECTED);
	assert(halo_network_join_refusal(1, 1, 1, 1, 1, 0, 0) == HALO_NETWORK_JOIN_WRONG_PLATFORM);
	assert(halo_network_join_refusal(1, 0, 0, 0, 0, 1, 0) == HALO_NETWORK_JOIN_NOT_SEARCHING);
	for (int refusal = 0; refusal <= HALO_NETWORK_JOIN_WRONG_PLATFORM; refusal++)
		assert(strlen(halo_network_join_refusal_name((enum halo_network_join_refusal)refusal)) > 10);
	return 0;
}
