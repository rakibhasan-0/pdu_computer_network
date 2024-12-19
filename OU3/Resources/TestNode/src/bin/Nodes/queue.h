#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>


typedef struct queue {
    void** items;
    int front;
    int rear;
    int size;
    int capacity;
} queue_t;


queue_t* queue_create(int capacity);
bool queue_is_empty(queue_t* q);
void queue_enqueue(queue_t* q, void* item);
void* queue_dequeue(queue_t* q);
int queue_size(queue_t* q);


#endif