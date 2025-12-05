#include "mod_event_agent.h"
#include <switch_json.h>

char *serialize_event_to_json(switch_event_t *event, const char *node_id)
{
    cJSON *json_event = NULL;
    cJSON *headers = NULL;
    char *json_str = NULL;
    switch_event_header_t *hp;

    if (!event) {
        return NULL;
    }

    json_event = cJSON_CreateObject();
    if (!json_event) {
        EVENT_LOG_ERROR("Failed to create JSON object");
        return NULL;
    }

    cJSON_AddStringToObject(json_event, "event_name", 
                           switch_event_name(event->event_id));
    
    cJSON_AddNumberToObject(json_event, "timestamp", 
                           (double)switch_micro_time_now());

    if (node_id) {
        cJSON_AddStringToObject(json_event, "node_id", node_id);
    }

    const char *uuid = switch_event_get_header(event, "Unique-ID");
    if (uuid) {
        cJSON_AddStringToObject(json_event, "uuid", uuid);
    }

    headers = cJSON_CreateObject();
    if (headers) {
        for (hp = event->headers; hp; hp = hp->next) {
            if (hp->name && hp->value) {
                cJSON_AddStringToObject(headers, hp->name, hp->value);
            }
        }
        cJSON_AddItemToObject(json_event, "headers", headers);
    }

    if (event->body) {
        cJSON_AddStringToObject(json_event, "body", event->body);
    }

    json_str = cJSON_PrintUnformatted(json_event);
    cJSON_Delete(json_event);

    return json_str;
}

void free_serialized_event(char *json)
{
    if (json) {
        cJSON_free(json);
    }
}
