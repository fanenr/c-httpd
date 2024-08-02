#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <pthread.h>

#define THREADPOOL_THREADS 4

#define attr_nonnull(...) __attribute__ ((nonnull (__VA_ARGS__)))

enum
{
  THREADPOOL_STS_RUN,
  THREADPOOL_STS_STOP,
  THREADPOOL_STS_QUIT,
};

enum
{
  THREADPOOL_OK,
  THREADPOOL_ERR_MTX,
  THREADPOOL_ERR_CND,
  THREADPOOL_ERR_CREATE,
  THREADPOOL_ERR_MALLOC,
  THREADPOOL_ERR_QUITTED,
};

typedef struct threadpool_t threadpool_t;
typedef void threadpool_func_t (void *arg);
typedef struct threadpool_task_t threadpool_task_t;

struct threadpool_t
{
  int status;

  size_t size;
  pthread_t *threads;

  size_t remain;
  threadpool_task_t *head;
  threadpool_task_t *tail;

  pthread_cond_t cond;
  pthread_mutex_t mtx;
};

struct threadpool_task_t
{
  void *arg;
  threadpool_func_t *func;
  threadpool_task_t *next;
};

extern void threadpool_free (threadpool_t *pool) attr_nonnull (1);
extern void threadpool_wait (threadpool_t *pool) attr_nonnull (1);

extern int threadpool_run (threadpool_t *pool) attr_nonnull (1);
extern int threadpool_stop (threadpool_t *pool) attr_nonnull (1);
extern void threadpool_quit (threadpool_t *pool) attr_nonnull (1);

extern int threadpool_init (threadpool_t *pool, size_t n) attr_nonnull (1);
extern int threadpool_post (threadpool_t *pool, threadpool_func_t *f, void *a)
    attr_nonnull (1, 2);

#endif
