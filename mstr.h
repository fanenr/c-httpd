#ifndef MSTR_H
#define MSTR_H

#include <stdbool.h>
#include <stddef.h>

#define MSTR_FLG_SSO 1
#define MSTR_FLG_HEAP 0

#define MSTR_EXPAN_RATIO 2
#define MSTR_SSO_CAP (sizeof (mstr_heap_t) - 1)

#define attr_nonnull(...) __attribute__ ((nonnull (__VA_ARGS__)))

typedef union mstr_t mstr_t;
typedef struct mstr_sso_t mstr_sso_t;
typedef struct mstr_heap_t mstr_heap_t;

struct mstr_heap_t
{
  size_t cap;
  size_t len;
  char *data;
};

struct mstr_sso_t
{
  unsigned char flg : 1;
  unsigned char len : 7;
  char data[MSTR_SSO_CAP];
};

union mstr_t
{
  mstr_sso_t sso;
  mstr_heap_t heap;
};

#define MSTR_INIT                                                             \
  (mstr_t) { .sso.flg = MSTR_FLG_SSO }

#define mstr_is_sso(str) ((str)->sso.flg == MSTR_FLG_SSO)
#define mstr_is_heap(str) ((str)->sso.flg == MSTR_FLG_HEAP)

#define mstr_cap(str) (mstr_is_sso (str) ? MSTR_SSO_CAP : (str)->heap.cap)
#define mstr_len(str) (mstr_is_sso (str) ? (str)->sso.len : (str)->heap.len)
#define mstr_data(str) (mstr_is_sso (str) ? (str)->sso.data : (str)->heap.data)

#define mstr_at(str, pos) ((pos) < mstr_len (str) ? mstr_data (str)[pos] : -1)

extern void mstr_free (mstr_t *str) attr_nonnull (1);

extern void mstr_clear (mstr_t *str) attr_nonnull (1);

extern mstr_t *mstr_reserve (mstr_t *str, size_t cap) attr_nonnull (1);

/* remove & substr */

extern mstr_t *mstr_remove (mstr_t *str, size_t start, size_t n)
    attr_nonnull (1);

extern mstr_t *mstr_substr (mstr_t *save, const mstr_t *from, size_t start,
                            size_t n) attr_nonnull (1, 2);

/* is start with */

extern bool mstr_start_with_char (const mstr_t *str, char ch) attr_nonnull (1);

extern bool mstr_start_with_cstr (const mstr_t *str, const char *cstr)
    attr_nonnull (1, 2);

extern bool mstr_start_with_mstr (const mstr_t *str, const mstr_t *other)
    attr_nonnull (1, 2);

extern bool mstr_start_with_byte (const mstr_t *str, const void *src, size_t n)
    attr_nonnull (1, 2);

/* is end with */

extern bool mstr_end_with_char (const mstr_t *str, char ch) attr_nonnull (1);

extern bool mstr_end_with_cstr (const mstr_t *str, const char *cstr)
    attr_nonnull (1, 2);

extern bool mstr_end_with_mstr (const mstr_t *str, const mstr_t *other)
    attr_nonnull (1, 2);

extern bool mstr_end_with_byte (const mstr_t *str, const void *src,
                                size_t n) attr_nonnull (1, 2);

/* compare */

extern int mstr_cmp_char (const mstr_t *str, char ch) attr_nonnull (1);

extern int mstr_cmp_cstr (const mstr_t *str, const char *cstr)
    attr_nonnull (1, 2);

extern int mstr_cmp_mstr (const mstr_t *str, const mstr_t *other)
    attr_nonnull (1, 2);

extern int mstr_cmp_byte (const mstr_t *str, const void *src, size_t n)
    attr_nonnull (1, 2);

/* append */

extern mstr_t *mstr_cat_char (mstr_t *str, char ch) attr_nonnull (1);

extern mstr_t *mstr_cat_cstr (mstr_t *str, const char *cstr)
    attr_nonnull (1, 2);

extern mstr_t *mstr_cat_mstr (mstr_t *str, const mstr_t *other)
    attr_nonnull (1, 2);

extern mstr_t *mstr_cat_byte (mstr_t *str, const void *src, size_t n)
    attr_nonnull (1, 2);

/* insert */

extern mstr_t *mstr_insert_char (mstr_t *str, size_t pos, char ch)
    attr_nonnull (1);

extern mstr_t *mstr_insert_cstr (mstr_t *str, size_t pos, const char *cstr)
    attr_nonnull (1, 3);

extern mstr_t *mstr_insert_mstr (mstr_t *str, size_t pos, const mstr_t *other)
    attr_nonnull (1, 3);

extern mstr_t *mstr_insert_byte (mstr_t *str, size_t pos,
                                 const void *src, size_t n)
    attr_nonnull (1, 3);

/* assign */

extern mstr_t *mstr_assign_char (mstr_t *str, char ch) attr_nonnull (1);

extern mstr_t *mstr_assign_cstr (mstr_t *str, const char *cstr)
    attr_nonnull (1, 2);

extern mstr_t *mstr_assign_mstr (mstr_t *str, const mstr_t *other)
    attr_nonnull (1, 2);

extern mstr_t *mstr_assign_byte (mstr_t *str, const void *src, size_t n)
    attr_nonnull (1, 2);

#endif
