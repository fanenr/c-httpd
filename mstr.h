#ifndef MSTR_H
#define MSTR_H

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define MSTR_FLG_SSO 0
#define MSTR_FLG_HEAP 1

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
  (mstr_t) {}

#define MSTR_VIEW(src, size)                                                  \
  (mstr_t) { .heap = { .data = (char *) (src), .len = (size), .cap = 1 }, }

#define mstr_is_sso(str) ((str)->sso.flg == MSTR_FLG_SSO)
#define mstr_is_heap(str) ((str)->sso.flg == MSTR_FLG_HEAP)

#define mstr_cap(str) (mstr_is_sso (str) ? MSTR_SSO_CAP : (str)->heap.cap)
#define mstr_len(str) (mstr_is_sso (str) ? (str)->sso.len : (str)->heap.len)
#define mstr_data(str) (mstr_is_sso (str) ? (str)->sso.data : (str)->heap.data)

#define mstr_at(str, pos) ((pos) < mstr_len (str) ? mstr_data (str)[pos] : -1)

/* free */

extern void mstr_free (mstr_t *str) attr_nonnull (1);

/* clear */

extern void mstr_clear (mstr_t *str) attr_nonnull (1);

/* reserve */

extern mstr_t *mstr_reserve (mstr_t *str, size_t cap) attr_nonnull (1);

/* remove */

extern mstr_t *mstr_remove (mstr_t *str, size_t start, size_t n)
    attr_nonnull (1);

/* substr */

extern mstr_t *mstr_substr (mstr_t *save, const mstr_t *from, size_t start,
			    size_t n) attr_nonnull (1, 2);

/* format */

extern mstr_t *mstr_format (mstr_t *str, const char *fmt, ...)
    attr_nonnull (1, 2) __attribute__ ((format (printf, 2, 3)));

/* trim */

extern void mstr_trim (mstr_t *str, const char *s) attr_nonnull (1);

/* is start with */

extern bool mstr_start_with_byte (const mstr_t *str, const void *src, size_t n)
    attr_nonnull (1, 2);

static inline bool
mstr_start_with_char (const mstr_t *str, char ch)
{
  return mstr_start_with_byte (str, &ch, 1);
}

static inline bool
mstr_start_with_cstr (const mstr_t *str, const char *cstr)
{
  return mstr_start_with_byte (str, cstr, strlen (cstr));
}

static inline bool
mstr_start_with_mstr (const mstr_t *str, const mstr_t *other)
{
  return mstr_start_with_byte (str, mstr_data (other), mstr_len (other));
}

/* is end with */

extern bool mstr_end_with_byte (const mstr_t *str, const void *src, size_t n)
    attr_nonnull (1, 2);

static inline bool
mstr_end_with_char (const mstr_t *str, char ch)
{
  return mstr_end_with_byte (str, &ch, 1);
}

static inline bool
mstr_end_with_cstr (const mstr_t *str, const char *cstr)
{
  return mstr_end_with_byte (str, cstr, strlen (cstr));
}

static inline bool
mstr_end_with_mstr (const mstr_t *str, const mstr_t *other)
{
  return mstr_end_with_byte (str, mstr_data (other), mstr_len (other));
}

/* compare */

extern int mstr_cmp_byte (const mstr_t *str, const void *src, size_t n)
    attr_nonnull (1, 2);

static inline int
mstr_cmp_char (const mstr_t *str, char ch)
{
  return mstr_cmp_byte (str, &ch, 1);
}

static inline int
mstr_cmp_cstr (const mstr_t *str, const char *cstr)
{
  return mstr_cmp_byte (str, cstr, strlen (cstr));
}

static inline int
mstr_cmp_mstr (const mstr_t *str, const mstr_t *other)
{
  return mstr_cmp_byte (str, mstr_data (other), mstr_len (other));
}

/* icompare */

extern int mstr_icmp_byte (const mstr_t *str, const void *src, size_t n)
    attr_nonnull (1, 2);

static inline int
mstr_icmp_char (const mstr_t *str, char ch)
{
  return mstr_icmp_byte (str, &ch, 1);
}

static inline int
mstr_icmp_cstr (const mstr_t *str, const char *cstr)
{
  return mstr_icmp_byte (str, cstr, strlen (cstr));
}

static inline int
mstr_icmp_mstr (const mstr_t *str, const mstr_t *other)
{
  return mstr_icmp_byte (str, mstr_data (other), mstr_len (other));
}

/* append */

extern mstr_t *mstr_cat_byte (mstr_t *str, const void *src, size_t n)
    attr_nonnull (1, 2);

static inline mstr_t *
mstr_cat_char (mstr_t *str, char ch)
{
  return mstr_cat_byte (str, &ch, 1);
}

static inline mstr_t *
mstr_cat_cstr (mstr_t *str, const char *cstr)
{
  return mstr_cat_byte (str, cstr, strlen (cstr));
}

static inline mstr_t *
mstr_cat_mstr (mstr_t *str, const mstr_t *other)
{
  return mstr_cat_byte (str, mstr_data (other), mstr_len (other));
}

/* insert */

extern mstr_t *mstr_insert_byte (mstr_t *str, size_t pos, const void *src,
				 size_t n) attr_nonnull (1, 3);

static inline mstr_t *
mstr_insert_char (mstr_t *str, size_t pos, char ch)
{
  return mstr_insert_byte (str, pos, &ch, 1);
}

static inline mstr_t *
mstr_insert_cstr (mstr_t *str, size_t pos, const char *cstr)
{
  return mstr_insert_byte (str, pos, cstr, strlen (cstr));
}

static inline mstr_t *
mstr_insert_mstr (mstr_t *str, size_t pos, const mstr_t *other)
{
  return mstr_insert_byte (str, pos, mstr_data (other), mstr_len (other));
}

/* assign */

extern mstr_t *mstr_assign_byte (mstr_t *str, const void *src, size_t n)
    attr_nonnull (1, 2);

static inline mstr_t *
mstr_assign_char (mstr_t *str, char ch)
{
  return mstr_assign_byte (str, &ch, 1);
}

static inline mstr_t *
mstr_assign_cstr (mstr_t *str, const char *cstr)
{
  return mstr_assign_byte (str, cstr, strlen (cstr));
}

static inline mstr_t *
mstr_assign_mstr (mstr_t *str, const mstr_t *other)
{
  return mstr_assign_byte (str, mstr_data (other), mstr_len (other));
}

#endif
