#include <stdio.h>
#include "rtos.h"

/* Task IDs */
#define PRODUCER_ID 0
#define CONSUMER_ID 1

/* Producer task */
static void producer(void) {
    static int next_value = 1;

    if (!check_timer(PRODUCER_ID))
        return;

    if (next_value <= 10) {
        send_message(next_value);
        log_task(PRODUCER_ID, "Producer: sent message");
        next_value++;

        set_timer(PRODUCER_ID, 3); // wait 3 ticks
    }
}

/* Consumer task */
static void consumer(void) {
    if (!check_timer(CONSUMER_ID))
        return;

    int msg = receive_message();
    if (msg != -1) {
        log_task(CONSUMER_ID, "Consumer: received message");
        set_timer(CONSUMER_ID, 3); // wait 3 ticks, aligned with producer
    }
    // For clean demo, do not log empty queue
}

/* Main function */
int main() {
    // Register tasks
    create_task(producer);
    create_task(consumer);

    // Initialize timers
    set_timer(PRODUCER_ID, 0); // Producer ready immediately
    set_timer(CONSUMER_ID, 3); // Start consumer after first producer

    // Run scheduler loop
    for (int tick_count = 0; tick_count < 30; tick_count++) {
        tick();           // Advance timers
        start_scheduler(); // Run all tasks
    }

    return 0;
}
