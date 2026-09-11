#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t record_id;
  unsigned selected;
  bool confirm_delete;
} memo_actions_t;
typedef enum { MEMO_ACTION_UP, MEMO_ACTION_DOWN, MEMO_ACTION_OK, MEMO_ACTION_BACK } memo_action_key_t;
typedef enum { MEMO_ACTION_NONE, MEMO_ACTION_CLOSE, MEMO_ACTION_SYNC, MEMO_ACTION_DELETE } memo_action_effect_t;

/* Pin the note ID when opening; indices may change as history is edited. */
bool memo_actions_open(memo_actions_t *state, uint32_t record_id);
memo_action_effect_t memo_actions_step(memo_actions_t *state, memo_action_key_t key);
