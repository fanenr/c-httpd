#include "httpd.h"
#include <stdlib.h>

#define PORT 3354
#define THREADS 16
#define BACKLOG 48
#define ROOT "./blog"
#define FLAGS HTTPD_SERVER_REUSEADDR

int
main (void)
{
  server_t serv;

  if (server_init (&serv, PORT, ROOT, THREADS, BACKLOG, FLAGS) != 0)
    abort ();

  for (int i = 0; i < 16; i++)
    server_poll (&serv);

  server_free (&serv);
}
