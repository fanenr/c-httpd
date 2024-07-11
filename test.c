#include "httpd.h"
#include <signal.h>
#include <stdlib.h>

int
main (void)
{
  struct sigaction act;
  act.sa_handler = SIG_IGN;

  if (sigaction (SIGPIPE, &act, NULL) != 0)
    abort ();

  server_t serv;

  server_config_t conf = {
    .port = 3354,
    .root = "./blog",
  };

  if (server_init (&serv, &conf) != 0)
    abort ();

  for (;;)
    server_poll (&serv);

  server_free (&serv);
}
