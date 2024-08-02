#include "respool.h"

#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
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
respool_add (respool_t *pool, const char *path)
{
  struct stat info;
  if (stat (path, &info) != 0)
    return NULL;

  resource_t *res;
  if (!(res = malloc (sizeof (resource_t))))
    return NULL;

  res->size = info.st_size;
  res->mtime = info.st_mtim;

  int fd = open (path, O_RDONLY);
  if ((res->fd = fd) == -1)
    goto clean_res;

  res->path = MSTR_INIT;
  if (!mstr_assign_cstr (&res->path, path))
    goto clean_fd;

  pthread_rwlock_wrlock (&pool->lock);
  bool ok = rbtree_insert (&pool->tree, &res->node, node_comp);
  pthread_rwlock_unlock (&pool->lock);

  if (!ok)
    goto clean_path;
  return res;

clean_path:
  mstr_free (&res->path);

clean_fd:
  close (fd);

clean_res:
  free (res);
  return NULL;
}

#define timespec_equal(a, b)                                                  \
  ((a).tv_sec == (b).tv_sec && (a).tv_nsec == (b).tv_nsec)

static inline resource_t *
resource_update (resource_t *res, const char *path, struct stat info)
{
  int nfd = open (path, O_RDONLY);
  if (nfd == -1)
    return NULL;

  close (res->fd);

  res->mtime = info.st_mtim;
  res->size = info.st_size;
  res->fd = nfd;

  return res;
}

resource_t *
respool_get (respool_t *pool, const char *path)
{
  resource_t target = { .path = MSTR_VIEW (path, strlen (path)) };

  pthread_rwlock_rdlock (&pool->lock);
  rbtree_node_t *node = rbtree_find (&pool->tree, &target.node, node_comp);
  pthread_rwlock_unlock (&pool->lock);

  if (!node)
    return respool_add (pool, path);

  struct stat info;
  if (stat (path, &info) != 0)
    return NULL;

  resource_t *res = container_of (node, resource_t, node);
  if (!timespec_equal (res->mtime, info.st_mtim))
    res = resource_update (res, path, info);

  return res;
}

static inline void
node_free (rbtree_node_t *n)
{
  resource_t *rn = container_of (n, resource_t, node);
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
