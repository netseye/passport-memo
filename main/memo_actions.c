#include "memo_actions.h"

bool memo_actions_open(memo_actions_t *s, uint32_t id) {
  if (!s) return false;
  *s = (memo_actions_t){.record_id = id};
  return id != 0;
}
memo_action_effect_t memo_actions_step(memo_actions_t *s, memo_action_key_t key) {
  if (!s || !s->record_id) return MEMO_ACTION_NONE;
  unsigned count = s->confirm_delete ? 2 : 3;
  if (s->selected >= count) return MEMO_ACTION_CLOSE;
  if (key == MEMO_ACTION_BACK) return MEMO_ACTION_CLOSE;
  if (key == MEMO_ACTION_UP || key == MEMO_ACTION_DOWN) {
    s->selected = (s->selected + (key == MEMO_ACTION_UP ? count - 1 : 1)) % count;
  } else if (key == MEMO_ACTION_OK) {
    if (s->confirm_delete)
      return s->selected == 1 ? MEMO_ACTION_DELETE : MEMO_ACTION_CLOSE;
    if (s->selected == 0) {
      s->confirm_delete = true;
      s->selected = 0; // Default to keeping the note, never to deleting it.
    } else {
      return s->selected == 1 ? MEMO_ACTION_SYNC : MEMO_ACTION_CLOSE;
    }
  }
  return MEMO_ACTION_NONE;
}
