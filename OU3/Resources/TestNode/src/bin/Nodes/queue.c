#include "queue.h"

// Helper function to resize the queue dynamically
static void queue_resize(queue_t* q) {
    int new_capacity = q->capacity * 2;
    void** new_items = realloc(q->items, new_capacity * sizeof(void*));
    if (new_items == NULL) {
        perror("realloc:");
        exit(EXIT_FAILURE);
    }

    q->items = new_items;
    q->capacity = new_capacity;
}

queue_t* queue_create(int capacity) {
    queue_t* q = (queue_t*)malloc(sizeof(queue_t));
    if (q == NULL) {
        perror("malloc:");
        exit(EXIT_FAILURE);
    }

    q->items = (void**)malloc(capacity * sizeof(void*));
    if (q->items == NULL) {
        perror("malloc:");
        exit(EXIT_FAILURE);
    }

    q->front = 0;
    q->rear = -1;
    q->size = 0;
    q->capacity = capacity;

    return q;
}

bool queue_is_empty(queue_t* q) {
    return q->size == 0;
}

void queue_enqueue(queue_t* q, void* item) {
    if (q->size == q->capacity) {
        queue_resize(q);
    }

    q->rear = (q->rear + 1) % q->capacity;
    q->items[q->rear] = item;
    q->size++;
}

void* queue_dequeue(queue_t* q) {
    if (queue_is_empty(q)) {
        fprintf(stderr, "Queue is empty\n");
        return NULL;
    }

    void* item = q->items[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size--;

    return item;
}

int queue_size(queue_t* q) {
    return q->size;
}
