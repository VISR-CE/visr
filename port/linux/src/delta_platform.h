/* Pure platform-key construction; callers supply host facts, never system handles. */
#ifndef DELTA_PLATFORM_H
#define DELTA_PLATFORM_H
#include "halo_delta.h"

/* Apple targets share a guest binary, so the host's existing device flags select its platform. */
int delta_platform_current(int mac, int tv, int vision);
/* memory_mb = 0 uses the policy row's memory class; otherwise floor(log2(MB)). */
void delta_platform_key(unsigned char key[DELTA_PLATFORM_KEY_SIZE], int platform,
	int opted_in, int dedicated, unsigned long memory_mb);
const char *delta_platform_arch(void);
#endif
