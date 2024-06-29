#include "mime.h"
#include <string.h>

static const char *table[][2] = {
  { ".css", "text/css" },
  { ".htm", "text/html" },
  { ".html", "text/html" },
  { ".txt", "text/plain" },
  { ".js", "text/javascript" },

  { ".png", "image/png" },
  { ".bmp", "image/bmp" },
  { ".gif", "image/gif" },
  { ".jpg", "image/jpeg" },
  { ".jpeg", "image/jpeg" },
  { ".webp", "image/webp" },
  { ".ico", "image/x-icon" },
  { ".svg", "image/svg+xml" },

  { ".ttf", "font/ttf" },
  { ".otf", "font/otf" },
  { ".woff", "font/woff" },
  { ".woff2", "font/woff2" },

  { ".wav", "audio/wav" },
  { ".aac", "audio/aac" },
  { ".mp3", "audio/mpeg" },
  { ".flac", "audio/flac" },

  { ".mp4", "video/mp4" },
  { ".webm", "video/webm" },
  { ".flv", "video/x-flv" },
  { ".avi", "video/x-msvideo" },
  { ".mkv", "video/x-matroska" },

  { ".zip", "application/zip" },
  { ".gz", "application/gzip" },
  { ".tgz", "application/gzip" },
  { ".bz", "application/x-bzip" },
  { ".tar", "application/x-tar" },
  { ".rar", "application/x-rar" },
  { ".bz2", "application/x-bzip2" },
  { ".7z", "application/x-7z-compressed" },

  { ".pdf", "application/pdf" },
  { ".json", "application/json" },
  { ".epub", "application/epub+zip" },
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
