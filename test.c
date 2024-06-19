#include "httpd.h"
#include <stdlib.h>

#define PORT 3354

int
main (void)
{
  server_t serv;

  if (server_init (&serv, PORT, "./static", 16, 48, SERVER_REUSEADDR) != 0)
    abort ();

  for (size_t i = 0; i < 4; i++)
    server_poll (&serv);

  server_free (&serv);
}
