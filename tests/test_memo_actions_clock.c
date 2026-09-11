#include "memo_actions.h"
#include "memo_clock.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

int main(void) {
  memo_actions_t s = {0};
  assert(!memo_actions_open(NULL, 42) && !memo_actions_open(&s, 0));
  assert(memo_actions_step(&s, MEMO_ACTION_OK) == MEMO_ACTION_NONE);
  assert(memo_actions_open(&s, 42));
  assert(memo_actions_step(&s, MEMO_ACTION_OK) == MEMO_ACTION_NONE);
  assert(s.confirm_delete && s.selected == 0 && s.record_id == 42);
  assert(memo_actions_step(&s, MEMO_ACTION_OK) == MEMO_ACTION_CLOSE);
  assert(memo_actions_open(&s, 42));
  assert(memo_actions_step(&s, MEMO_ACTION_OK) == MEMO_ACTION_NONE);
  assert(memo_actions_step(&s, MEMO_ACTION_DOWN) == MEMO_ACTION_NONE);
  assert(memo_actions_step(&s, MEMO_ACTION_OK) == MEMO_ACTION_DELETE);
  assert(s.record_id == 42);
  assert(memo_actions_open(&s, 99));
  assert(memo_actions_step(&s, MEMO_ACTION_UP) == MEMO_ACTION_NONE && s.selected == 2);
  assert(memo_actions_step(&s, MEMO_ACTION_DOWN) == MEMO_ACTION_NONE && s.selected == 0);
  assert(memo_actions_step(&s, MEMO_ACTION_DOWN) == MEMO_ACTION_NONE);
  assert(memo_actions_step(&s, MEMO_ACTION_OK) == MEMO_ACTION_SYNC);
  assert(memo_actions_step(&s, MEMO_ACTION_BACK) == MEMO_ACTION_CLOSE);
  assert(memo_actions_open(&s, 42));
  assert(memo_actions_step(&s, (memo_action_key_t)99) == MEMO_ACTION_NONE);
  s.selected = UINT_MAX;
  assert(memo_actions_step(&s, MEMO_ACTION_OK) == MEMO_ACTION_CLOSE);
  assert(!memo_actions_open(&s, 0) && !s.record_id && !s.confirm_delete && !s.selected);
  assert(memo_actions_step(&s, MEMO_ACTION_OK) == MEMO_ACTION_NONE);

  char clock[6], expected[6];
  const int64_t epoch = INT64_C(1704067200);
  memo_clock_format(clock, 0); assert(!strcmp(clock, "--:--"));
  memo_clock_format(clock, INT64_MIN); assert(!strcmp(clock, "--:--"));
  memo_clock_format(clock, epoch); assert(!strcmp(clock, "--:--"));
  for (unsigned minute = 0; minute < 1440; minute++) {
    unsigned local = (minute + 480) % 1440;
    snprintf(expected, sizeof(expected), "%02u:%02u", local / 60, local % 60);
    memo_clock_format(clock, epoch + minute * 60 + 1);
    assert(!strcmp(clock, expected));
    memo_clock_format(clock, epoch + minute * 60 + 59);
    assert(!strcmp(clock, expected));
  }
  memo_clock_format(clock, epoch + 16 * 3600 - 1); assert(!strcmp(clock, "23:59"));
  memo_clock_format(clock, epoch + 16 * 3600); assert(!strcmp(clock, "00:00"));
  memo_clock_format(clock, INT64_MAX); assert(strlen(clock) == 5 && clock[2] == ':');
  puts("Actions and clock: safe defaults, navigation, pinned IDs, UTC+8 and minute/day boundaries PASS");
}
