#ifndef COMMAND_CORE_H
#define COMMAND_CORE_H

#include "../mod_event_agent.h"

typedef struct {
    uint64_t requests_received;
    uint64_t requests_success;
    uint64_t requests_failed;
} command_stats_t;

switch_bool_t should_process_request(cJSON *json);
char* build_json_response(switch_bool_t success, const char *message, const char *data);
void command_stats_increment_received(void);
void command_stats_increment_success(void);
void command_stats_increment_failed(void);
void command_stats_get(uint64_t *requests, uint64_t *success, uint64_t *failed);

#endif
