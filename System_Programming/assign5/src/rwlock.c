/*--------------------------------------------------------------------*/
/* rwlock.c                                                           */
/* Author: Junghan Yoon, KyoungSoo Park                               */
/* Modified by: Jaeun Park                                            */
/*--------------------------------------------------------------------*/
#include "rwlock.h"
/*--------------------------------------------------------------------*/
struct uctx
{
    pthread_cond_t read_cv;
    pthread_cond_t write_cv;
    int waiting_writers;
};
/*--------------------------------------------------------------------*/
int rwlock_init(rwlock_t *rw, int delay)
{
    TRACE_PRINT();
    /*--------------------------------------------------------------------*/
    if (!rw)
    {
        errno = EINVAL;
        return -1;
    }

    if (pthread_mutex_init(&rw->lock, NULL) != 0)
    {
        return -1;
    }

    rw->current_readers = 0;
    rw->current_writers = 0;
    rw->delay = delay;

    rw->uctx = malloc(sizeof(struct uctx));
    if (!rw->uctx)
    {
        pthread_mutex_destroy(&rw->lock);
        return -1;
    }

    struct uctx *ctx = (struct uctx *)rw->uctx;
    pthread_cond_init(&ctx->read_cv, NULL);
    pthread_cond_init(&ctx->write_cv, NULL);
    ctx->waiting_writers = 0;
    /*--------------------------------------------------------------------*/
    return 0;
}
/*--------------------------------------------------------------------*/
int rwlock_read_lock(rwlock_t *rw, int quick)
{
    TRACE_PRINT();
    /*--------------------------------------------------------------------*/
    if (!rw)
    {
        errno = EINVAL;
        return -1;
    }

    struct uctx *ctx = (struct uctx *)rw->uctx;
    pthread_mutex_lock(&rw->lock);

    if (quick)
    {
        // quick read: writer만 없으면 즉시 진입
        while (rw->current_writers > 0)
        {
            pthread_cond_wait(&ctx->read_cv, &rw->lock);
        }
    }
    else
    {
        // 일반 read: FIFO (대기 writer가 있으면 대기)
        while (rw->current_writers > 0 || ctx->waiting_writers > 0)
        {
            pthread_cond_wait(&ctx->read_cv, &rw->lock);
        }
    }

    rw->current_readers++;
    pthread_mutex_unlock(&rw->lock);
    /*--------------------------------------------------------------------*/
    return 0;
}
/*--------------------------------------------------------------------*/
int rwlock_read_unlock(rwlock_t *rw)
{
    TRACE_PRINT();
    if (!rw)
    {
        errno = EINVAL;
        return -1;
    }
    sleep(rw->delay);
    /*--------------------------------------------------------------------*/
    struct uctx *ctx = (struct uctx *)rw->uctx;
    pthread_mutex_lock(&rw->lock);

    rw->current_readers--;

    // 마지막 reader면 대기 중인 writer 깨우기
    if (rw->current_readers == 0 && ctx->waiting_writers > 0)
    {
        pthread_cond_signal(&ctx->write_cv);
    }

    pthread_mutex_unlock(&rw->lock);
    /*--------------------------------------------------------------------*/
    return 0;
}
/*--------------------------------------------------------------------*/
int rwlock_write_lock(rwlock_t *rw)
{
    TRACE_PRINT();
    /*--------------------------------------------------------------------*/
    if (!rw)
    {
        errno = EINVAL;
        return -1;
    }

    struct uctx *ctx = (struct uctx *)rw->uctx;
    pthread_mutex_lock(&rw->lock);

    ctx->waiting_writers++;
    while (rw->current_readers > 0 || rw->current_writers > 0)
    {
        pthread_cond_wait(&ctx->write_cv, &rw->lock);
    }
    ctx->waiting_writers--;

    rw->current_writers++;
    pthread_mutex_unlock(&rw->lock);
    /*--------------------------------------------------------------------*/
    return 0;
}
/*--------------------------------------------------------------------*/
int rwlock_write_unlock(rwlock_t *rw)
{
    TRACE_PRINT();
    if (!rw)
    {
        errno = EINVAL;
        return -1;
    }
    sleep(rw->delay);
    /*--------------------------------------------------------------------*/
    struct uctx *ctx = (struct uctx *)rw->uctx;
    pthread_mutex_lock(&rw->lock);

    rw->current_writers--;

    // writer 우선, 없으면 reader들 깨우기
    if (ctx->waiting_writers > 0)
    {
        pthread_cond_signal(&ctx->write_cv);
    }
    else
    {
        pthread_cond_broadcast(&ctx->read_cv);
    }

    pthread_mutex_unlock(&rw->lock);
    /*--------------------------------------------------------------------*/
    return 0;
}
/*--------------------------------------------------------------------*/
int rwlock_destroy(rwlock_t *rw)
{
    TRACE_PRINT();
    /*--------------------------------------------------------------------*/
    if (!rw)
    {
        errno = EINVAL;
        return -1;
    }

    struct uctx *ctx = (struct uctx *)rw->uctx;
    pthread_cond_destroy(&ctx->read_cv);
    pthread_cond_destroy(&ctx->write_cv);
    free(ctx);
    pthread_mutex_destroy(&rw->lock);
    /*--------------------------------------------------------------------*/

    return 0;
}
