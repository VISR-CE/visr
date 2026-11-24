#ifndef CONSOLE_CONFIG_H
#define CONSOLE_CONFIG_H

/* the script console's `visr_set SECTION.KEY VALUE` (port/linux/src/console_config.c): returns 1 and fills reply
   when line is that command (development builds set the setting for this run; distribution builds refuse), 0 for
   any other line, which the console evaluates as a script */
int console_config_command(const char *line, char *reply, unsigned long reply_size);

#endif
