#ifndef RESPOOL_H
#define RESPOOL_H

#include "mstr.h"
#include "rbtree.h"

#include <pthread.h>

typedef struct respool_t respool_t;
typedef struct resource_t resource_t;
typedef struct respool_node_t respool_node_t;

struct respool_t
{
  rbtree_t tree;
  pthread_rwlock_t lock;
};

struct resource_t
{
  int fd;
  void *data;
  size_t size;
  mstr_t path;
  rbtree_node_t node;
};

extern int respool_init (respool_t *pool);

extern void respool_free (respool_t *pool);

extern void respool_del (respool_t *pool, const char *path);

extern resource_t *respool_get (respool_t *pool, const char *path);

extern resource_t *respool_add (respool_t *pool, const char *path,
                                size_t size);

#endif
