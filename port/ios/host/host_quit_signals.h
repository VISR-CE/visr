/* Apache-2.0. The Mac apps' quit signals (host_quit_signals.c): SIGTERM, SIGINT and SIGHUP are taken by dispatch
   sources and handed to handler on queue, where it may do real work (a raw signal handler may not). */
#pragma once
#include <dispatch/dispatch.h>

typedef void (*host_quit_signal_handler)(int signal_number);
/* 1 when all three are watched; the signals' default action (ending the process at once) is turned off */
int host_quit_signals_install(dispatch_queue_t queue, host_quit_signal_handler handler);
