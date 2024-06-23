#ifndef RBTREE_H
#define RBTREE_H

#include <stddef.h>

#define container_of(ptr, type, member)                                       \
  ((type *)((void *)(ptr) - offsetof (type, member)))

#define attr_nonnull(...) __attribute__ ((nonnull (__VA_ARGS__)))

typedef int rbtree_color_t;
typedef struct rbtree_t rbtree_t;
typedef struct rbtree_node_t rbtree_node_t;

#define RBTREE_RED 0
#define RBTREE_BLACK 1

struct rbtree_node_t
{
  rbtree_node_t *left;
  rbtree_node_t *right;
  rbtree_node_t *parent;
  rbtree_color_t color;
};

struct rbtree_t
{
  size_t size;
  rbtree_node_t *root;
};

#define RBTREE_INIT                                                           \
  (rbtree_t) {}

extern void rbtree_link (rbtree_t *tree, rbtree_node_t **inpos,
                         rbtree_node_t *parent, rbtree_node_t *node)
    attr_nonnull (1, 2, 4);

extern void rbtree_erase (rbtree_t *tree, rbtree_node_t *node)
    attr_nonnull (1, 2);

extern rbtree_node_t *rbtree_last (const rbtree_t *tree) attr_nonnull (1);

extern rbtree_node_t *rbtree_first (const rbtree_t *tree) attr_nonnull (1);

extern rbtree_node_t *rbtree_next (const rbtree_node_t *node) attr_nonnull (1);

extern rbtree_node_t *rbtree_prev (const rbtree_node_t *node) attr_nonnull (1);

/* **************************************************************** */
/*                               ext                                */
/* **************************************************************** */

typedef void rbtree_visit_t (rbtree_node_t *n);

typedef int rbtree_comp_t (const rbtree_node_t *a, const rbtree_node_t *b);

extern rbtree_node_t *rbtree_find (const rbtree_t *tree,
                                   const rbtree_node_t *target,
                                   rbtree_comp_t *comp) attr_nonnull (1, 2, 3);

extern rbtree_node_t *rbtree_insert (rbtree_t *tree, rbtree_node_t *node,
                                     rbtree_comp_t *comp)
    attr_nonnull (1, 2, 3);

extern void rbtree_visit (rbtree_t *tree, rbtree_visit_t *func)
    attr_nonnull (1, 2);

#endif
