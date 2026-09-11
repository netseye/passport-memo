#include "memo_clock.h"
#include <string.h>

void memo_clock_format(char out[6], int64_t utc) {
  if (utc <= INT64_C(1704067200)) {
    memcpy(out, "--:--", 6);
    return;
  }
  unsigned minutes = (unsigned)((utc / 60 + 8 * 60) % (24 * 60));
  unsigned h = minutes / 60, m = minutes % 60;
  out[0] = '0' + h / 10;
  out[1] = '0' + h % 10;
  out[2] = ':';
  out[3] = '0' + m / 10;
  out[4] = '0' + m % 10;
  out[5] = 0;
}
