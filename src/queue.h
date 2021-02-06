#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

typedef struct queue_t queue_t;

typedef void *(*alloc_cb_t)(void *opaque, size_t *size);
extern queue_t *new_queue(size_t capacity, alloc_cb_t alloc_cb, void *opaque);
extern int queue_push(queue_t *queue, void *mem);
extern void * queue_pop(queue_t *queue);
extern int queue_size(queue_t *queue);
extern void *queue_peek(queue_t *queue);
extern void *queue_peek_next(queue_t *queue);
extern void *queue_peek_last(queue_t *queue);

#endif