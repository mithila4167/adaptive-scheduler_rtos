#include "rtos.h"
#include <stdio.h>

/* Logging helpers implemented once here to avoid multiple definitions. */

void log_info(const char* message) {
    printf("[INFO]: %s\n", message);
}

void log_error(const char* message) {
    printf("[ERROR]: %s\n", message);
}

/* Optional: Log a task-specific action */
void log_task(int id, const char* action) {
    printf("[TASK %d]: %s\n", id, action);
}
