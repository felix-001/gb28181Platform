#include "public.h"
#include "mem_pool.h"

typedef struct {
    void    *mem;
    size_t  size;
} blk_t;

typedef struct mem_pool_t {
    size_t          capacity;
    size_t          size;
    int             read_idx;
    int             write_idx;
    realloc_cb_t    realloc_cb;
    push_blk_cb_t   push_blk_cb;
    void            *opaque;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    blk_t           blks[0];
} mem_pool_t;

mem_pool_t *new_mem_pool(size_t capacity,
                         size_t blk_size,
                         alloc_cb_t alloc_cb,
                         realloc_cb_t realloc_cb,
                         push_blk_cb_t push_blk_cb,
                         void *opaque)
{
    mem_pool_t *mp = (mem_pool_t *)malloc(sizeof(mem_pool_t) + sizeof(void *)*capacity);

    if (!mp) {
        LOGE("mem error");
        return NULL;
    }
    memset(mp, 0, sizeof(*mp));
    mp->capacity = capacity;
    mp->realloc_cb = realloc_cb;
    mp->push_blk_cb = push_blk_cb;
    mp->opaque = opaque;
    if (alloc_cb) {
        for (int i = 0; i < capacity; i++) {
            mp->blks[i].mem = alloc_cb(blk_size, opaque);
            if (!mp->blks[i].mem) {
                LOGE("alloc cb error");
                return NULL;
            }
        }
        return mp;
    }
    for (int i=0; i<capacity; i++) {
        mp->blks[i].mem = malloc(blk_size);
        mp->blks[i].size = blk_size;
    }
    pthread_mutex_init(&mp->mutex, NULL);
    pthread_cond_init(&mp->cond, NULL);
    return mp;
}

int mem_pool_push_blk(mem_pool_t *mp, void *blk, size_t blk_size)
{
    int ret = -1;

    pthread_mutex_lock(&mp->mutex);
    if (mp->size == mp->capacity) {
        LOGE("pool full");
        goto err;
    }
    blk_t *blk = &mp->blks[mp->write_idx];
    if (blk_size != blk->size) {
        if (!mp->realloc_cb) {
            LOGE("check realloc cb error");
            goto err;
        }
        blk->mem = mp->realloc_cb(blk->mem, blk_size, mp->opaque);
        if (!blk->mem) {
            LOGE("mem error");
            goto err;
        }
        blk->size = blk_size;
    }
    if (blk) {
        memcpy(blk->mem, blk, blk_size);
    } else {
        if (!mp->push_blk_cb) {
            LOGE("check push blk cb error");
            goto err;
        }
        if (mp->push_blk_cb(blk, blk->mem, blk_size, opaque) < 0) {
            LOGE("push blk cb error");
            goto err;
        }
    }
    if (++mp->write_idx == mp->capacity) {
        mp->write_idx = 0;
    }
    mp->size++;
    ret = 0;
    pthread_cond_signal(&mp->cond);

err:
    pthread_mutex_unlock(&mp->mutex);
    return ret;
}

int mem_pool_pop_blk(mem_pool_t *mp, void **blk, size_t *size)
{
    int ret = -1;

    if (!mp || !blk || !size)
        return -1;
    pthread_mutex_lock(&mp->mutex);
    while (!mp->size) {
        pthread_cond_wait(&mp->cond, &mp->mutex);
    }
    blk_t *blk = &mp->blks[mp->read_idx];
    if (!blk->mem) {
        LOGE("get mem error");
        goto err;
    }
    *blk = blk->mem;
    *size = blk->size;
    if (++mp->read_idx == mp->capacity)
        mp->read_idx = 0;
    mp->size--;
    ret = 0;

err:
    pthread_mutex_lock(&mp->mutex);
    return ret;
}