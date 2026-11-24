/* Temporary folders for native test programs: made under $TMPDIR (/tmp when it is unset) and removed when the
program ends (unless KEEP_TEST_TMP is set, for debugging), whether it returns, calls exit (a failed CHECK) or aborts (a failed assert). A test that made its own
folder under a hard-coded /tmp path and removed it only on the success path left one behind per failed or aborted
run. tools/test_harness_tmp_clean.py refuses any test source that makes a folder another way. */
#ifndef TEST_TMPDIR_H
#define TEST_TMPDIR_H

#include <dirent.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TEST_TMPDIR_MAX 64

static char test_tmpdir_made[TEST_TMPDIR_MAX][PATH_MAX];
static int test_tmpdir_count;
static int test_tmpdir_ready;
static pid_t test_tmpdir_owner;   /* a forked child that exits must not remove its parent's folders */

static void test_tmpdir_remove(char const *path)
{
	struct stat status;
	DIR *directory;
	struct dirent *entry;
	char child[PATH_MAX];

	if (lstat(path, &status) != 0)
		return;
	if (S_ISDIR(status.st_mode) && (directory = opendir(path)) != NULL)
	{
		while ((entry = readdir(directory)) != NULL)
		{
			if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
				continue;
			snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
			test_tmpdir_remove(child);
		}
		closedir(directory);
		rmdir(path);
	}
	else
		unlink(path);
}

static void test_tmpdir_cleanup(void)
{
	int index;

	if (getpid() != test_tmpdir_owner)
		return;
	if (getenv("KEEP_TEST_TMP") && *getenv("KEEP_TEST_TMP"))   /* debugging: leave the folders for inspection */
		return;
	for (index = 0; index < test_tmpdir_count; index++)
		test_tmpdir_remove(test_tmpdir_made[index]);
	test_tmpdir_count = 0;
}

static void test_tmpdir_abort(int signal_number)
{
	alarm(5);   /* the cleanup is not async-signal-safe: if the signal hit inside malloc it must not hang the run */
	test_tmpdir_cleanup();
	signal(signal_number, SIG_DFL);
	raise(signal_number);
}

/* makes `<TMPDIR>/<name>-XXXXXX` into `folder` (`size` bytes) and returns it, NULL when it cannot */
static char *test_mkdtemp(char *folder, size_t size, char const *name)
{
	char const *root = getenv("TMPDIR");

	if (!root || !*root)
		root = "/tmp";
	if (test_tmpdir_count >= TEST_TMPDIR_MAX)
	{
		int index, kept = 0;   /* a test that removes its own folders gives their places back */

		for (index = 0; index < test_tmpdir_count; index++)
			if (access(test_tmpdir_made[index], F_OK) == 0)
			{
				if (kept != index)
					memcpy(test_tmpdir_made[kept], test_tmpdir_made[index], PATH_MAX);
				kept++;
			}
		test_tmpdir_count = kept;
		if (test_tmpdir_count >= TEST_TMPDIR_MAX)
			return NULL;
	}
	if (!test_tmpdir_ready)
	{
		test_tmpdir_ready = 1;
		test_tmpdir_owner = getpid();
		atexit(test_tmpdir_cleanup);
		signal(SIGABRT, test_tmpdir_abort);
		signal(SIGSEGV, test_tmpdir_abort);
		signal(SIGBUS, test_tmpdir_abort);
		signal(SIGTERM, test_tmpdir_abort);
	}
	/* a template that does not fit is refused: a cut one loses its XXXXXX and mkdtemp would make one fixed folder */
	if (snprintf(folder, size, "%s%s%s-XXXXXX", root, root[strlen(root) - 1] == '/' ? "" : "/", name) >= (int)size
		|| !mkdtemp(folder))
		return NULL;
	snprintf(test_tmpdir_made[test_tmpdir_count++], PATH_MAX, "%s", folder);
	return folder;
}

#endif
