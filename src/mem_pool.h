#include "public.h"

typedef void * (*alloc_cb_t)(size_t blk_size, void *opaque);
typedef void * (*realloc_cb_t)(void *mem, size_t blk_size, void *opaque);
typedef int (*push_blk_cb_t)(void *src, void *mem, size_t blk_size, void *opaque);

extern mem_pool_t *new_mem_pool(size_t capacity,
                         size_t blk_size,
                         alloc_cb_t alloc_cb,
                         realloc_cb_t realloc_cb,
                         push_blk_cb_t push_blk_cb,
                         void *opaque);
extern int mem_pool_push_blk(mem_pool_t *mp, void *blk, size_t blk_size);
extern int mem_pool_pop_blk(mem_pool_t *mp, void **blk, size_t *size);