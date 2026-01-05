#include "mod_event_agent.h"
#include <stdarg.h>

void event_agent_log(switch_log_level_t level, const char *fmt, ...)
{
    switch_log_level_t effective_level = globals.log_level ? globals.log_level : SWITCH_LOG_INFO;

    if (level > effective_level) {
        return;
    }

    char *log_line;
    va_list ap;

    va_start(ap, fmt);
    if (switch_vasprintf(&log_line, fmt, ap) != -1) {
        switch_log_printf(SWITCH_CHANNEL_LOG, level, "[mod_event_agent] %s\n", log_line);
        free(log_line);
    }
    va_end(ap);
}
