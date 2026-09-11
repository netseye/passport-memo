#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define MEMO_MAX_CHARS 240
#define MEMO_TEXT_BYTES 961
#define MEMO_HISTORY 32
#define MEMO_MAX_SECONDS 120
#define MEMO_RX_BYTES 8192
typedef enum {
  MEMO_HOME,
  MEMO_CONNECTING,
  MEMO_RECORDING,
  MEMO_FINISHING,
  MEMO_REVIEW,
  MEMO_HISTORY_PAGE,
  MEMO_SETTINGS,
  MEMO_ERROR,
  MEMO_PLAYBACK
} memo_phase_t;
typedef struct {
  uint32_t version, id, seconds;
  int64_t created;
  uint8_t done, synced, partial, reserved;
  char text[MEMO_TEXT_BYTES];
} memo_record_t;
typedef struct {
  uint8_t type, flags;
  int32_t sequence;
  uint32_t error;
  const uint8_t *payload;
  size_t size;
  bool final;
} memo_packet_t;
/* Reject malformed UTF-8 and controls. Limit without splitting a code point. */
bool memo_text_valid(const char *s, size_t *characters);
size_t memo_text_prefix(const char *s, size_t max_chars, size_t max_bytes);
size_t memo_request(uint8_t *out, size_t capacity, const void *payload, size_t size,
                    bool audio, bool final);
bool memo_response(const uint8_t *data, size_t size, memo_packet_t *packet);
bool memo_active(memo_phase_t phase);
bool memo_header_value(const char *s, size_t max_len);

/* Reassemble WebSocket frames and the client's smaller read chunks. */
typedef struct {
  size_t used, frame_received, frame_length;
  unsigned opcode;
  bool message_open, frame_active;
} memo_stream_t;
/* Returns -1 for invalid framing, 0 while incomplete, 1 for a full message. */
int memo_stream_feed(memo_stream_t *stream, uint8_t *buffer, size_t capacity,
                     unsigned opcode, size_t offset, size_t frame_length, bool final,
                     const void *data, size_t size);

typedef enum {
  MEMO_LINK_UNCONFIGURED,
  MEMO_LINK_PROVISIONING,
  MEMO_LINK_CONNECTING,
  MEMO_LINK_AUTH_FAILED,
  MEMO_LINK_NOT_FOUND,
  MEMO_LINK_FAILED,
  MEMO_LINK_SYNCING,
  MEMO_LINK_READY
} memo_link_state_t;
const char *memo_link_message(memo_link_state_t state);

/* HTTP rejection wins over transport/TLS symptoms during the WS upgrade. */
const char *memo_asr_connection_message(int http_status, bool tls_error,
                                        bool connected);

/* Error payload is length-delimited, not NUL-terminated. Return only fixed
 * local text, so provider payloads cannot leak credentials into UI/logs. */
const char *memo_asr_service_message(uint32_t code, const uint8_t *detail,
                                     size_t length);
