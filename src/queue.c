#include "public.h"
#include "queue.h"

typedef struct Element {
    struct Element *next;
    void *data;
    int size;
} Element;

typedef struct queue_t {
    Element             *pIn;
    Element             *pOut;
    Element             *pLast;
    int                 size;
    pthread_mutex_t     mutex;
    pthread_cond_t      cond;
} queue_t;

queue_t* new_queue()
{
    queue_t *q = ( queue_t *) malloc (sizeof(queue_t) );
    
    if ( !q ) {
        return NULL;
    }

    q->size = 0;
    q->pLast = q->pIn = q->pOut = NULL;
    pthread_mutex_init( &q->mutex, NULL );
    pthread_cond_init( &q->cond, NULL );

    return q;
}

int enqueue( queue_t *q, void *data, int size )
{
    Element *elem = NULL;

    if ( !q || !data ) {
        goto err;
    }

    pthread_mutex_lock( &q->mutex );
    elem = ( Element *) malloc ( sizeof(Element) );
    if ( !elem ) {
        goto err;
    }
    memset( elem, 0, sizeof(Element) );
    elem->data = malloc( size );
    if ( !elem->data ) {
        goto err1;
    }
    elem->size = size;
    memset( elem->data, 0, size );
    memcpy( elem->data, data, size );
    if ( q->size == 0 ) {
        q->pIn = q->pOut = elem;
    } else {
        q->pIn->next = elem;
        q->pIn = elem;
    }
    q->size ++;
    pthread_cond_signal( &q->cond );
    pthread_mutex_unlock( &q->mutex );

    return 0;

err:
    pthread_mutex_unlock( &q->mutex );
    return -1;

err1:
    pthread_mutex_unlock( &q->mutex );
    free( elem );
    return -1;
}

int dequeue( queue_t *q, void *data, int *outSize )
{
    Element *elem = NULL;

    if ( !q || !data  ) {
        goto err;
    }

    pthread_mutex_lock( &q->mutex );

    while ( q->size == 0 ) {
        pthread_cond_wait( &q->cond, &q->mutex );
    }

    if ( !q->pOut ) {
        goto err;
    }
    elem = q->pOut;
    memcpy( data, elem->data, elem->size );
    if ( outSize )
        *outSize = elem->size;
    if ( q->size > 1 ) {
        q->pOut = elem->next;
        q->pLast = elem;
    } else {
        q->pIn = q->pOut = NULL;
        free( elem->data );
        free( elem);
    }
    q->size--;
    if ( q->pLast ) {
        free( q->pLast->data );
        free( q->pLast );
        q->pLast = NULL;
    }

    pthread_mutex_unlock( &q->mutex );

    return 0;

err:
    return -1;
}

int queue_size( queue_t *q )
{
    return q->size;
}

