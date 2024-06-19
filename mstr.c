#include "mstr.h"

#include <stdlib.h>
#include <string.h>

#define set_len(str, new)                                                     \
  do                                                                          \
    if (!mstr_is_sso (str))                                                   \
      (str)->heap.len = new;                                                  \
    else                                                                      \
      (str)->sso.len = new;                                                   \
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
  *mstr_data (str) = '\0';
  set_len (str, 0);
}

mstr_t *
mstr_reserve (mstr_t *str, size_t cap)
{
  char *newdata;
  size_t newcap;

  if ((newcap = mstr_cap (str)) >= cap)
    return str;

  /* compute capacity */
  while (newcap < cap)
    newcap *= MSTR_EXPAN_RATIO;

  if (newcap % 2)
    newcap++;

  if (mstr_is_heap (str))
    {
      if (!(newdata = realloc (str->heap.data, newcap)))
        return NULL;
    }
  else
    {
      if (!(newdata = malloc (newcap)))
        return NULL;

      /* copy to heap */
      size_t len = str->sso.len;
      if (memcpy (newdata, str->sso.data, len + 1) != newdata)
        { /* copy failed */
          free (newdata);
          return NULL;
        }

      /* save length */
      str->heap.len = len;
    }

  str->heap.data = newdata;
  str->heap.cap = newcap;
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

  if (n > len - start)
    n = len - start;

  char *dest = data + start;
  char *src = dest + n;

  if (memmove (dest, src, len - start - n + 1) != dest)
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

  return mstr_assign_byte (save, (void *)pos, n);
}

bool
mstr_start_with_char (const mstr_t *str, char ch)
{
  return mstr_start_with_byte (str, (void *)&ch, 1);
}

bool
mstr_start_with_cstr (const mstr_t *str, const char *cstr)
{
  return mstr_start_with_byte (str, (void *)cstr, strlen (cstr));
}

bool
mstr_start_with_mstr (const mstr_t *str, const mstr_t *other)
{
  return mstr_start_with_byte (str, (void *)mstr_data (other),
                               mstr_len (other));
}

bool
mstr_start_with_byte (const mstr_t *str, const mstr_byte_t *src, size_t n)
{
  if (!n || n > mstr_len (str))
    return false;

  return memcmp (mstr_data (str), src, n) == 0;
}

bool
mstr_end_with_char (const mstr_t *str, char ch)
{
  return mstr_end_with_byte (str, (void *)&ch, 1);
}

bool
mstr_end_with_cstr (const mstr_t *str, const char *cstr)
{
  return mstr_end_with_byte (str, (void *)cstr, strlen (cstr));
}

bool
mstr_end_with_mstr (const mstr_t *str, const mstr_t *other)
{
  return mstr_end_with_byte (str, (void *)mstr_data (other), mstr_len (other));
}

bool
mstr_end_with_byte (const mstr_t *str, const mstr_byte_t *src, size_t n)
{
  if (!n || n > mstr_len (str))
    return false;

  const char *pos = mstr_data (str) + mstr_len (str) - n;
  return memcmp (pos, src, n) == 0;
}

int
mstr_cmp_char (const mstr_t *str, char ch)
{
  return mstr_cmp_byte (str, (void *)&ch, 1);
}

int
mstr_cmp_cstr (const mstr_t *str, const char *cstr)
{
  return mstr_cmp_byte (str, (void *)cstr, strlen (cstr));
}

int
mstr_cmp_mstr (const mstr_t *str, const mstr_t *other)
{
  return mstr_cmp_byte (str, (void *)mstr_data (other), mstr_len (other));
}

int
mstr_cmp_byte (const mstr_t *str, const mstr_byte_t *src, size_t n)
{
  if (!n)
    return 0;

  size_t len = mstr_len (str);
  const char *data = mstr_data (str);
  int ret = memcmp (data, src, n > len ? len : n);

  if (ret != 0 || n == len)
    return ret;
  return len > n ? 1 : -1;
}

mstr_t *
mstr_cat_char (mstr_t *str, char ch)
{
  return mstr_cat_byte (str, (void *)&ch, 1);
}

mstr_t *
mstr_cat_cstr (mstr_t *str, const char *cstr)
{
  return mstr_cat_byte (str, (void *)cstr, strlen (cstr));
}

mstr_t *
mstr_cat_mstr (mstr_t *str, const mstr_t *other)
{
  return mstr_cat_byte (str, (void *)mstr_data (other), mstr_len (other));
}

mstr_t *
mstr_cat_byte (mstr_t *str, const mstr_byte_t *src, size_t n)
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
  /* save NULL */
  dest[n] = '\0';

  set_len (str, len + n);
  return str;
}

mstr_t *
mstr_insert_char (mstr_t *str, size_t pos, char ch)
{
  return mstr_insert_byte (str, pos, (void *)&ch, 1);
}

mstr_t *
mstr_insert_cstr (mstr_t *str, size_t pos, const char *cstr)
{
  return mstr_insert_byte (str, pos, (void *)cstr, strlen (cstr));
}

mstr_t *
mstr_insert_mstr (mstr_t *str, size_t pos, const mstr_t *other)
{
  return mstr_insert_byte (str, pos, (void *)mstr_data (other),
                           mstr_len (other));
}

mstr_t *
mstr_insert_byte (mstr_t *str, size_t pos, const mstr_byte_t *src, size_t n)
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

  if (memmove (dest, start, len - pos + 1) != dest)
    /* move failed */
    return NULL;

  if (memcpy (start, src, n) != start)
    /* copy failed */
    return NULL;

  set_len (str, len + n);
  return str;
}

mstr_t *
mstr_assign_char (mstr_t *str, char ch)
{
  return mstr_assign_byte (str, (void *)&ch, 1);
}

mstr_t *
mstr_assign_cstr (mstr_t *str, const char *cstr)
{
  return mstr_assign_byte (str, (void *)cstr, strlen (cstr));
}

mstr_t *
mstr_assign_mstr (mstr_t *str, const mstr_t *other)
{
  return mstr_assign_byte (str, (void *)mstr_data (other), mstr_len (other));
}

mstr_t *
mstr_assign_byte (mstr_t *str, const mstr_byte_t *src, size_t n)
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
  /* save NULL */
  data[n] = '\0';

  set_len (str, n);
  return str;
}
