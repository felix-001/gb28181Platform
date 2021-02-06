#include "public.h"
#include "mem_pool.h"

typedef struct {
    void *mem;
    size_t size;
} blk_t;

typedef struct queue_t {
    size_t          capacity;
    size_t          size;
    int             read_idx;
    int             write_idx;
    void            *opaque;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    blk_t            blks[0];
} queue_t;

queue_t *new_queue(size_t capacity, alloc_cb_t alloc_cb, void *opaque)
{
    if (!alloc_cb) {
        LOGE("check alloc_cb error");
        return NULL;
    }
    queue_t *queue = (queue_t *)malloc(sizeof(queue_t) + sizeof(blk_t)*capacity);

    if (!queue) {
        LOGE("mem error");
        return NULL;
    }
    memset(queue, 0, sizeof(*queue));
    queue->capacity = capacity;
    queue->opaque = opaque;
    for (int i = 0; i < capacity; i++) {
        size_t size = 0;
        queue->blks[i].mem = alloc_cb(opaque, &size);
        if (!queue->blks[i].mem) {
            LOGE("alloc cb error");
            return NULL;
        }
        queue->blks[i].size = size;
    }
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->cond, NULL);
    return queue;
}

int queue_push(queue_t *queue, void *mem)
{
    pthread_mutex_lock(&queue->mutex);
    while (queue->size >= queue->capacity) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    blk_t *blk = &queue->blks[queue->write_idx];
    memcpy(blk->mem, mem, blk->size);
    if (++queue->write_idx == queue->capacity)
        queue->write_idx = 0;
    queue->size++;
    pthread_cond_signal(&queue->cond);
    pthread_mutex_unlock(&queue->mutex);
    return 0;
}

void * queue_pop(queue_t *queue)
{
    pthread_mutex_lock(&queue->mutex);
    while (queue->size <= 0) {
        pthread_cond_wait(&queue->cond, &queue->mutex);
    }
    blk_t *blk = &queue->blks[queue->write_idx];
    if (++queue->read_idx == queue->capacity)
        queue->read_idx = 0;
    queue->size--;
    SDL_CondSignal(&queue->cond);
    SDL_UnlockMutex(&queue->mutex);
    pthread_mutex_unlock(&queue->mutex);
    return blk->mem;
}

int queue_size(queue_t *queue)
{
    return queue->size;
}

void *queue_peek(queue_t *queue)
{
    return queue->blks[queue->read_idx].mem;
}

void *queue_peek_next(queue_t *queue)
{
    int idx = (queue->read_idx + 1) % queue->capacity;
    return queue->blks[idx].mem;
}

void *queue_peek_last(queue_t *queue)
{
    int idx = queue->read_idx > 0 ? queue->read_idx - 1 : queue->capacity - 1; 
    return queue->blks[idx].mem;
}