#include "httpd.h"
#include "config.h"
#include "rbtree.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
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
typedef struct resource_t resource_t;

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
  FILE *out;
  request_t req;
  client_t *clnt;
};

static void context_free (context_t *ctx);
static int context_init (context_t *ctx, client_t *clnt);

/* resource */

struct resource_t
{
  int mime;
  FILE *src;
  size_t size;
  context_t *ctx;
};

static void resource_free (resource_t *res);
static int resource_init (resource_t *res, context_t *ctx);

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
static void serve_not_found (context_t *ctx);

static bool send_data (context_t *ctx, const void *data, size_t n);
static int reshdr_init (char *dst, int max, int code, resource_t *res);

void
server_free (server_t *serv)
{
  threadpool_free (&serv->pool);
  mstr_free (&serv->root);
  close (serv->sock);
}

void
server_poll (server_t *serv)
{
  client_t *clnt;
  socklen_t len = sizeof (clnt->addr);

  if (!(clnt = malloc (sizeof (client_t))))
    error ("malloc failed");

  /* init serv */
  clnt->serv = serv;

  /* init sock and addr */
  if ((clnt->sock = accept (serv->sock, (void *)&clnt->addr, &len)) == -1)
    goto clean_clnt;

  /* post task */
  if (threadpool_post (&serv->pool, serve, clnt) != 0)
    goto clean_sock;

  return;

clean_sock:
  close (clnt->sock);

clean_clnt:
  free (clnt);
}

int
server_init (server_t *serv, uint16_t port, const char *root, size_t threads,
             int backlog, int flags)
{
  int ret;

  /* init port */
  serv->port = port;

  /* init addr */
  serv->addr = (sockaddr4_t){
    .sin_family = AF_INET,
    .sin_port = htons (port),
    .sin_addr.s_addr = htonl (INADDR_ANY),
  };

  /* init sock */
  if ((serv->sock = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    reto (HTTPD_ERR_SERVER_INIT_SOCK, ret);

  /* bind addr */
  if (bind (serv->sock, (void *)&serv->addr, sizeof (serv->addr)) != 0)
    reto (HTTPD_ERR_SERVER_INIT_BIND, clean_sock);

  /* open reuseaddr option */
  if (flags & SERVER_REUSEADDR)
    {
      int opt = true;
      socklen_t len = sizeof (int);
      if (setsockopt (serv->sock, SOL_SOCKET, SO_REUSEADDR, &opt, len) != 0)
        reto (HTTPD_ERR_SERVER_INIT_REUSEADDR, clean_sock);
    }

  /* init root */
  serv->root = MSTR_INIT;
  if (!mstr_assign_cstr (&serv->root, root))
    reto (HTTPD_ERR_SERVER_INIT_ROOT, clean_sock);

  /* init pool */
  if (threadpool_init (&serv->pool, threads) != 0)
    reto (HTTPD_ERR_SERVER_INIT_POOL, clean_root);

  /* listen */
  if (listen (serv->sock, backlog) != 0)
    reto (HTTPD_ERR_SERVER_INIT_LISTEN, clean_pool);

  return 0;

clean_pool:
  threadpool_free (&serv->pool);

clean_root:
  mstr_free (&serv->root);

clean_sock:
  close (serv->sock);

ret:
  return ret;
}

static void
client_free (client_t *clnt)
{
  close (clnt->sock);
  free (clnt);
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
    reto (HTTPD_ERR_REQUEST_INIT_LINE, ret);

  pos = line;
  if (!(end = strchr (pos, ' ')))
    reto (HTTPD_ERR_REQUEST_INIT_METHOD, ret);
  len = end - pos;

  /* init method */
  static const char *methods[] = {
    "GET",    "PUT",     "HEAD",    "POST",      "TRACE",
    "DELETE", "OPTIONS", "CONNECT", "EXTENSION",
  };

  const int msize = sizeof (methods) / sizeof (*methods);

  for (int i = 0; i < msize; i++)
    if (memcmp (pos, methods[i], len) == 0 || i == msize - 1)
      {
        req->method = i;
        break;
      }

  pos = end + 1;
  if (!(end = strchr (pos, ' ')))
    reto (HTTPD_ERR_REQUEST_INIT_URI, ret);
  len = end - pos;

  /* init uri */
  req->uri = MSTR_INIT;
  if (!mstr_assign_byte (&req->uri, pos, len))
    reto (HTTPD_ERR_REQUEST_INIT_URI, ret);

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

      /* reach end */
      if (len == 2 && pos[0] == '\r' && pos[1] == '\n')
        return 0;

      if (!(sep = strchr (pos, ':')) || !(end = strchr (sep, '\r')))
        reto (HTTPD_ERR_REQUEST_INIT_HEADERS, clean_hdrs);

      header_t *header;
      if (!(header = malloc (sizeof (header_t))))
        reto (HTTPD_ERR_REQUEST_INIT_HEADERS, clean_hdrs);
      /* init field and value */
      header->field = header->value = MSTR_INIT;

      ptrdiff_t fld_len = sep - pos;
      if (fld_len <= 0 || !mstr_assign_byte (&header->field, pos, fld_len))
        reto (HTTPD_ERR_REQUEST_INIT_HEADERS, clean_header);

      char *val_st = sep + 1, *val_ed = end - 1;
      for (; *val_st == ' ' || *val_st == '\t'; val_st++)
        ;
      for (; *val_ed == ' ' || *val_ed == '\t'; val_ed--)
        ;

      ptrdiff_t val_len = val_ed - val_st;
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

ret:
  return ret;
}

static void
context_free (context_t *ctx)
{
  fclose (ctx->in);
  fclose (ctx->out);
  request_free (&ctx->req);
}

static int
context_init (context_t *ctx, client_t *clnt)
{
  int ret;

  /* init in */
  if (!(ctx->in = fdopen (clnt->sock, "r")))
    reto (HTTPD_ERR_CONTEXT_INIT_IN, ret);

  /* init out */
  if (!(ctx->out = fdopen (dup (clnt->sock), "w")))
    reto (HTTPD_ERR_CONTEXT_INIT_OUT, clean_in);

  /* init req */
  if (request_init (&ctx->req, ctx) != 0)
    reto (HTTPD_ERR_CONTEXT_INIT_REQ, clean_out);

  /* init clnt */
  ctx->clnt = clnt;

  return 0;

clean_out:
  fclose (ctx->out);

clean_in:
  fclose (ctx->in);

ret:
  return ret;
}

static void
resource_free (resource_t *res)
{
  fclose (res->src);
}

static int
resource_init (resource_t *res, context_t *ctx)
{
  /* init ctx */
  res->ctx = ctx;

  /* init src */
  client_t *clnt = ctx->clnt;
  mstr_t *root = &clnt->serv->root, *uri = &ctx->req.uri;
  size_t root_len = mstr_len (root), uri_len = mstr_len (uri);

  size_t path_len = root_len + uri_len;
  char *path = alloca (path_len + 16);
  struct stat info;

  path[path_len] = '\0';
  memcpy (path, mstr_data (root), root_len);
  memcpy (path + root_len, mstr_data (uri), uri_len);

  if (stat (path, &info) != 0)
    return HTTPD_ERR_RESOURCE_IHIT_404;

  if (S_ISDIR (info.st_mode))
    for (size_t i = 0; i < indexs_size; i++)
      {
        strcpy (path + path_len, indexs[i]);
        if (stat (path, &info) == 0 && S_ISREG (info.st_mode))
          break;
        if (i == indexs_size - 1)
          return HTTPD_ERR_RESOURCE_IHIT_404;
      }

  if (!(res->src = fopen (path, "r")))
    return HTTPD_ERR_RESOURCE_IHIT_SRC;

  /* init size */
  res->size = info.st_size;

  /* init mime */
  char *pos = strrchr (path, '.');

  if (pos == NULL)
    res->mime = HTTPD_MIME_TEXT;
  else if (strcmp (pos, ".html") == 0)
    res->mime = HTTPD_MIME_HTML;
  else if (strcmp (pos, ".htm") == 0)
    res->mime = HTTPD_MIME_HTML;
  else if (strcmp (pos, ".css") == 0)
    res->mime = HTTPD_MIME_CSS;
  else if (strcmp (pos, ".js") == 0)
    res->mime = HTTPD_MIME_JS;
  else
    res->mime = HTTPD_MIME_PLAIN;

  return 0;
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
  header_t temp = { .field.heap.data = (char *)field };
  rbtree_node_t *ret = rbtree_find (headers, &temp.node, header_comp);
  return ret ? container_of (ret, header_t, node) : NULL;
}

static int
header_comp (const rbtree_node_t *a, const rbtree_node_t *b)
{
  const header_t *ha = container_of (a, header_t, node);
  const header_t *hb = container_of (b, header_t, node);
  return mstr_cmp_mstr (&ha->field, &hb->field);
}

static void
serve (void *arg)
{
  context_t ctx;
  resource_t res;

  if (context_init (&ctx, arg) != 0)
    goto ret;

  int init = resource_init (&res, &ctx);

  if (init != 0)
    {
      if (init == HTTPD_ERR_RESOURCE_IHIT_404)
        serve_not_found (&ctx);
      goto clean_ctx;
    }

  const size_t buff_size = MAX_RESHEAD_LEN;
  static __thread char buff[MAX_RESHEAD_LEN];

  int code = (init == 0) ? 200 : 404;
  int size = reshdr_init (buff, buff_size, code, &res);

  if (size <= 0)
    goto clean_res;

  /* send header */
  if (!send_data (&ctx, buff, size))
    goto clean_res;

  /* send data */
  for (; (size = fread (buff, 1, buff_size, res.src));)
    if (!send_data (&ctx, buff, size))
      goto clean_res;

clean_res:
  resource_free (&res);

clean_ctx:
  context_free (&ctx);

ret:
  client_free (arg);
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

static inline bool
send_data (context_t *ctx, const void *data, size_t size)
{
  return size ? fwrite (data, 1, size, ctx->out) : true;
}

static int
reshdr_init (char *dst, int max, int code, resource_t *res)
{
  const char *msg, *mime;
  size_t size = res->size;

  context_t *ctx = res->ctx;
  request_t req = ctx->req;
  int ver = req.proto;

  switch (code)
    {
    case 200:
      msg = "OK";
      break;

    case 404:
      msg = "NOT FOUND";
      break;

    default:
      /* only support 200 and 404 */
      return 0;
    }

  switch (res->mime)
    {
    case HTTPD_MIME_JS:
      mime = "application/javascript";
      break;

    case HTTPD_MIME_CSS:
      mime = "text/css";
      break;

    case HTTPD_MIME_HTML:
      mime = "text/html";
      break;

    case HTTPD_MIME_TEXT:
    case HTTPD_MIME_PLAIN:
      mime = "text/plain";
      break;

    default:
      mime = "text/plain";
      break;
    }

  static const char *format = "HTTP/1.%d %d %s"
                              "\r\n"
                              "Server: httpd\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %lu\r\n"
                              "\r\n";

  return snprintf (dst, max, format, ver, code, msg, mime, size);
}
