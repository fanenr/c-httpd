#ifndef HTTPD_H
#define HTTPD_H

#include "mstr.h"
#include "threadpool.h"

#include <netinet/in.h>
#include <sys/socket.h>

#define SERVER_REUSEADDR 1

typedef struct sockaddr sockaddr_t;
typedef struct sockaddr_in sockaddr4_t;
typedef struct sockaddr_in6 sockaddr6_t;

typedef struct server_t server_t;
typedef struct client_t client_t;

struct server_t
{
  int sock;
  mstr_t root;
  uint16_t port;
  sockaddr4_t addr;
  threadpool_t pool;
};

struct client_t
{
  int sock;
  server_t *serv;
  sockaddr4_t addr;
};

extern void server_free (server_t *serv);

extern void server_poll (server_t *serv);

extern int server_init (server_t *serv, uint16_t port, const char *root,
                        size_t threads, int backlog, int flags);

#endif
