#include "httpd.h"
#include <signal.h>
#include <stdlib.h>

#define PORT 3354
#define THREADS 16
#define BACKLOG 48
#define ROOT "./blog"
#define FLAGS SERVER_REUSEADDR

int
main (void)
{
  struct sigaction act;
  act.sa_handler = SIG_IGN;

  if (sigaction (SIGPIPE, &act, NULL) != 0)
    return 1;

  server_t serv;

  if (server_init (&serv, PORT, ROOT, THREADS, BACKLOG, FLAGS) != 0)
    abort ();

  for (;;)
    server_poll (&serv);

  server_free (&serv);
}
