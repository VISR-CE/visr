#ifndef CONSOLE_CONFIG_H
#define CONSOLE_CONFIG_H

/* the script console's own commands (port/linux/src/console_config.c's table: visr_set SECTION.KEY VALUE,
   visr_cinematic [skip]): returns 1 and fills reply when line is one of them (development builds run it;
   distribution builds refuse), 0 for any other line, which the console evaluates as a script */
int console_config_command(const char *line, char *reply, unsigned long reply_size);

#endif
