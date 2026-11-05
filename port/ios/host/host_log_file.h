/* Apache-2.0. ios-runtime.log: the host's own log, kept in the app's Documents. */
#ifndef HOST_LOG_FILE_H
#define HOST_LOG_FILE_H

#include <stdio.h>

/* Starts a session's log in the working directory. The log of the session before (ios-runtime.log, when it has
   anything) becomes ios-runtime.previous.log, the earlier one of that is replaced, and a rotated file left by it
   goes. The log is then kept to limit bytes: past it the file becomes ios-runtime.rotated.log and a new
   ios-runtime.log starts, over the same descriptor, which host_crash.c holds. Returns the log, NULL if it cannot
   be opened. */
FILE *host_log_file_open(long limit);
/* one line (no newline) into the log; nothing before it is opened */
void host_log_file_line(const char *text);

#endif
