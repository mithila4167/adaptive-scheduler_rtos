#include "rtos.h"
#include <stdio.h>
#include <stdlib.h>

/* Priority-based preemptive scheduler with 2-cycle time slice (quantum).
 * - Tasks arrive over time; ready queue kept sorted by (priority asc, id asc)
 * - On each tick:
 *   - Enqueue new arrivals
 *   - Select highest priority task
 *   - Run 1 cycle; after 2 cycles or preemption by higher-priority arrival, reselect
 * - Tracks completion/turnaround/waiting times; printed at the end
 *
 * NOTE:
 * - Metrics helpers (metrics_open, metrics_log_tick, metrics_close) are implemented in src/metrics.c.
 * - Logging helpers (log_info, log_error, log_task) are implemented in src/util.c.
 * - This file MUST NOT define metrics_* or log_* functions to avoid multiple definitions.
 */

#ifndef AI_DEBUG
#define AI_DEBUG 0
#endif
#define AI_DPRINTF(...) do { if (AI_DEBUG) printf(__VA_ARGS__); } while (0)

typedef struct ReadyNode {
    int task_index;               /* index into task_table */
    struct ReadyNode* next;
} ReadyNode;

static RtosTask* task_table = NULL;
static int task_count = 0;
static int task_capacity = 0;

static ReadyNode* ready_head = NULL;
static int sim_time = 0;

static const int QUANTUM = 2;

/* ---------- Ready queue helpers (sorted by prio,id) ---------- */
static void ready_push_sorted(int task_index) {
    ReadyNode* node = (ReadyNode*)malloc(sizeof(ReadyNode));
    if (!node) return; /* TODO: handle allocation error */
    node->task_index = task_index;
    node->next = NULL;

    if (!ready_head) {
        ready_head = node;
        return;
    }
    RtosTask* t = &task_table[task_index];

    ReadyNode* prev = NULL;
    ReadyNode* cur = ready_head;
    while (cur) {
        RtosTask* c = &task_table[cur->task_index];
        if (t->priority < c->priority) break;
        if (t->priority == c->priority && t->id < c->id) break;
        prev = cur;
        cur = cur->next;
    }
    if (!prev) {
        node->next = ready_head;
        ready_head = node;
    } else {
        node->next = cur;
        prev->next = node;
    }
}

static int ready_pop_front(void) {
    if (!ready_head) return -1;
    ReadyNode* n = ready_head;
    int idx = n->task_index;
    ready_head = ready_head->next;
    free(n);
    return idx;
}

static int ready_queue_length(void) {
    int n = 0;
    ReadyNode* p = ready_head;
    while (p) { ++n; p = p->next; }
    return n;
}

static int higher_priority_available(int current_index) {
    if (!ready_head) return 0;
    if (current_index < 0) return 1;
    RtosTask* cur = &task_table[current_index];
    RtosTask* top = &task_table[ready_head->task_index];
    if (top->priority < cur->priority) return 1;
    if (top->priority == cur->priority && top->id < cur->id) return 1;
    return 0;
}

/* ---------- Public API ---------- */
void scheduler_init(int maxTasks) {
    /* cleanup previous state if any */
    while (ready_head) {
        ReadyNode* n = ready_head;
        ready_head = ready_head->next;
        free(n);
    }
    if (task_table) {
        free(task_table);
        task_table = NULL;
    }
    task_capacity = (maxTasks > 0) ? maxTasks : 0;
    task_count = 0;
    sim_time = 0;
    ready_head = NULL;
    if (task_capacity > 0) {
        task_table = (RtosTask*)calloc((size_t)task_capacity, sizeof(RtosTask));
    }
}

int scheduler_add_task(int id, int priority, int arrival_time, int burst_time) {
    if (!task_table || task_count >= task_capacity) return -1;
    if (burst_time <= 0) return -1;

    RtosTask* t = &task_table[task_count];
    t->id = id;
    t->priority = priority;
    t->arrival_time = arrival_time;
    t->burst_time = burst_time;
    t->remaining_time = burst_time;
    t->completion_time = -1;
    t->enqueued = 0;
    t->waiting_time = 0;
    return task_count++;
}

/* Enqueue all tasks that arrive at or before 'sim_time' and not yet enqueued */
static void enqueue_arrivals(void) {
    for (int i = 0; i < task_count; ++i) {
        RtosTask* t = &task_table[i];
        if (!t->enqueued && t->arrival_time <= sim_time && t->remaining_time > 0) {
            ready_push_sorted(i);
            t->enqueued = 1;
        }
    }
}

static int all_finished(void) {
    for (int i = 0; i < task_count; ++i) {
        if (task_table[i].remaining_time > 0) return 0;
    }
    return 1;
}

/* ---------- Optional AI priority application (file-based) ---------- */
static int last_applied_tick = -1;

static int apply_ai_priorities_for_tick(int current_tick) {
    FILE* f = fopen("new_priorities.csv", "r");
    if (!f) return 0;

    char line[256];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return 0; } // header

    int ai_tick, task_id, new_prio;
    int applied_any = 0, freshest_tick = -1;

    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "%d,%d,%d", &ai_tick, &task_id, &new_prio) == 3) {
            if (ai_tick == current_tick && ai_tick > last_applied_tick) {
                for (int i = 0; i < task_count; ++i) {
                    if (task_table[i].id == task_id) {
                        int old_prio = task_table[i].priority;
                        task_table[i].priority = new_prio;
                        AI_DPRINTF("[SCHED-DBG] tick=%d task=%d priority %d -> %d\n",
                                   current_tick, task_id, old_prio, new_prio);
                        (void)old_prio; // avoid unused warning if AI_DEBUG=0
                        applied_any = 1;
                        break;
                    }
                }
                if (ai_tick > freshest_tick) freshest_tick = ai_tick;
            } else {
                AI_DPRINTF("[SCHED-DBG] tick=%d ignoring AI row tick=%d task=%d\n",
                           current_tick, ai_tick, task_id);
            }
        }
    }
    fclose(f);
    if (applied_any && freshest_tick >= 0) last_applied_tick = freshest_tick;
    return applied_any;
}

void start_scheduler(void) {
    /* Open metrics file once */
    if (metrics_open("metrics.csv") != 0) {
        log_error("Failed to open metrics.csv (continuing without metrics)");
    }

    int current = -1;         /* index of running task, -1 if none */
    int current_quantum = 0;  /* cycles used in current dispatch */

    while (!all_finished()) {
        /* Step 1: enqueue arrivals for this tick */
        enqueue_arrivals();

        /* Step 2: optionally wait briefly for AI priorities for this tick */
        int waited_ms = 0;
        while (waited_ms < 100) {
            if (apply_ai_priorities_for_tick(sim_time)) break;
            // Sleep ~10ms if available (Windows Sleep(10) / POSIX usleep(10*1000))
            waited_ms += 10;
        }

        /* Step 3: (Re)select if needed using potentially updated priorities */
        if (current < 0 || higher_priority_available(current) || current_quantum >= QUANTUM) {
            if (current >= 0 && task_table[current].remaining_time > 0) {
                ready_push_sorted(current);
            }
            current = ready_pop_front();
            current_quantum = 0;
        }

        /* Step 4: run 1 cycle if we have a task; update waiting times as you already do */
        if (current >= 0) {
            RtosTask* t = &task_table[current];
            printf("Time %d: Running task %d (prio=%d, rem=%d)\n",
                   sim_time, t->id, t->priority, t->remaining_time);

            t->remaining_time -= 1;
            current_quantum += 1;

            /* Update waiting time for other ready tasks this tick */
            for (int i = 0; i < task_count; ++i) {
                if (i == current) continue;
                if (task_table[i].enqueued && task_table[i].remaining_time > 0) {
                    task_table[i].waiting_time += 1;
                }
            }

            if (t->remaining_time <= 0) {
                t->completion_time = sim_time + 1; /* completes at end of this tick */
                current = -1;                      /* drop current */
                current_quantum = 0;
            }
        } else {
            printf("Time %d: CPU idle\n", sim_time);
        }

        /* Step 5: log metrics for this tick */
        {
            int running_task_id = (current >= 0) ? task_table[current].id : -1;
            double cpu_usage = (running_task_id >= 0) ? 1.0 : 0.0;
            int ready_queue_len = ready_queue_length();
            metrics_log_tick(sim_time, task_table, task_count, running_task_id, ready_queue_len, cpu_usage);
        }

        /* Step 6: advance time */
        sim_time += 1;
    }

    /* Close metrics once at the end */
    metrics_close();
}

void scheduler_print_stats(void) {
    printf("\nTask statistics:\n");
    printf("ID\tArr\tBurst\tCompl\tTurn\tWait\n");
    for (int i = 0; i < task_count; ++i) {
        RtosTask* t = &task_table[i];
        int turn = t->completion_time - t->arrival_time;
        int wait = turn - t->burst_time;
        printf("%d\t%d\t%d\t%d\t%d\t%d\n",
               t->id, t->arrival_time, t->burst_time,
               t->completion_time, turn, wait);
    }
}

void scheduler_cleanup(void) {
    /* free ready queue nodes */
    while (ready_head) {
        ReadyNode* n = ready_head;
        ready_head = ready_head->next;
        free(n);
    }
    if (task_table) {
        free(task_table);
        task_table = NULL;
    }
    task_count = 0;
    task_capacity = 0;
    sim_time = 0;
}
