#pragma once
#include <stddef.h>
#include <stdint.h>

/* Replay-only +6 dB with a continuous soft knee; never modifies captured Opus. */
unsigned memo_playback_prepare(int16_t *pcm, size_t samples);
