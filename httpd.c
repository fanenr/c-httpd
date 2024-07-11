#include "httpd.h"
#include "config.h"
#include "mime.h"
#include "rbtree.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_REQLINE_LEN 1024
#define MAX_RESHEAD_LEN 4096

#define error(fmt, ...)                                                       \
  do                                                                          \
    {                                                                         \
      fprintf (stderr, "%s:%d (%s): ", __FUNCTION__, __LINE__, __FILE__);     \
      fprintf (stderr, fmt, ##__VA_ARGS__);                                   \
      fprintf (stderr, "\n");                                                 \
      __builtin_trap ();                                                      \
    }                                                                         \
  while (0)

#define reto(val, label)                                                      \
  do                                                                          \
    {                                                                         \
      ret = val;                                                              \
      goto label;                                                             \
    }                                                                         \
  while (0)

typedef struct header_t header_t;
typedef struct client_t client_t;
typedef struct request_t request_t;
typedef struct context_t context_t;

/* client */

struct client_t
{
  int sock;
  server_t *serv;
  sockaddr4_t addr;
};

static void client_free (client_t *clnt);

/* request */

struct request_t
{
  int proto;
  int method;
  mstr_t uri;
  rbtree_t headers;
};

static void request_free (request_t *req);
static int request_init (request_t *req, context_t *ctx);

/* context */

struct context_t
{
  FILE *in;
  request_t req;
  client_t *clnt;
};

static void context_free (context_t *ctx);
static int context_init (context_t *ctx, client_t *clnt);

/* header */

struct header_t
{
  mstr_t field;
  mstr_t value;
  rbtree_node_t node;
};

static void header_free (rbtree_node_t *n);
static bool header_insert (rbtree_t *headers, header_t *header);
static header_t *header_get (rbtree_t *headers, const char *field);
static int header_comp (const rbtree_node_t *a, const rbtree_node_t *b);

/* serve */

static void serve (void *arg);
static void serve_file (context_t *ctx);
static void serve_not_found (context_t *ctx);

static resource_t *resource_get (context_t *ctx);
static bool send_data (context_t *ctx, const void *data, size_t n);
static int reshdr_init (char *dst, int max, int code, const char *msg,
                        resource_t *res);

void
server_free (server_t *serv)
{
  threadpool_free (&serv->tpool);
  respool_free (&serv->rpool);
  arena_free (&serv->mpool);
  mstr_free (&serv->root);
  close (serv->sock);
}

void
server_poll (server_t *serv)
{
  client_t *clnt;
  socklen_t len = sizeof (clnt->addr);

  if (!(clnt = arena_alloc (&serv->mpool, sizeof (client_t))))
    error ("arena_alloc failed");

  /* init serv */
  clnt->serv = serv;

  /* init sock and addr */
  if ((clnt->sock = accept (serv->sock, (void *)&clnt->addr, &len)) == -1)
    goto clean_clnt;

  /* post task */
  if (threadpool_post (&serv->tpool, serve, clnt) != 0)
    goto clean_sock;

  return;

clean_sock:
  close (clnt->sock);

clean_clnt:
  free (clnt);
}

int
server_init (server_t *serv, const server_config_t *conf)
{
#define conf_get(mem, def) (conf ? (conf->mem ?: def) : def)

  int flags = conf_get (flags, FLAGS);
  uint16_t port = conf_get (port, PORT);
  const char *root = conf_get (root, ROOT);
  int backlog = conf_get (backlog, BACKLOG);
  size_t threads = conf_get (threads, THREADS);

#undef conf_get

  int ret;

  /* init port */
  serv->port = port;

  /* init mpool */
  serv->mpool = ARENA_INIT;

  /* init addr */
  serv->addr = (sockaddr4_t){
    .sin_family = AF_INET,
    .sin_port = htons (port),
    .sin_addr.s_addr = htonl (INADDR_ANY),
  };

  /* init root */
  serv->root = MSTR_INIT;
  if (!mstr_assign_cstr (&serv->root, root))
    return HTTPD_ERR_SERVER_INIT_ROOT;

  /* init rpool */
  if (respool_init (&serv->rpool) != 0)
    reto (HTTPD_ERR_SERVER_INIT_RPOOL, clean_root);

  /* init tpool */
  if (threadpool_init (&serv->tpool, threads) != 0)
    reto (HTTPD_ERR_SERVER_INIT_TPOOL, clean_rpool);

  /* init sock */
  if ((serv->sock = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    reto (HTTPD_ERR_SERVER_INIT_SOCK, clean_tpool);

  /* bind addr */
  if (bind (serv->sock, (void *)&serv->addr, sizeof (serv->addr)) != 0)
    reto (HTTPD_ERR_SERVER_INIT_BIND, clean_sock);

  /* listen */
  if (listen (serv->sock, backlog) != 0)
    reto (HTTPD_ERR_SERVER_INIT_LISTEN, clean_sock);

  /* open reuseaddr option */
  if (flags & SERVER_REUSEADDR)
    {
      int opt = true;
      socklen_t len = sizeof (int);
      if (setsockopt (serv->sock, SOL_SOCKET, SO_REUSEADDR, &opt, len) != 0)
        reto (HTTPD_ERR_SERVER_INIT_REUSEADDR, clean_sock);
    }

  return 0;

clean_sock:
  close (serv->sock);

clean_tpool:
  threadpool_free (&serv->tpool);

clean_rpool:
  respool_free (&serv->rpool);

clean_root:
  mstr_free (&serv->root);

  return ret;
}

static void
client_free (client_t *clnt)
{
  close (clnt->sock);
}

static void
request_free (request_t *req)
{
  mstr_free (&req->uri);
  rbtree_visit (&req->headers, header_free);
}

static int
request_init (request_t *req, context_t *ctx)
{
  static __thread char line[MAX_REQLINE_LEN];
  const char *pos, *end;
  size_t len;
  int ret;

  if (!fgets (line, MAX_REQLINE_LEN, ctx->in))
    return HTTPD_ERR_REQUEST_INIT_LINE;

  pos = line;
  if (!(end = strchr (pos, ' ')))
    return HTTPD_ERR_REQUEST_INIT_METHOD;
  len = end - pos;

  /* init method */
  req->method = HTTPD_METHOD_EXTENSION;

  static const char *methods[] = { "GET",   "PUT",    "HEAD",    "POST",
                                   "TRACE", "DELETE", "OPTIONS", "CONNECT" };

  for (int i = 0; i < HTTPD_METHOD_EXTENSION; i++)
    if (strncmp (pos, methods[i], len) == 0)
      {
        req->method = i;
        break;
      }

  pos = end + 1;
  if (!(end = strchr (pos, ' ')))
    return HTTPD_ERR_REQUEST_INIT_URI;
  len = end - pos;

  /* init uri */
  req->uri = MSTR_INIT;
  if (!mstr_assign_byte (&req->uri, pos, len))
    return HTTPD_ERR_REQUEST_INIT_URI;

  pos = end + 1;
  if (!(end = strchr (pos, '\r')))
    reto (HTTPD_ERR_REQUEST_INIT_VER, clean_uri);
  len = end - pos;

  /* init ver */
  if (strncmp (pos, "HTTP/1.1", len) == 0)
    req->proto = 0;
  else if (strncmp (pos, "HTTP/1.0", len) == 0)
    req->proto = 1;
  else
    reto (HTTPD_ERR_REQUEST_INIT_VER, clean_uri);

  /* init headers */
  req->headers = RBTREE_INIT;

  for (;;)
    {
      if (!fgets (line, MAX_REQLINE_LEN, ctx->in))
        reto (HTTPD_ERR_REQUEST_INIT_LINE, clean_uri);

      size_t len = strlen (line);
      char *pos = line, *sep, *end;

      /* end of parsing */
      if (len == 2 && pos[0] == '\r' && pos[1] == '\n')
        return 0;

      /* init sep and end */
      if (!(sep = strchr (pos, ':')) || !(end = strchr (sep, '\r')))
        reto (HTTPD_ERR_REQUEST_INIT_HEADERS, clean_hdrs);

      header_t *header;
      ptrdiff_t fld_len = sep - pos;
      char *val_st = sep + 1, *val_ed = end - 1;

      /* init header */
      if (!(header = malloc (sizeof (header_t))))
        reto (HTTPD_ERR_REQUEST_INIT_HEADERS, clean_hdrs);
      header->field = header->value = MSTR_INIT;

      if (fld_len <= 0 || !mstr_assign_byte (&header->field, pos, fld_len))
        reto (HTTPD_ERR_REQUEST_INIT_HEADERS, clean_header);

      for (; *val_st == ' ' || *val_st == '\t';)
        val_st++;
      for (; *val_ed == ' ' || *val_ed == '\t';)
        val_ed--;

      ptrdiff_t val_len = val_ed - val_st + 1;
      if (val_len < 0 || !mstr_assign_byte (&header->value, val_st, val_len))
        reto (HTTPD_ERR_REQUEST_INIT_HEADERS, clean_field);

      if (!header_insert (&req->headers, header))
        reto (HTTPD_ERR_REQUEST_INIT_HEADERS, clean_value);

      continue;

    clean_value:
      mstr_free (&header->value);

    clean_field:
      mstr_free (&header->field);

    clean_header:
      free (header);
      goto clean_hdrs;
    }

clean_hdrs:
  rbtree_visit (&req->headers, header_free);

clean_uri:
  mstr_free (&req->uri);

  return ret;
}

static void
context_free (context_t *ctx)
{
  fclose (ctx->in);
  request_free (&ctx->req);
}

static int
context_init (context_t *ctx, client_t *clnt)
{
  int ret;

  /* init in */
  if (!(ctx->in = fdopen (clnt->sock, "r")))
    return HTTPD_ERR_CONTEXT_INIT_IN;

  /* init req */
  if (request_init (&ctx->req, ctx) != 0)
    reto (HTTPD_ERR_CONTEXT_INIT_REQ, clean_in);

  /* init clnt */
  ctx->clnt = clnt;

  return 0;

clean_in:
  fclose (ctx->in);

  return ret;
}

static void
header_free (rbtree_node_t *n)
{
  header_t *h = container_of (n, header_t, node);
  mstr_free (&h->field);
  mstr_free (&h->value);
  free (h);
}

static bool
header_insert (rbtree_t *headers, header_t *header)
{
  return rbtree_insert (headers, &header->node, header_comp);
}

static header_t *
header_get (rbtree_t *headers, const char *field)
{
  header_t target = { .field = MSTR_VIEW (field, strlen (field)) };
  rbtree_node_t *node = rbtree_find (headers, &target.node, header_comp);
  return node ? container_of (node, header_t, node) : NULL;
}

static int
header_comp (const rbtree_node_t *a, const rbtree_node_t *b)
{
  const header_t *ha = container_of (a, header_t, node);
  const header_t *hb = container_of (b, header_t, node);
  return mstr_icmp_mstr (&ha->field, &hb->field);
}

static void
serve (void *arg)
{
  context_t ctx;
  if (context_init (&ctx, arg) != 0)
    goto clean_arg;

  serve_file (&ctx);
  context_free (&ctx);

clean_arg:
  client_free (arg);
}

static void
serve_file (context_t *ctx)
{
  resource_t *res;

  if (!(res = resource_get (ctx)))
    {
      serve_not_found (ctx);
      return;
    }

  /* init response header */
  static __thread char head[MAX_RESHEAD_LEN];

  int size = reshdr_init (head, sizeof (head), 200, "OK", res);

  if (size <= 0)
    return;

  /* send header */
  if (!send_data (ctx, head, size))
    return;

  /* send data */
  if (!send_data (ctx, res->data, res->size))
    return;
}

static void
serve_not_found (context_t *ctx)
{
  static const char *res = "HTTP/1.1 404 NOT FOUND\r\n"
                           "Server: httpd\r\n"
                           "Content-Type: text/html\r\n"
                           "Content-Length: 13\r\n\r\n"
                           "404 NOT FOUND";
  send_data (ctx, res, 99);
}

static resource_t *
resource_get (context_t *ctx)
{
  /* build path */
  server_t *serv = ctx->clnt->serv;
  mstr_t *root = &serv->root, *uri = &ctx->req.uri;
  size_t root_len = mstr_len (root), uri_len = mstr_len (uri);

  size_t path_len = root_len + uri_len;
  char *path = alloca (path_len + 16);
  struct stat info;

  path[path_len] = '\0';
  memcpy (path, mstr_data (root), root_len);
  memcpy (path + root_len, mstr_data (uri), uri_len);

  if (stat (path, &info) != 0)
    return NULL;

  if (S_ISDIR (info.st_mode))
    for (int i = 0; i < indexs_size; i++)
      {
        strcpy (path + path_len, indexs[i]);
        if (stat (path, &info) == 0 && S_ISREG (info.st_mode))
          break;
        if (i == indexs_size - 1)
          return NULL;
      }

  /* query size */
  size_t size = info.st_size;
  respool_t *pool = &serv->rpool;
  resource_t *res = respool_get (pool, path);
  return res ? res : respool_add (pool, path, size);
}

static inline bool
send_data (context_t *ctx, const void *data, size_t size)
{
  return size ? send (ctx->clnt->sock, data, size, 0) != -1 : true;
}

static int
reshdr_init (char *dst, int max, int code, const char *msg, resource_t *res)
{
  const char *mime;
  size_t size = res->size;

  if (!(mime = mime_of (mstr_data (&res->path))))
    mime = "text/plain";

  static const char *format = "HTTP/1.1 %d %s"
                              "\r\n"
                              "Server: httpd\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %lu\r\n"
                              "\r\n";

  return snprintf (dst, max, format, code, msg, mime, size);
}
