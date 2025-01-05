#include "queue.h"

/**
 * @brief Resizes the queue by doubling its current capacity.
 *
 * @param q Pointer to the queue to resize.
 */
static void queue_resize(queue_t* q) {
    int new_capacity = q->capacity * 2;
    void** new_items = malloc(new_capacity * sizeof(void*));
    if (new_items == NULL) {
        perror("Failed to allocate memory during queue_resize");
        exit(EXIT_FAILURE);
    }

    // Initialize new items to NULL
    for (int i = 0; i < new_capacity; i++) {
        new_items[i] = NULL;
    }

    // Copy elements to the new buffer in correct order
    for (int i = 0; i < q->size; i++) {
        new_items[i] = q->items[(q->front + i) % q->capacity];
    }

    // Free the old buffer
    free(q->items);

    // Update queue properties
    q->items = new_items;
    q->front = 0;
    q->rear = q->size - 1;
    q->capacity = new_capacity;
}
/**
 * @brief Creates a new queue with the specified initial capacity.
 *
 * @param capacity Initial capacity of the queue.
 * @return Pointer to the newly created queue.
 */
queue_t* queue_create(int capacity) {
    if (capacity <= 0) {
        fprintf(stderr, "Queue capacity must be greater than 0.\n");
        exit(EXIT_FAILURE);
    }

    queue_t* q = malloc(sizeof(queue_t));
    if (q == NULL) {
        perror("Failed to allocate memory for queue");
        exit(EXIT_FAILURE);
    }

    q->items = malloc(capacity * sizeof(void*));
    if (q->items == NULL) {
        perror("Failed to allocate memory for queue items");
        free(q);
        exit(EXIT_FAILURE);
    }

    q->front = 0;
    q->rear = -1;
    q->size = 0;
    q->capacity = capacity;
    q->destroyed = 0;

    if (pthread_mutex_init(&q->lock, NULL) != 0) {
        perror("Failed to initialize mutex for queue");
        free(q->items);
        free(q);
        exit(EXIT_FAILURE);
    }

    // Initialize all items to NULL
    for (int i = 0; i < capacity; i++) {
        q->items[i] = NULL;
    }

    printf("Queue created with capacity: %d\n", capacity);
    return q;
}

/**
 * @brief Checks if the queue is empty.
 *
 * @param q Pointer to the queue.
 * @return true if the queue is empty, false otherwise.
 */
bool queue_is_empty(queue_t* q) {
    bool empty;
    //pthread_mutex_lock(&q->lock);
    empty = (q->size == 0);
    //pthread_mutex_unlock(&q->lock);
    return empty;
}

/**
 * @brief Enqueues an item to the queue.
 *
 * @param q Pointer to the queue.
 * @param item Pointer to the item to enqueue.
 */
void queue_enqueue(queue_t* q, void* item) {
    if (item == NULL) {
        fprintf(stderr, "Cannot enqueue NULL item.\n");
        return;
    }

    //pthread_mutex_lock(&q->lock);

    // Resize if the queue is full
    if (q->size == q->capacity) {
        queue_resize(q);
    }

    // Calculate the new rear position
    q->rear = (q->rear + 1) % q->capacity;
    q->items[q->rear] = item;
    q->size++;

    //pthread_mutex_unlock(&q->lock);
}

void clean_up_queue(queue_t* q){

   for(int i = 0; i < q->size; i++){
       free(q->items[i]);
   }
    
}

/**
 * @brief Dequeues an item from the queue.
 *
 * @param q Pointer to the queue.
 * @return Pointer to the dequeued item, or NULL if the queue is empty.
 */
void* queue_dequeue(queue_t* q) {
   // pthread_mutex_lock(&q->lock);

    if (q->size == 0) {
        fprintf(stderr, "Queue is empty. Cannot dequeue.\n");
        pthread_mutex_unlock(&q->lock);
        return NULL;
    }

    void* item = q->items[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->size--;

    //pthread_mutex_unlock(&q->lock);
    return item;
}

/**
 * @brief Retrieves the current size of the queue.
 *
 * @param q Pointer to the queue.
 * @return Current number of items in the queue.
 */
int queue_size(queue_t* q) {
    int size;
    //pthread_mutex_lock(&q->lock);
    size = q->size;
    //pthread_mutex_unlock(&q->lock);
    return size;
}

/**
 * @brief Destroys the queue and frees all associated resources.
 *
 * @param q Pointer to the queue.
 */
void queue_destroy(queue_t* q) {
    if (q == NULL || q->destroyed) {
        return;
    }

    q->destroyed = 1; // Set the flag to indicate the queue is destroyed

    printf("Destroying queue with capacity: %d, size: %d\n", q->capacity, q->size);

    // Free all items in the queue
    if (q->items != NULL) {
        /*for (int i = 0; i < q->capacity; i++) {
            if (q->items[i] != NULL) {
                //printf("Freeing item at index %d\n", i);
                //free(q->items[i]);
                q->items[i] = NULL; // Set to NULL after freeing
            }
        }*/
        printf("Freeing items array\n");
        free(q->items);
        q->items = NULL; // Set to NULL after freeing
    }

    pthread_mutex_destroy(&q->lock);
    printf("Freeing queue structure\n");
    free(q);
}

