#ifndef HTTPD_H
#define HTTPD_H

#include "mstr.h"
#include "threadpool.h"

#include <netinet/in.h>
#include <sys/socket.h>

#define HTTPD_SERVER_REUSEADDR 1

enum
{
  HTTPD_OK,

  HTTPD_ERR_SERVER_INIT_ROOT,
  HTTPD_ERR_SERVER_INIT_POOL,
  HTTPD_ERR_SERVER_INIT_SOCK,
  HTTPD_ERR_SERVER_INIT_BIND,
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

  HTTPD_ERR_RESOURCE_IHIT_SRC,
  HTTPD_ERR_RESOURCE_IHIT_404,
};

enum
{
  HTTPD_MIME_JS,
  HTTPD_MIME_CSS,
  HTTPD_MIME_HTML,
  HTTPD_MIME_TEXT,
  HTTPD_MIME_PLAIN,
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
