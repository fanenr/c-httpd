#include "respool.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static void node_free (rbtree_node_t *n);
static int node_comp (const rbtree_node_t *a, const rbtree_node_t *b);

int
respool_init (respool_t *pool)
{
  pool->tree = RBTREE_INIT;
  return pthread_rwlock_init (&pool->lock, NULL);
}

void
respool_free (respool_t *pool)
{
  rbtree_visit (&pool->tree, node_free);
  pthread_rwlock_destroy (&pool->lock);
}

void
respool_del (respool_t *pool, const char *path)
{
  rbtree_node_t *node;
  resource_t target = { .path = MSTR_VIEW (path, strlen (path)) };

  pthread_rwlock_wrlock (&pool->lock);
  if ((node = rbtree_find (&pool->tree, &target.node, node_comp)))
    rbtree_erase (&pool->tree, node);
  pthread_rwlock_unlock (&pool->lock);

  node_free (node);
}

resource_t *
respool_add (respool_t *pool, const char *path, size_t size)
{
  resource_t *res;
  if (!(res = malloc (sizeof (resource_t))))
    return NULL;

  if (!(res->size = size))
    goto clean_new;

  int fd = open (path, O_RDONLY);
  if ((res->fd = fd) == -1)
    goto clean_new;

  void *data = mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if ((res->data = data) == MAP_FAILED)
    goto clean_fd;

  res->path = MSTR_INIT;
  if (!mstr_assign_cstr (&res->path, path))
    goto clean_data;

  bool ok;
  pthread_rwlock_wrlock (&pool->lock);
  ok = rbtree_insert (&pool->tree, &res->node, node_comp);
  pthread_rwlock_unlock (&pool->lock);

  if (!ok)
    goto clean_path;
  return res;

clean_path:
  mstr_free (&res->path);

clean_data:
  munmap (data, size);

clean_fd:
  close (fd);

clean_new:
  free (res);

  return NULL;
}

resource_t *
respool_get (respool_t *pool, const char *path)
{
  rbtree_node_t *node;
  resource_t target = { .path = MSTR_VIEW (path, strlen (path)) };

  pthread_rwlock_rdlock (&pool->lock);
  node = rbtree_find (&pool->tree, &target.node, node_comp);
  pthread_rwlock_unlock (&pool->lock);

  return node ? container_of (node, resource_t, node) : NULL;
}

static inline void
node_free (rbtree_node_t *n)
{
  resource_t *rn = container_of (n, resource_t, node);
  munmap (rn->data, rn->size);
  mstr_free (&rn->path);
  close (rn->fd);
  free (rn);
}

static inline int
node_comp (const rbtree_node_t *a, const rbtree_node_t *b)
{
  resource_t *ra = container_of (a, resource_t, node);
  resource_t *rb = container_of (b, resource_t, node);
  return mstr_cmp_mstr (&ra->path, &rb->path);
}
