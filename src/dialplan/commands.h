#ifndef DIALPLAN_COMMANDS_H
#define DIALPLAN_COMMANDS_H

#include <switch.h>
#include "manager.h"
#include "../drivers/interface.h"

/* Initialize dialplan command handlers */
switch_status_t command_dialplan_init(event_driver_t *driver, dialplan_manager_t *manager);

/* Shutdown dialplan command handlers */
void command_dialplan_shutdown(event_driver_t *driver);

#endif /* DIALPLAN_COMMANDS_H */
