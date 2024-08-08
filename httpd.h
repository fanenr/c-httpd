#ifndef HTTPD_H
#define HTTPD_H

#include "arena.h"
#include "mstr.h"
#include "respool.h"
#include "threadpool.h"

#include <netinet/in.h>
#include <sys/socket.h>

#define SERVER_REUSEADDR 1
#define SERVER_NONBLOCK 2

enum
{
  HTTPD_OK,

  HTTPD_ERR_SERVER_INIT_ROOT,
  HTTPD_ERR_SERVER_INIT_SOCK,
  HTTPD_ERR_SERVER_INIT_BIND,
  HTTPD_ERR_SERVER_INIT_RPOOL,
  HTTPD_ERR_SERVER_INIT_TPOOL,
  HTTPD_ERR_SERVER_INIT_LISTEN,
  HTTPD_ERR_SERVER_INIT_REUSEADDR,

  HTTPD_ERR_REQUEST_INIT_VER,
  HTTPD_ERR_REQUEST_INIT_URI,
  HTTPD_ERR_REQUEST_INIT_LINE,
  HTTPD_ERR_REQUEST_INIT_METHOD,
  HTTPD_ERR_REQUEST_INIT_HEADERS,

  HTTPD_ERR_CONTEXT_INIT_IN,
  HTTPD_ERR_CONTEXT_INIT_OUT,
  HTTPD_ERR_CONTEXT_INIT_REQ,

  HTTPD_ERR_RESOURCE_IHIT_FD,
  HTTPD_ERR_RESOURCE_IHIT_404,
  HTTPD_ERR_RESOURCE_IHIT_PATH,
  HTTPD_ERR_RESOURCE_IHIT_DATA,
};

enum
{
  HTTPD_METHOD_GET,
  HTTPD_METHOD_PUT,
  HTTPD_METHOD_HEAD,
  HTTPD_METHOD_POST,
  HTTPD_METHOD_TRACE,
  HTTPD_METHOD_DELETE,
  HTTPD_METHOD_OPTIONS,
  HTTPD_METHOD_CONNECT,
  HTTPD_METHOD_EXTENSION,
};

typedef struct sockaddr sockaddr_t;
typedef struct sockaddr_in sockaddr4_t;
typedef struct sockaddr_in6 sockaddr6_t;

typedef struct server_t server_t;
typedef struct server_config_t server_config_t;

struct server_t
{
  int sock;
  mstr_t root;
  uint16_t port;
  arena_t mpool;
  respool_t rpool;
  sockaddr4_t addr;
  threadpool_t tpool;
};

struct server_config_t
{
  int flags;
  int backlog;
  uint16_t port;
  size_t threads;
  const char *root;
};

extern void server_free (server_t *serv);

extern void server_poll (server_t *serv);

extern int server_init (server_t *serv, const server_config_t *conf);

#endif
