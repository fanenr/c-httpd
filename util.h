#ifndef UTIL_H
#define UTIL_H

#define error(fmt, ...)                                                       \
  do                                                                          \
    {                                                                         \
      fprintf (stderr, "%s:%d (%s): ", __FUNCTION__, __LINE__, __FILE__);     \
      fprintf (stderr, fmt, ##__VA_ARGS__);                                   \
      fprintf (stderr, "\n");                                                 \
      __builtin_trap ();                                                      \
    }                                                                         \
  while (0)

#define error_if(expr, fmt, ...)                                              \
  do                                                                          \
    if (expr)                                                                 \
      error (fmt, ##__VA_ARGS__);                                             \
  while (0)

typedef struct sockaddr sockaddr_t;
typedef struct sockaddr_in sockaddr4_t;
typedef struct sockaddr_in6 sockaddr6_t;

#endif
