#include "rtos.h"
#include <stdlib.h>

// TODO: handle synchronization in the future

typedef struct Message {
    int msg;
    struct Message* next;
} Message;

static Message* msg_queue = NULL;

void send_message(int msg) {
    // TODO: handle synchronization in the future
    Message* new_node = (Message*)malloc(sizeof(Message));
    if (new_node == NULL) {
        return;
    }
    new_node->msg = msg;
    new_node->next = NULL;

    if (msg_queue == NULL) {
        msg_queue = new_node;
        return;
    }

    Message* tail = msg_queue;
    while (tail->next != NULL) {
        tail = tail->next;
    }
    tail->next = new_node;
}

int receive_message() {
    // TODO: handle synchronization in the future
    if (msg_queue == NULL) {
        return -1;
    }

    Message* front = msg_queue;
    int value = front->msg;
    msg_queue = front->next;
    free(front);
    return value;
}
