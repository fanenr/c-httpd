#include "threadpool.h"

#include <stdlib.h>

static void *work (void *arg);

void
threadpool_free (threadpool_t *pool)
{
  threadpool_wait (pool);
  threadpool_quit (pool);

  size_t size = pool->size;
  pthread_t *threads = pool->threads;

  for (size_t i = 0; i < size; i++)
    pthread_join (threads[i], NULL);

  free (pool->threads);
  pthread_mutex_destroy (&pool->mtx);
  pthread_cond_destroy (&pool->cond);

  pool->size = 0;
  pool->threads = NULL;
}

void
threadpool_wait (threadpool_t *pool)
{
  __atomic_fetch_add (&pool->remain, 0, __ATOMIC_RELAXED);
  for (; __atomic_load_n (&pool->remain, __ATOMIC_RELAXED);)
    ;
}

void
threadpool_run (threadpool_t *pool)
{
  pthread_mutex_lock (&pool->mtx);
  pool->status = THREADPOOL_STS_RUN;
  pthread_mutex_unlock (&pool->mtx);
  pthread_cond_broadcast (&pool->cond);
}

void
threadpool_stop (threadpool_t *pool)
{
  pthread_mutex_lock (&pool->mtx);
  pool->status = THREADPOOL_STS_STOP;
  pthread_mutex_unlock (&pool->mtx);
  pthread_cond_broadcast (&pool->cond);
}

void
threadpool_quit (threadpool_t *pool)
{
  pthread_mutex_lock (&pool->mtx);
  pool->status = THREADPOOL_STS_QUIT;
  pthread_mutex_unlock (&pool->mtx);
  pthread_cond_broadcast (&pool->cond);
}

int
threadpool_init (threadpool_t *pool, size_t n)
{
  *pool = (threadpool_t){ .status = THREADPOOL_STS_RUN };

  if (0 != pthread_mutex_init (&pool->mtx, NULL))
    return THREADPOOL_ERR_MTX;
  if (0 != pthread_cond_init (&pool->cond, NULL))
    return THREADPOOL_ERR_CND;

  size_t init = 0;
  pthread_t *threads;
  n = n ?: THREADPOOL_THREADS;

  if (!(threads = malloc (n * sizeof (pthread_t))))
    return THREADPOOL_ERR_MALLOC;

  for (; init < n; init++)
    if (0 != pthread_create (&threads[init], NULL, work, pool))
      goto err;

  pool->threads = threads;
  pool->size = n;
  return 0;

err:
  for (size_t i = 0; i < init; i++)
    pthread_cancel (threads[i]);

  free (threads);

  return THREADPOOL_ERR_CREATE;
}

int
threadpool_post (threadpool_t *pool, threadpool_func_t *f, void *a)
{
  threadpool_task_t *task;

  if (!(task = malloc (sizeof (threadpool_task_t))))
    return THREADPOOL_ERR_MALLOC;

  task->arg = a;
  task->func = f;
  task->next = NULL;

  pthread_mutex_lock (&pool->mtx);

  if (pool->status == THREADPOOL_STS_QUIT)
    {
      pthread_mutex_unlock (&pool->mtx);
      free (task);
      return THREADPOOL_ERR_QUITTED;
    }

  if (pool->tail)
    pool->tail->next = task;
  else
    pool->head = task;
  pool->tail = task;

  pthread_mutex_unlock (&pool->mtx);

  __atomic_fetch_add (&pool->remain, 1, __ATOMIC_RELAXED);
  pthread_cond_signal (&pool->cond);
  return 0;
}

static void *
work (void *arg)
{
  threadpool_t *pool = arg;

  for (;;)
    {
      pthread_mutex_lock (&pool->mtx);

      for (; (pool->status == THREADPOOL_STS_RUN && !pool->head)
             || pool->status == THREADPOOL_STS_STOP;)
        pthread_cond_wait (&pool->cond, &pool->mtx);

      if (pool->status == THREADPOOL_STS_QUIT)
        {
          pthread_mutex_unlock (&pool->mtx);
          break;
        }

      threadpool_task_t *task = pool->head;
      if (!(pool->head = task->next))
        pool->tail = NULL;

      pthread_mutex_unlock (&pool->mtx);

      task->func (task->arg);
      free (task);

      __atomic_fetch_sub (&pool->remain, 1, __ATOMIC_RELAXED);
    }

  return NULL;
}
