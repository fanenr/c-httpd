#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>

#define error(fmt, ...)                                                       \
  do                                                                          \
    {                                                                         \
      fprintf (stderr, "%s:%d (%s): ", __FUNCTION__, __LINE__, __FILE__);     \
      fprintf (stderr, fmt, ##__VA_ARGS__);                                   \
      fprintf (stderr, "\n");                                                 \
      exit (1);                                                               \
    }                                                                         \
  while (0)

#define reto(val, label)                                                      \
  do                                                                          \
    {                                                                         \
      ret = val;                                                              \
      goto label;                                                             \
    }                                                                         \
  while (0)

#endif
