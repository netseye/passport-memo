#pragma once
#include "esp_err.h"
#include "memo_replay_export.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Lifecycle ownership: worker begin -> capture append -> joined worker finish.
 * Playback and note edits are excluded while ASR/capture is active. */
void memo_replay_init(void);
bool memo_replay_begin(void);
void memo_replay_append(const void *opus, size_t bytes, unsigned input_samples);
bool memo_replay_finish(const char *text);
bool memo_replay_matches(const char *text, uint32_t record_id);
bool memo_replay_bind(uint32_t record_id);
void memo_replay_forget(uint32_t record_id);

/* Setup HTTP task only: handlers are serialized; leaving setup waits for
 * httpd_stop() before the app worker can record, bind, or play another clip.
 * Do not call from a background/async HTTP handler. */
bool memo_replay_saved_info(uint32_t record_id, unsigned *duration_ms);
memo_export_result_t memo_replay_stream(uint32_t record_id,
                                       memo_replay_write_t write, void *ctx);

typedef void (*memo_replay_progress_t)(unsigned seconds, unsigned total, int level);
/* Synchronous to the app worker; the decoder uses a temporary task/stack. */
esp_err_t memo_replay_play(memo_replay_progress_t progress);
/* Button-safe: atomics only, no codec or Flash access. */
void memo_replay_stop(void);
void memo_replay_volume_step(int delta);
unsigned memo_replay_volume(void);
