#include "rtos.h"
#include <stddef.h>

/*
 * Simple task timer mechanism.
 * Each task has a countdown timer in "ticks". The scheduler or a timer ISR
 * should call tick() periodically to decrement active timers.
 */

// TODO: Define MAX_TASKS in rtos.h for global consistency across modules.
#ifndef MAX_TASKS
#define MAX_TASKS 16
#endif

// Remaining ticks for each task; 0 means expired or not set.
int timers[MAX_TASKS] = {0};

// TODO: Add input validation/reporting as needed.
void set_timer(int task_id, int ticks) {
    if (task_id < 0 || task_id >= MAX_TASKS) {
        return;
    }
    timers[task_id] = ticks;
}

// Returns 1 if timer expired (<= 0), otherwise 0.
// TODO: Consider distinguishing "never set" from "expired".
int check_timer(int task_id) {
    if (task_id < 0 || task_id >= MAX_TASKS) {
        return 0;
    }
    return (timers[task_id] <= 0) ? 1 : 0;
}

// Decrement all non-zero timers to simulate time passing.
// TODO: Integrate with a real tick source and overflow handling if needed.
void tick(void) {
    for (int i = 0; i < MAX_TASKS; ++i) {
        if (timers[i] > 0) {
            timers[i] -= 1;
        }
    }
}
