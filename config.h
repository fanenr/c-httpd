#ifndef CONFIG_H
#define CONFIG_H

#define ROOT "."
#define PORT 8080
#define THREADS 16
#define BACKLOG 32
#define FLAGS SERVER_REUSEADDR

static const char *indexs[] = { "index.htm", "index.html" };
static const int indexs_size = sizeof (indexs) / sizeof (*indexs);

#endif
