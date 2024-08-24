#include "threadpool.h"

#include <stdlib.h>

static void *work (void *arg);
static void init_clean (threadpool_t *pool, size_t init);

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
  for (; __atomic_load_n (&pool->remain, __ATOMIC_RELAXED);)
    ;
}

int
threadpool_run (threadpool_t *pool)
{
  int ret = 0;

  pthread_mutex_lock (&pool->mtx);
  switch (pool->status)
    {
    case THREADPOOL_STS_STOP:
      pool->status = THREADPOOL_STS_RUN;
      break;

    case THREADPOOL_STS_QUIT:
      ret = THREADPOOL_ERR_QUITTED;
      break;
    }
  pthread_mutex_unlock (&pool->mtx);

  pthread_cond_broadcast (&pool->cond);

  return ret;
}

int
threadpool_stop (threadpool_t *pool)
{
  int ret = 0;

  pthread_mutex_lock (&pool->mtx);
  switch (pool->status)
    {
    case THREADPOOL_STS_RUN:
      pool->status = THREADPOOL_STS_STOP;
      break;

    case THREADPOOL_STS_QUIT:
      ret = THREADPOOL_ERR_QUITTED;
      break;
    }
  pthread_mutex_unlock (&pool->mtx);

  return ret;
}

void
threadpool_quit (threadpool_t *pool)
{
  pthread_mutex_lock (&pool->mtx);
  pool->status = THREADPOOL_STS_QUIT;
  pthread_mutex_unlock (&pool->mtx);

  pthread_cond_broadcast (&pool->cond);
}

static inline void
init_clean (threadpool_t *pool, size_t init)
{
  pthread_t *threads = pool->threads;

  pthread_mutex_destroy (&pool->mtx);
  pthread_cond_destroy (&pool->cond);

  for (size_t i = 0; i < init; i++)
    pthread_cancel (threads[i]);
  free (threads);
}

int
threadpool_init (threadpool_t *pool, size_t n)
{
  *pool = (threadpool_t){ .status = THREADPOOL_STS_RUN };

  if (0 != pthread_mutex_init (&pool->mtx, NULL))
    return THREADPOOL_ERR_MTX;
  if (0 != pthread_cond_init (&pool->cond, NULL))
    return THREADPOOL_ERR_CND;

  pthread_t *threads;
  n = n ?: THREADPOOL_THREADS;

  if (!(threads = malloc (n * sizeof (pthread_t))))
    return THREADPOOL_ERR_MALLOC;

  for (size_t init = 0; init < n; init++)
    if (pthread_create (&threads[init], NULL, work, pool))
      return (init_clean (pool, init), THREADPOOL_ERR_CREATE);

  pool->threads = threads;
  pool->size = n;
  return 0;
}

int
threadpool_post (threadpool_t *pool, threadpool_func_t *f, void *a)
{
  int ret = 0, status;

  threadpool_task_t *task;
  if (!(task = malloc (sizeof (threadpool_task_t))))
    return THREADPOOL_ERR_MALLOC;

  task->arg = a;
  task->func = f;
  task->next = NULL;

  pthread_mutex_lock (&pool->mtx);
  if ((status = pool->status) != THREADPOOL_STS_QUIT)
    {
      if (pool->tail)
        pool->tail->next = task;
      else
        pool->head = task;
      pool->tail = task;
    }
  pthread_mutex_unlock (&pool->mtx);

  switch (status)
    {
    case THREADPOOL_STS_RUN:
    case THREADPOOL_STS_STOP:
      __atomic_fetch_add (&pool->remain, 1, __ATOMIC_RELAXED);
      pthread_cond_signal (&pool->cond);
      break;

    case THREADPOOL_STS_QUIT:
      ret = THREADPOOL_ERR_QUITTED;
      free (task);
      break;
    }

  return ret;
}

static void *
work (void *arg)
{
  threadpool_t *pool = arg;

  for (threadpool_task_t *task;; free (task))
    {
      int status;

      pthread_mutex_lock (&pool->mtx);
      for (; (pool->status == THREADPOOL_STS_RUN && !pool->head)
             || pool->status == THREADPOOL_STS_STOP;)
        pthread_cond_wait (&pool->cond, &pool->mtx);

      if ((status = pool->status) != THREADPOOL_STS_QUIT)
        {
          task = pool->head;
          if (!(pool->head = task->next))
            pool->tail = NULL;
        }
      pthread_mutex_unlock (&pool->mtx);

      if (status == THREADPOOL_STS_QUIT)
        break;

      task->func (task->arg);
      __atomic_fetch_sub (&pool->remain, 1, __ATOMIC_RELAXED);
    }

  return NULL;
}
