#ifndef LOGGER_H
#define LOGGER_H

#include <switch.h>

void event_agent_log(switch_log_level_t level, const char *fmt, ...);

#define EVENT_LOG_DEBUG(fmt, ...) event_agent_log(SWITCH_LOG_DEBUG, fmt, ##__VA_ARGS__)
#define EVENT_LOG_INFO(fmt, ...)  event_agent_log(SWITCH_LOG_INFO, fmt, ##__VA_ARGS__)
#define EVENT_LOG_WARNING(fmt, ...) event_agent_log(SWITCH_LOG_WARNING, fmt, ##__VA_ARGS__)
#define EVENT_LOG_ERROR(fmt, ...) event_agent_log(SWITCH_LOG_ERROR, fmt, ##__VA_ARGS__)

#endif
