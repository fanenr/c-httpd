#include "mstr.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define set_len(str, new)                                                     \
  do                                                                          \
    if (mstr_is_sso (str))                                                    \
      {                                                                       \
        (str)->sso.len = new;                                                 \
        (str)->sso.data[new] = 0;                                             \
      }                                                                       \
    else                                                                      \
      {                                                                       \
        (str)->heap.len = new;                                                \
        (str)->heap.data[new] = 0;                                            \
      }                                                                       \
  while (0)

void
mstr_free (mstr_t *str)
{
  if (mstr_is_heap (str))
    free (str->heap.data);
  *str = MSTR_INIT;
}

void
mstr_clear (mstr_t *str)
{
  set_len (str, 0);
}

mstr_t *
mstr_reserve (mstr_t *str, size_t n)
{
  char *data;
  size_t cap;

  if ((cap = mstr_cap (str)) >= n)
    return str;

  do
    cap <<= 1;
  while (cap < n);

  cap++;

  if (mstr_is_heap (str))
    {
      if (!(data = realloc (str->heap.data, cap)))
        return NULL;
    }
  else
    {
      if (!(data = malloc (cap)))
        return NULL;

      /* copy to heap */
      size_t len = str->sso.len;
      if (memcpy (data, str->sso.data, len + 1) != data)
        { /* copy failed */
          free (data);
          return NULL;
        }

      /* save length */
      str->heap.len = len;
    }

  str->heap.data = data;
  str->heap.cap = cap;
  return str;
}

mstr_t *
mstr_remove (mstr_t *str, size_t start, size_t n)
{
  if (!n)
    return str;

  size_t len = mstr_len (str);
  char *data = mstr_data (str);

  if (start >= len)
    /* out of range */
    return NULL;

  if (start + n >= len)
    {
      set_len (str, start);
      return str;
    }

  char *dest = data + start;
  char *src = dest + n;

  if (memmove (dest, src, len - start - n) != dest)
    /* move failed */
    return NULL;

  set_len (str, len - n);
  return str;
}

mstr_t *
mstr_substr (mstr_t *save, const mstr_t *from, size_t start, size_t n)
{
  if (!n)
    return NULL;

  size_t len = mstr_len (from);
  const char *pos = mstr_data (from) + start;

  if (start >= len)
    /* out of range */
    return NULL;

  if (n > len - start)
    n = len - start;

  return mstr_assign_byte (save, pos, n);
}

mstr_t *
mstr_format (mstr_t *str, const char *fmt, ...)
{
  va_list ap, copy;
  va_start (ap, fmt);
  va_copy (copy, ap);

  mstr_t *ret = NULL;
  mstr_t new = MSTR_INIT;
  size_t cap = MSTR_SSO_CAP;
  int need = vsnprintf (new.sso.data, cap, fmt, ap);

  if (need < 0)
    goto err;

  if ((size_t)need >= cap)
    {
      cap = need + 1;
      if (mstr_reserve (&new, cap) != &new)
        goto err;
      if (vsnprintf (new.heap.data, cap, fmt, copy) < 0)
        goto err;
    }

  set_len (&new, need);
  mstr_free (str);
  *str = new;

  ret = str;
  goto ret;

err:
  mstr_free (&new);

ret:
  va_end (copy);
  va_end (ap);
  return ret;
}

void
mstr_trim (mstr_t *str)
{
  size_t len;

  if (!(len = mstr_len (str)))
    return;

  char *data = mstr_data (str);
  char *end = data + len - 1;
  char *start = data;
  size_t newlen;

  for (; start <= end && isspace (*start);)
    start++;
  for (; end >= start && isspace (*end);)
    end--;

  if (!(newlen = end - start + 1))
    return mstr_clear (str);
  if (newlen == len)
    return;

  if (memmove (data, start, newlen) == data)
    set_len (str, newlen);
}

bool
mstr_start_with_byte (const mstr_t *str, const void *src, size_t n)
{
  if (!n || n > mstr_len (str))
    return false;

  return memcmp (mstr_data (str), src, n) == 0;
}

bool
mstr_end_with_byte (const mstr_t *str, const void *src, size_t n)
{
  if (!n || n > mstr_len (str))
    return false;

  const char *pos = mstr_data (str) + mstr_len (str) - n;
  return memcmp (pos, src, n) == 0;
}

int
mstr_cmp_byte (const mstr_t *str, const void *src, size_t n)
{
  size_t len = mstr_len (str);

  if (!n || !len)
    return 0;

  const char *data = mstr_data (str);
  int ret = memcmp (data, src, n > len ? len : n);

  if (ret != 0 || n == len)
    return ret;
  return n < len ? 1 : -1;
}

int
mstr_icmp_byte (const mstr_t *str, const void *src, size_t n)
{
  size_t len = mstr_len (str);

  if (!n || !len)
    return 0;

  const char *data = mstr_data (str);
  int ret = strncasecmp (data, src, n > len ? len : n);

  if (ret != 0 || n == len)
    return ret;
  return n < len ? 1 : -1;
}

mstr_t *
mstr_cat_byte (mstr_t *str, const void *src, size_t n)
{
  if (!n)
    return str;

  size_t len = mstr_len (str);
  if (mstr_reserve (str, len + n + 1) != str)
    /* allocate failed */
    return NULL;

  char *dest = mstr_data (str) + len;
  if (memcpy (dest, src, n) != dest)
    /* copy failed */
    return NULL;

  set_len (str, len + n);
  return str;
}

mstr_t *
mstr_insert_byte (mstr_t *str, size_t pos, const void *src, size_t n)
{
  if (!n)
    return str;

  size_t len = mstr_len (str);

  if (pos > len)
    return NULL;

  if (pos == len)
    return mstr_cat_byte (str, src, n);

  if (mstr_reserve (str, len + n + 1) != str)
    /* allocate failed */
    return NULL;

  char *data = mstr_data (str);
  char *start = data + pos;
  char *dest = start + n;

  if (memmove (dest, start, len - pos) != dest)
    /* move failed */
    return NULL;

  if (memcpy (start, src, n) != start)
    /* copy failed */
    return NULL;

  set_len (str, len + n);
  return str;
}

mstr_t *
mstr_assign_byte (mstr_t *str, const void *src, size_t n)
{
  if (!n)
    return str;

  if (mstr_reserve (str, n + 1) != str)
    /* allocate failed */
    return NULL;

  char *data = mstr_data (str);
  if (memcpy (data, src, n) != data)
    /* copy failed */
    return NULL;

  set_len (str, n);
  return str;
}
