/* Apache-2.0. The About screen's SEND LOG row: the game's log to the developer through iCloud. */
#pragma once

/* whether this build can send a log (the same builds that deliver performance reports) */
int host_log_send_available(void);
/* Starts the upload on a worker. A press while one is running is ignored. */
void host_log_send_start(void);
/* The row's value, in capitals for the menu font: empty before a press, then SENDING..., SENT. CODE: ABCD1234 or why not.
   Copied into buffer. */
void host_log_send_status(char *buffer, unsigned int size);
