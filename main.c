#include "threadpool.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

/* config */

#define PORT 3354
#define BACKLOG 16
#define THREADS 16

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

      /* accept */
      clnt->sock = accept (sock, (void *)&clnt->addr, &clnt->len);

      if (clnt->sock == -1)
        continue;

      if (threadpool_post (&pool, serve, clnt) != 0)
        error ("post task failed");
    }

  threadpool_free (&pool);
  close (sock);
}

static void
serve (void *arg)
{
  client_t *clnt = arg;

  close (clnt->sock);
  free (clnt);
}
