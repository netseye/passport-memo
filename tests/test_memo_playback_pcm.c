#include "memo_playback_pcm.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

int main(void) {
  assert(!memo_playback_prepare(NULL, 1));
  int16_t quiet[] = {0, 100, -100, 1000, -1000, 12000, -12000};
  memo_playback_prepare(quiet, 7);
  assert(quiet[0] == 0 && quiet[1] == 200 && quiet[2] == -200);
  assert(quiet[3] == 2000 && quiet[4] == -2000);
  assert(quiet[5] == 24000 && quiet[6] == -24000);
  // Exhaustive signed-PCM range: no overflow, sign change or non-monotonic knee.
  int last = INT16_MIN;
  for (int i = INT16_MIN; i <= INT16_MAX; i++) {
    int16_t sample = i;
    assert(memo_playback_prepare(&sample, 1) <= 100);
    assert(sample >= last && sample > -32000 && sample < 32000);
    assert((i < 0 && sample < 0) || (i == 0 && sample == 0) || (i > 0 && sample > 0));
    last = sample;
  }
  for (int i = 0; i <= INT16_MAX; i++) {
    int16_t pair[] = {i, -i};
    memo_playback_prepare(pair, 2);
    assert(pair[0] == -pair[1]);
  }
  puts("Replay gain, soft limit and all signed PCM values: PASS");
  return 0;
}
