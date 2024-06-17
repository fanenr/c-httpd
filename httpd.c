#include "threadpool.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

/* config */

#define PORT 3354
#define BACKLOG 16
#define THREADS 16
#define LENGTH "4096"
#define BUFF_SIZE 4096

char sts_200[] = "HTTP/1.1 200 OK\r\n";

char sts_404[] = "HTTP/1.1 404 NOT FOUND\r\n";

char msg_header[] = "Server: tinyhttpd\r\n"
                    "Content-type: text/html\r\n"
                    "Content-length: " LENGTH "\r\n\r\n";

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

      printf ("new connection: %s:%d\n", inet_ntoa (clnt->addr.sin_addr),
              ntohs (clnt->addr.sin_port));

      if (threadpool_post (&pool, serve, clnt) != 0)
        error ("post task failed");
    }

  threadpool_free (&pool);
  close (sock);
}

static void
serve_404 (int sock)
{
  /* send status line */
  if (send (sock, sts_404, sizeof (sts_404) - 1, 0) == -1)
    return;

  /* send message header */
  if (send (sock, msg_header, sizeof (msg_header) - 1, 0) == -1)
    return;

  send (sock, "404 NOT FOUND", 13, 0);
}

static void
serve (void *arg)
{
  client_t *clnt = arg;
  int sock = clnt->sock;

  char method[16];
  char uri[64];
  char pro[16];

  FILE *source;
  FILE *input;

  if (!(input = fdopen (sock, "r")))
    goto err;

  /* parse method */
  if (fscanf (input, "%s", method) != 1)
    goto err2;

  /* parse uri */
  if (fscanf (input, "%s", uri) != 1)
    goto err2;

  /* parse pro */
  if (fscanf (input, "%s", pro) != 1)
    goto err2;

  /* only accept GET & HTTP/1.1 */
  if (!is_equal (method, "GET") || !is_equal (pro, "HTTP/1.1"))
    goto err2;

  /* respond 404 if the target file fails to open */
  if (!(source = fopen (uri + 1, "r")))
    {
      serve_404 (sock);
      goto err2;
    }

  /* send status line */
  if (send (sock, sts_200, sizeof (sts_200) - 1, 0) == -1)
    goto err3;

  /* send message header */
  if (send (sock, msg_header, sizeof (msg_header) - 1, 0) == -1)
    goto err3;

  char buff[BUFF_SIZE];
  size_t read;

  if (!(read = fread (buff, 1, BUFF_SIZE, source)))
    goto err3;

  /* send message body */
  send (sock, buff, read, 0);

err3:
  fclose (source);

err2:
  fclose (input);

err:
  close (sock);
  free (clnt);
}
