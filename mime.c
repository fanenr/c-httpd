#include "mime.h"
#include <string.h>

static const char *table[][2] = {
  { ".html", "text/html" },
  { ".htm", "text/html" },
  { ".js", "text/javascript" },
  { ".css", "text/css" },
  { ".txt", "text/plain" },

  { ".png", "image/png" },
  { ".jpg", "image/jpeg" },
  { ".jpeg", "image/jpeg" },
  { ".gif", "image/gif" },
  { ".svg", "image/svg+xml" },
  { ".bmp", "image/x-ms-bmp" },
  { ".ico", "image/vnd.microsoft.icon" },

  { ".ttf", "font/ttf" },
  { ".otf", "font/otf" },
  { ".woff", "font/woff" },
  { ".woff2", "font/woff2" },

  { ".mp3", "audio/mpeg" },
  { ".wav", "audio/wav" },

  { ".mp4", "video/mp4" },
  { ".avi", "video/x-msvideo" },
  { ".mkv", "video/x-matroska" },
  { ".flv", "video/x-flv" },

  { ".gz", "application/gzip" },
  { ".tgz", "application/gzip" },
  { ".tar", "application/x-tar" },
  { ".bz", "application/x-bzip" },
  { ".bz2", "application/x-bzip2" },
  { ".zip", "application/zip" },
  { ".rar", "application/x-rar" },
  { ".7z", "application/x-7z-compressed" },

  { ".pdf", "application/pdf" },
  { ".json", "application/json" },
  { ".doc", "application/vnd.ms-word" },
  { ".xls", "application/vnd.ms-excel" },
  { ".ppt", "application/vnd.ms-powerpoint" },
};

static const size_t table_size = sizeof (table) / sizeof (*table);

const char *
mime_of (const char *path)
{
  const char *ext;

  if (!(ext = strrchr (path, '.')))
    return NULL;

  for (size_t i = 0; i < table_size; i++)
    if (strcmp (ext, table[i][0]) == 0)
      return table[i][1];

  return NULL;
}
