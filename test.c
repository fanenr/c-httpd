#include "httpd.h"
#include "util.h"

#include <signal.h>

int
main (int argc, char **args)
{
  if (argc < 2)
    error ("Usage: %s <port> <dir>", args[0]);

  struct sigaction act;
  act.sa_handler = SIG_IGN;
  if (sigaction (SIGPIPE, &act, NULL) != 0)
    abort ();

  server_t serv;

  server_config_t conf = {
    .port = atoi (args[1]),
    .root = args[2],
  };

  if (server_init (&serv, &conf) != 0)
    abort ();

  for (;;)
    server_poll (&serv);

  server_free (&serv);
}
