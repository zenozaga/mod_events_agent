#ifndef COMMAND_STATUS_H
#define COMMAND_STATUS_H

#include "../driver_interface.h"

void handle_status(const char *subject, const char *data, size_t len, const char *reply_to, void *user_data);
switch_status_t command_status_register(event_driver_t *driver);

#endif
