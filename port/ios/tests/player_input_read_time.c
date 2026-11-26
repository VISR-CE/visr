/* Time of the synchronous profile read: open, read 512 bytes, decode the extension (what player_profile_get_input does
besides the saved-games mutex), over 20000 reads of a file in the temp folder. */
#include "../../linux/game/player_input_profile.c"
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include "test_tmpdir.h"
int main(void)
{
	unsigned char block[512] = { 0 }, read_back[512];
	struct player_input_extension extension, decoded;
	char folder[PATH_MAX], path[PATH_MAX + 16];
	FILE *out;
	int i, n = 20000;
	struct timespec start, end;
	double worst = 0.0, total = 0.0;

	player_input_extension_defaults(&extension);
	player_input_extension_encode(&extension, block + PLAYER_INPUT_REGION_OFFSET);
	assert(test_mkdtemp(folder, sizeof(folder), "pip-read-time"));
	snprintf(path, sizeof(path), "%s/profile", folder);
	out = fopen(path, "wb");
	assert(out && fwrite(block, 1, 512, out) == 512);
	fclose(out);
	for (i = 0; i < n; i++)
	{
		double us;
		FILE *file;

		clock_gettime(CLOCK_MONOTONIC, &start);
		file = fopen(path, "rb");
		assert(file && fread(read_back, 1, 512, file) == 512);
		fclose(file);
		player_input_extension_decode(read_back + PLAYER_INPUT_REGION_OFFSET, &decoded);
		clock_gettime(CLOCK_MONOTONIC, &end);
		us = (end.tv_sec - start.tv_sec) * 1e6 + (end.tv_nsec - start.tv_nsec) / 1e3;
		total += us;
		if (us > worst)
			worst = us;
	}
	printf("profile input read: mean %.1f us, worst %.1f us over %d reads\n", total / n, worst, n);
	return 0;
}
