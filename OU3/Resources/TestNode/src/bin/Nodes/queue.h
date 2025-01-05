#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

/**
 * @brief A thread-safe circular queue implementation.
 *
 * @details The queue uses a dynamically allocated array to store items.
 * It maintains indices for the front and rear to support FIFO operations.
 * The queue automatically resizes when it reaches capacity.
 */
typedef struct queue {
    void** items;             ///< Array of pointers to queue items.
    int front;                ///< Index of the front item.
    int rear;                 ///< Index of the rear item.
    int size;                 ///< Current number of items in the queue.
    int capacity;             ///< Current capacity of the queue.
    pthread_mutex_t lock;     ///< Mutex for thread-safe operations.
	int destroyed;            ///< Flag to indicate if the queue has been destroyed.
} queue_t;

/**
 * @brief Creates a new queue with the specified initial capacity.
 *
 * @param capacity Initial capacity of the queue.
 * @return Pointer to the newly created queue.
 */
queue_t* queue_create(int capacity);

/**
 * @brief Checks if the queue is empty.
 *
 * @param q Pointer to the queue.
 * @return true if the queue is empty, false otherwise.
 */
bool queue_is_empty(queue_t* q);

/**
 * @brief Enqueues an item to the queue.
 *
 * @param q Pointer to the queue.
 * @param item Pointer to the item to enqueue.
 */
void queue_enqueue(queue_t* q, void* item);

/**
 * @brief Dequeues an item from the queue.
 *
 * @param q Pointer to the queue.
 * @return Pointer to the dequeued item, or NULL if the queue is empty.
 */
void* queue_dequeue(queue_t* q);

/**
 * @brief Retrieves the current size of the queue.
 *
 * @param q Pointer to the queue.
 * @return Current number of items in the queue.
 */
int queue_size(queue_t* q);

/**
 * @brief Destroys the queue and frees all associated resources.
 *
 * @param q Pointer to the queue.
 */
void queue_destroy(queue_t* q);

#endif // QUEUE_H
