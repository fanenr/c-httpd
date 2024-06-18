#include "threadpool.h"
#include "util.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

/* config */

#define PORT 3354
#define BACKLOG 48
#define THREADS 16
#define BUFF_SIZE 4096
#define REUSEADDR true
#define DEFAULT_LENGTH

/* typedef */

typedef struct client_t client_t;

struct client_t
{
  int sock;
  socklen_t len;
  sockaddr4_t addr;
};

/* global variables */

static int sock;
static sockaddr4_t addr;
static threadpool_t pool;
static uint16_t port = PORT;

static void serve (void *arg);
static bool send_header (FILE *out, int code, size_t len);
static bool send_data (FILE *out, const void *data, size_t n);

int
main (int argc, char **argv)
{
  if (threadpool_init (&pool, THREADS) != 0)
    error ("threadpool init failed");

  if (argc >= 2)
    {
      char *raw = argv[1], *end;
      port = strtol (raw, &end, 10);
      error_if (end == raw, "port pase failed");
    }

  addr.sin_family = AF_INET;
  addr.sin_port = htons (port);
  addr.sin_addr.s_addr = htonl (INADDR_ANY);

  /* socket */
  if ((sock = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    error ("create socket failed");

  /* setsockopt */
  int opt = REUSEADDR;
  socklen_t opt_len = sizeof (opt);
  if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &opt, opt_len) != 0)
    error ("setsockopt failed");

  /* bind */
  if (bind (sock, (void *)&addr, sizeof (addr)) == -1)
    error ("bind addr failed");

  /* listen */
  if (listen (sock, BACKLOG) == -1)
    error ("listen failed");

  for (client_t *clnt;;)
    {
      if (!(clnt = malloc (sizeof (client_t))))
        error ("malloc failed");

      /* init clnt.len */
      clnt->len = sizeof (clnt->addr);

      /* accept */
      clnt->sock = accept (sock, (void *)&clnt->addr, &clnt->len);

      if (clnt->sock == -1)
        continue;

      printf ("new request from %s:%d\n", inet_ntoa (clnt->addr.sin_addr),
              ntohs (clnt->addr.sin_port));

      if (threadpool_post (&pool, serve, clnt) != 0)
        error ("post task failed");
    }

  threadpool_free (&pool);
  close (sock);
}

static void
serve (void *arg)
{
  client_t *clnt = (client_t *)arg;
  int sock = clnt->sock;

  char method[16], uri[64], version[16];
  FILE *in, *out, *src;

  static _Thread_local char buff[BUFF_SIZE];
  size_t read;

  if (!(in = fdopen (sock, "r")) || !(out = fdopen (sock, "w")))
    goto err;

  if (!fgets (buff, BUFF_SIZE, in))
    goto err2;

  if (sscanf (buff, "%s %s %s", method, uri, version) != 3)
    goto err2;

  /* only accept GET & HTTP/1.1 */
  if (!is_equal (method, "GET") || !is_equal (version, "HTTP/1.1"))
    goto err2;

  /* respond 404 if the target file fails to open */
  if (!(src = fopen (uri + 1, "r")))
    {
      if (send_header (out, 404, 13))
        send_data (out, "404 NOT FOUND", 13);
      goto err2;
    }

  struct stat info;
  if (fstat (fileno (src), &info) != 0)
    goto err3;

  if (!send_header (out, 200, info.st_size))
    goto err3;

  /* send file content */
  for (; (read = fread (buff, 1, BUFF_SIZE, src));)
    if (!send_data (out, buff, read))
      goto err3;

err3:
  fclose (src);

err2:
  fclose (out);
  fclose (in);

err:
  close (sock);
  free (clnt);
}

static bool
send_header (FILE *out, int code, size_t len)
{
  static const char s200[] = "HTTP/1.1 200 OK\r\n";

  static const char s404[] = "HTTP/1.1 404 NOT FOUND\r\n";

  static const char header[] = "Server: tinyhttpd\r\n"
                               "Content-type: text/html\r\n"
                               "Content-length: " DEFAULT_LENGTH;

  size_t status_len;
  const char *status_src;
  size_t header_len = sizeof (header) - 1;

  switch (code)
    {
    case 200:
      status_len = sizeof (s200) - 1;
      status_src = s200;
      break;

    case 404:
      status_len = sizeof (s404) - 1;
      status_src = s404;
      break;

    default:
      /* only support 404 and 200 */
      return false;
    }

  /* send status line */
  if (!send_data (out, status_src, status_len))
    return false;

  /* send parital header */
  if (!send_data (out, header, header_len))
    return false;

  char conv[24];
  if (snprintf (conv, 24, "%lu", len) <= 0)
    return false;

  /* send length */
  if (!send_data (out, conv, strlen (conv)))
    return false;

  /* send newline */
  return send_data (out, "\r\n\r\n", 4);
}

static inline bool
send_data (FILE *out, const void *data, size_t size)
{
  return size ? fwrite (data, 1, size, out) : true;
}
