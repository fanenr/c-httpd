#include "rbtree.h"

#define likely(exp) __builtin_expect (!!(exp), 1)
#define unlikely(exp) __builtin_expect (!!(exp), 0)

#define IS_RED(NODE) ((NODE) ? (NODE)->color == RBTREE_RED : 0)
#define IS_BLACK(NODE) ((NODE) ? (NODE)->color == RBTREE_BLACK : 1)

static inline void
rotate_left (rbtree_t *tree, rbtree_node_t *node)
{
  rbtree_node_t *child = node->right;
  rbtree_node_t *parent = node->parent;

  if ((node->right = child->left))
    child->left->parent = node;

  if (parent)
    if (node == parent->left)
      parent->left = child;
    else
      parent->right = child;
  else
    tree->root = child;
  child->parent = parent;

  child->left = node;
  node->parent = child;
}

static inline void
rotate_right (rbtree_t *tree, rbtree_node_t *node)
{
  rbtree_node_t *child = node->left;
  rbtree_node_t *parent = node->parent;

  if ((node->left = child->right))
    child->right->parent = node;

  if (parent)
    if (node == parent->left)
      parent->left = child;
    else
      parent->right = child;
  else
    tree->root = child;
  child->parent = parent;

  child->right = node;
  node->parent = child;
}

void
rbtree_link (rbtree_t *tree, rbtree_node_t **inpos, rbtree_node_t *parent,
             rbtree_node_t *node)
{
  node->left = node->right = NULL;
  node->color = RBTREE_RED;
  node->parent = parent;
  *inpos = node;

  for (rbtree_node_t *curr = node; curr;)
    {
      if (IS_BLACK (parent))
        break;

      rbtree_node_t *gprnt = parent->parent;
      int curr_left = (curr == parent->left);
      int prnt_left = (parent == gprnt->left);
      rbtree_node_t *uncle = prnt_left ? gprnt->right : gprnt->left;

      if (IS_RED (uncle))
        {
          gprnt->color = RBTREE_RED;
          parent->color = uncle->color = RBTREE_BLACK;
          parent = gprnt->parent;
          curr = gprnt;
          continue;
        }

      if (prnt_left)
        {
          if (!curr_left)
            rotate_left (tree, parent);
          rotate_right (tree, gprnt);
        }
      else
        {
          if (curr_left)
            rotate_right (tree, parent);
          rotate_left (tree, gprnt);
        }

      gprnt->parent->color = RBTREE_BLACK;
      gprnt->color = RBTREE_RED;
      break;
    }

  tree->size++;
  tree->root->color = RBTREE_BLACK;
}

void
rbtree_erase (rbtree_t *tree, rbtree_node_t *node)
{
  rbtree_node_t **rmpos;
  rbtree_node_t *left = node->left;
  rbtree_node_t *right = node->right;
  rbtree_color_t color = node->color;
  rbtree_node_t *parent = node->parent;
  rbtree_node_t *child = left ? left : right;

  rmpos = parent ? (node == parent->left) ? &parent->left : &parent->right
                 : &tree->root;

  if (left && right)
    {
      rbtree_node_t *next = right;
      for (rbtree_node_t *temp; (temp = next->left);)
        next = temp;

      color = next->color;
      child = next->right;
      parent = next->parent;

      left->parent = next;
      right->parent = next;

      *next = *node;
      *rmpos = next;

      if (next == right)
        {
          parent = next;
          rmpos = &next->right;
        }
      else
        rmpos = &parent->left;
    }

  *rmpos = child;

  if (color == RBTREE_RED)
    goto ret;

  if (child)
    {
      child->color = RBTREE_BLACK;
      child->parent = parent;
      goto ret;
    }

  for (rbtree_node_t *curr = child; IS_BLACK (curr) && parent;
       parent = curr->parent)
    {
      int curr_left = (curr == parent->left);
      rbtree_node_t *bro = curr_left ? parent->right : parent->left;

      if (IS_RED (bro))
        {
          parent->color = RBTREE_RED;
          bro->color = RBTREE_BLACK;
          if (curr_left)
            {
              rotate_left (tree, parent);
              bro = parent->right;
            }
          else
            {
              rotate_right (tree, parent);
              bro = parent->left;
            }
        }

      if (IS_BLACK (bro->left) && IS_BLACK (bro->right))
        {
          bro->color = RBTREE_RED;
          if (IS_RED (parent))
            {
              parent->color = RBTREE_BLACK;
              break;
            }
          curr = parent;
          continue;
        }

      if (curr_left)
        {
          if (IS_BLACK (bro->right))
            {
              bro->left->color = RBTREE_BLACK;
              bro->color = RBTREE_RED;
              rotate_right (tree, bro);
              bro = parent->right;
            }
          rotate_left (tree, parent);
          bro->right->color = RBTREE_BLACK;
        }
      else
        {
          if (IS_BLACK (bro->left))
            {
              bro->right->color = RBTREE_BLACK;
              bro->color = RBTREE_RED;
              rotate_left (tree, bro);
              bro = parent->left;
            }
          rotate_right (tree, parent);
          bro->left->color = RBTREE_BLACK;
        }

      bro->color = parent->color;
      parent->color = RBTREE_BLACK;
      break;
    }

ret:
  tree->size--;
  if (tree->root)
    tree->root->color = RBTREE_BLACK;
}

/* **************************************************************** */
/*                               ext                                */
/* **************************************************************** */

rbtree_node_t *
rbtree_find (const rbtree_t *tree, const rbtree_node_t *target,
             rbtree_comp_t *comp)
{
  for (rbtree_node_t *curr = tree->root; curr;)
    {
      int comp_ret = comp (target, curr);

      if (comp_ret == 0)
        return curr;

      curr = comp_ret < 0 ? curr->left : curr->right;
    }

  return NULL;
}

rbtree_node_t *
rbtree_insert (rbtree_t *tree, rbtree_node_t *node, rbtree_comp_t *comp)
{
  int comp_ret = 0;
  rbtree_node_t *parent = NULL;

  for (rbtree_node_t *curr = tree->root; curr;)
    {
      comp_ret = comp (node, curr);

      if (unlikely (comp_ret == 0))
        return NULL;

      parent = curr;
      curr = comp_ret < 0 ? curr->left : curr->right;
    }

  rbtree_node_t **inpos
      = comp_ret ? comp_ret < 0 ? &parent->left : &parent->right : &tree->root;

  rbtree_link (tree, inpos, parent, node);

  return node;
}

static inline void
rbtree_for_each_impl (rbtree_node_t *node, rbtree_visit_t *visit)
{
  if (node)
    {
      rbtree_node_t *left = node->left;
      rbtree_node_t *right = node->right;

      rbtree_for_each_impl (left, visit);
      visit (node);
      rbtree_for_each_impl (right, visit);
    }
}

void
rbtree_visit (rbtree_t *tree, rbtree_visit_t *visit)
{
  rbtree_for_each_impl (tree->root, visit);
}
