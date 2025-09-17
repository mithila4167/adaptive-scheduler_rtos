#include "rtos.h"

/* Driver:
 * - Prompts user for number of tasks
 * - Prompts per task: id, priority (lower is higher), arrival time, burst time
 * - Loads tasks into scheduler, runs simulation, prints stats
 */

int main(void) {
    int n = 0;
    printf("Enter number of tasks: ");
    if (scanf("%d", &n) != 1 || n <= 0) {
        log_error("Invalid number of tasks");
        return 1;
    }

    scheduler_init(n);

    printf("Enter each task as: <id> <priority> <arrival_time> <burst_time>\n");
    printf("(lower priority value means higher priority).\n");
    for (int i = 0; i < n; ++i) {
        int id, prio, arr, burst;
        if (scanf("%d %d %d %d", &id, &prio, &arr, &burst) != 4) {
            log_error("Invalid task input");
            scheduler_cleanup();
            return 1;
        }
        if (scheduler_add_task(id, prio, arr, burst) < 0) {
            log_error("Failed to add task");
            scheduler_cleanup();
            return 1;
        }
    }

    log_info("Starting priority-based preemptive scheduling simulation...");
    start_scheduler();
    scheduler_print_stats();
    scheduler_cleanup();
    return 0;
}
