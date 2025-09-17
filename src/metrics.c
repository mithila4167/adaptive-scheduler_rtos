// File: src/metrics.c
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "rtos.h"

/* Single file handle for the whole run */
static FILE* g_metrics_fp = NULL;

int metrics_open(const char* path) {
    if (g_metrics_fp) {
        return 0; // already open
    }
    g_metrics_fp = fopen(path, "w");
    if (!g_metrics_fp) {
        fprintf(stderr, "metrics_open: failed to open %s: %s\n", path, strerror(errno));
        return -1;
    }

    // Optional: buffered I/O for efficiency
    setvbuf(g_metrics_fp, NULL, _IOFBF, 1 << 15); // 32KB buffer

    // CSV header
    if (fprintf(g_metrics_fp,
        "tick,task_id,current_priority,remaining_time,waiting_time,queue_len,cpu_usage,is_running\n") < 0) {
        fprintf(stderr, "metrics_open: header write failed: %s\n", strerror(errno));
        fclose(g_metrics_fp);
        g_metrics_fp = NULL;
        return -1;
    }
    if (fflush(g_metrics_fp) != 0) {
        fprintf(stderr, "metrics_open: fflush failed: %s\n", strerror(errno));
        fclose(g_metrics_fp);
        g_metrics_fp = NULL;
        return -1;
    }
    return 0;
}

void metrics_log_tick(int tick,
                      const RtosTask* tasks,
                      int task_count,
                      int running_task_id,
                      int ready_queue_len,
                      double cpu_usage)
{
    if (!g_metrics_fp) return;

    for (int i = 0; i < task_count; ++i) {
        const RtosTask* t = &tasks[i];
        int is_running = (t->id == running_task_id) ? 1 : 0;

        if (fprintf(g_metrics_fp, "%d,%d,%d,%d,%d,%d,%.2f,%d\n",
                    tick,
                    t->id,
                    t->priority,
                    t->remaining_time,
                    t->waiting_time,
                    ready_queue_len,
                    cpu_usage,
                    is_running) < 0) {
            fprintf(stderr, "metrics_log_tick: write failed: %s\n", strerror(errno));
        }
    }

    // Flush so external AI can read live
    if (fflush(g_metrics_fp) != 0) {
        fprintf(stderr, "metrics_log_tick: fflush failed: %s\n", strerror(errno));
    }
}

void metrics_close(void) {
    if (g_metrics_fp) {
        if (fclose(g_metrics_fp) != 0) {
            fprintf(stderr, "metrics_close: fclose failed: %s\n", strerror(errno));
        }
        g_metrics_fp = NULL;
    }
}
