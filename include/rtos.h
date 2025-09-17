/*
 * RTOS Header File
 * Priority-based preemptive scheduler interfaces and types.
 */

#ifndef RTOS_H
#define RTOS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* ---------------- Scheduler API ---------------- */

typedef struct RtosTask {
    int id;
    int priority;          /* lower value => higher priority */
    int arrival_time;      /* when task becomes available (in cycles) */
    int burst_time;        /* total CPU cycles needed */
    int remaining_time;    /* internal: remaining CPU time */
    int completion_time;   /* internal: set when task finishes */
    int enqueued;          /* internal: 0/1, whether put into ready queue */
    int waiting_time;      /* REQUIRED: cumulative waiting ticks (for metrics/AI) */
} RtosTask;

/* Existing scheduler APIs you already have */
void scheduler_init(int maxTasks);
int scheduler_add_task(int id, int priority, int arrival_time, int burst_time);
void start_scheduler(void);
void scheduler_print_stats(void);
void scheduler_cleanup(void);

/* Logging (implemented in src/util.c) */
void log_info(const char* message);
void log_error(const char* message);
void log_task(int id, const char* action);

/* Metrics CSV helpers (implemented in src/metrics.c) */
int metrics_open(const char* path);
void metrics_log_tick(int tick,
                      const RtosTask* tasks,
                      int task_count,
                      int running_task_id,
                      int ready_queue_len,
                      double cpu_usage);
void metrics_close(void);

#endif /* RTOS_H */
