/* Apache-2.0. ios-runtime.log (host_log_file.h). */
#include "host_log_file.h"

#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static FILE *log_file;
static long log_limit, log_bytes;

FILE *host_log_file_open(long limit)
{
	struct stat previous;

	if (stat("ios-runtime.log", &previous) == 0 && previous.st_size > 0)
		rename("ios-runtime.log", "ios-runtime.previous.log");
	unlink("ios-runtime.rotated.log");
	log_limit = limit;
	log_bytes = 0;
	log_file = fopen("ios-runtime.log", "w");
	return log_file;
}

void host_log_file_line(const char *text)
{
	long length;

	if (!log_file)
		return;
	flockfile(log_file);
	length = (long)strlen(text) + 1;
	if (log_bytes + length > log_limit)
	{
		int fresh;

		rename("ios-runtime.log", "ios-runtime.rotated.log");
		fresh = open("ios-runtime.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
		log_bytes = 0;
		if (fresh >= 0)
		{
			dup2(fresh, fileno(log_file));
			close(fresh);
			log_bytes = fprintf(log_file, "(log continued; the earlier part is ios-runtime.rotated.log)\n");
		}
	}
	fprintf(log_file, "%s\n", text);
	fflush(log_file);
	log_bytes += length;
	funlockfile(log_file);
}
