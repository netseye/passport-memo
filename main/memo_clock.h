#pragma once
#include <stdint.h>
/* Display Beijing time (UTC+8), without changing the system timezone or epochs.
 * Until the existing SNTP validity threshold is reached, show --:--. */
void memo_clock_format(char out[6], int64_t utc_seconds);
