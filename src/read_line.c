#include <errno.h>
#include "read_line.h"

ssize_t readLine(int fd, void *buffer, size_t n) {
  ssize_t numRead;
  size_t totRead;
  char *buf;
  char ch;
  int cr = 0;

  if (n <= 0 || buffer == NULL) {
    errno = EINVAL;
    return -1;
  }

  buf = buffer;

  totRead = 0;
  for (;;) {
    numRead = read(fd, &ch, 1);

    if (numRead == -1) {
      if (errno == EINTR)
        continue;
      else
        return -1;
    } else if (numRead == 0) {
      if (totRead == 0)
        return 0;
      else
        break;
    } else {
      if (ch == '\n' && cr) {
        buf--;
        break;
      }
      if (totRead < n) {
        totRead++;
        *buf++ = ch;
        if (ch == '\r') cr = 1;
        else cr = 0;
      }
    }
  }

  *buf = '\0';
  return totRead;
}
