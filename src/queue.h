#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

typedef struct queue_t queue_t;

extern queue_t* new_queue();
extern int enqueue( queue_t *q, void *data, int size );
extern int queue_size( queue_t *q );
extern int dequeue( queue_t *q, void *data, int *outSize );
extern int enqueue( queue_t *q, void *data, int size );

#endif