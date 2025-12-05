/*
 * logger.c
 * Logging utilities for mod_event_agent
 */

#include "mod_event_agent.h"
#include <stdarg.h>

void event_agent_log(switch_log_level_t level, const char *fmt, ...)
{
    char *log_line;
    va_list ap;

    va_start(ap, fmt);
    if (switch_vasprintf(&log_line, fmt, ap) != -1) {
        switch_log_printf(SWITCH_CHANNEL_LOG, level, "[mod_event_agent] %s\n", log_line);
        free(log_line);
    }
    va_end(ap);
}
