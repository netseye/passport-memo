#include "memo_core.h"
#include <string.h>
static uint32_t be32(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static size_t rune(const unsigned char *s, size_t left) {
  if (!left || !s[0])
    return 0;
  if (s[0] < 128)
    return (s[0] >= 32 || s[0] == '\n' || s[0] == '\t') ? 1 : 0;
  size_t n = s[0] >= 0xc2 && s[0] <= 0xdf   ? 2
             : s[0] >= 0xe0 && s[0] <= 0xef ? 3
             : s[0] >= 0xf0 && s[0] <= 0xf4 ? 4
                                            : 0;
  if (!n || n > left)
    return 0;
  for (size_t i = 1; i < n; i++)
    if ((s[i] & 0xc0) != 0x80)
      return 0;
  if ((s[0] == 0xe0 && s[1] < 0xa0) || (s[0] == 0xed && s[1] >= 0xa0) ||
      (s[0] == 0xf0 && s[1] < 0x90) || (s[0] == 0xf4 && s[1] >= 0x90))
    return 0;
  return n;
}
bool memo_text_valid(const char *s, size_t *characters) {
  if (!s)
    return false;
  size_t len = strlen(s), off = 0, count = 0;
  if (len >= MEMO_TEXT_BYTES)
    return false;
  while (off < len) {
    size_t n = rune((const unsigned char *)s + off, len - off);
    if (!n)
      return false;
    off += n;
    count++;
  }
  if (characters)
    *characters = count;
  return count <= MEMO_MAX_CHARS;
}
size_t memo_text_prefix(const char *s, size_t max_chars, size_t max_bytes) {
  size_t off = 0, len = strlen(s), count = 0;
  while (off < len && count < max_chars) {
    size_t n = rune((const unsigned char *)s + off, len - off);
    if (!n || off + n > max_bytes)
      break;
    off += n;
    count++;
  }
  return off;
}
size_t memo_request(uint8_t *out, size_t cap, const void *payload, size_t size,
                    bool audio, bool final) {
  if (!out || cap < 8 || size > cap - 8 || size > UINT32_MAX || (size && !payload))
    return 0;
  out[0] = 0x11;
  out[1] = (audio ? 0x20 : 0x10) | (final ? 2 : 0);
  out[2] = audio ? 0 : 0x10;
  out[3] = 0;
  out[4] = size >> 24;
  out[5] = size >> 16;
  out[6] = size >> 8;
  out[7] = size;
  if (size)
    memmove(out + 8, payload, size);
  return size + 8;
}
bool memo_response(const uint8_t *d, size_t n, memo_packet_t *p) {
  if (!d || !p || n < 8 || d[0] >> 4 != 1 || (d[0] & 15) == 0)
    return false;
  memset(p, 0, sizeof(*p));
  size_t off = (d[0] & 15) * 4u;
  p->type = d[1] >> 4;
  p->flags = d[1] & 15;
  p->final = (p->flags & 2) != 0;
  /* Full request uses no compression; the documented server mirrors it. */
  if ((d[2] & 15) != 0 || p->flags > 3 || off > n)
    return false;
  if (p->type == 15) {
    if (n - off < 4)
      return false;
    p->error = be32(d + off);
    off += 4;
  } else if (p->type == 9) {
    if (d[2] >> 4 != 1)
      return false;
    if (p->flags & 1) {
      if (n - off < 4)
        return false;
      p->sequence = (int32_t)be32(d + off);
      off += 4;
    }
  } else
    return false;
  if (n - off < 4)
    return false;
  p->size = be32(d + off);
  off += 4;
  if (p->size != n - off)
    return false;
  p->payload = d + off;
  return true;
}
bool memo_active(memo_phase_t p) {
  return p == MEMO_CONNECTING || p == MEMO_RECORDING || p == MEMO_FINISHING;
}
bool memo_header_value(const char *s, size_t max_len) {
  if (!s || strlen(s) > max_len)
    return false;
  for (; *s; s++)
    if ((unsigned char)*s < 32 || (unsigned char)*s > 126)
      return false;
  return true;
}

int memo_stream_feed(memo_stream_t *s, uint8_t *buffer, size_t cap, unsigned opcode,
                     size_t offset, size_t length, bool final, const void *data,
                     size_t size) {
  if (!s || !buffer || (size && !data) || (opcode != 2 && opcode != 0))
    return -1;
  if (offset == 0) {
    if (s->frame_active)
      return -1;
    if (opcode == 2) {
      if (s->message_open)
        return -1;
      s->used = 0;
      s->message_open = true;
    } else if (!s->message_open)
      return -1;
    s->frame_active = true;
    s->frame_received = 0;
    s->frame_length = length;
    s->opcode = opcode;
  }
  if (!s->frame_active || s->opcode != opcode || s->frame_length != length ||
      s->frame_received != offset || offset > length || size > length - offset ||
      s->used > cap || size > cap - s->used || length - offset > cap - s->used)
    return -1;
  if (size)
    memcpy(buffer + s->used, data, size);
  s->used += size;
  s->frame_received += size;
  if (s->frame_received == length) {
    s->frame_active = false;
    if (final) {
      s->message_open = false;
      return 1;
    }
  }
  return 0;
}

const char *memo_link_message(memo_link_state_t state) {
  switch (state) {
  case MEMO_LINK_UNCONFIGURED:
    return "请先配置路由器 Wi-Fi";
  case MEMO_LINK_PROVISIONING:
    return "配置热点已开启，退出后连接路由器";
  case MEMO_LINK_CONNECTING:
    return "正在连接路由器 Wi-Fi…";
  case MEMO_LINK_AUTH_FAILED:
    return "Wi-Fi认证失败，请核对路由器密码";
  case MEMO_LINK_NOT_FOUND:
    return "找不到 Wi-Fi，请检查名称和2.4G网络";
  case MEMO_LINK_FAILED:
    return "Wi-Fi连接中断，正在重试";
  case MEMO_LINK_SYNCING:
    return "Wi-Fi已连接，正在校时…";
  case MEMO_LINK_READY:
    return "网络已就绪，可以开始录音";
  default:
    return "正在检查网络…";
  }
}

const char *memo_asr_connection_message(int status, bool tls_error, bool connected) {
  if (status == 401)
    return "火山鉴权失败(401)，请核对语音凭据";
  if (status == 403)
    return "火山拒绝访问(403)，请核对资源权限";
  if (status == 429)
    return "火山请求限流(429)，请稍后重试";
  if (status >= 500 && status <= 599)
    return "火山服务暂不可用，请稍后重试";
  if (status >= 400 && status <= 499)
    return "火山拒绝握手，请核对接口配置";
  if (tls_error)
    return "安全连接失败，请检查网络和校时";
  return connected ? "识别连接中断，已有文字保留" : "未连上识别服务，请检查网络";
}

static bool contains(const uint8_t *data, size_t length, const char *word) {
  size_t n = strlen(word);
  if (!data || n > length)
    return false;
  for (size_t i = 0; i <= length - n; i++)
    if (!memcmp(data + i, word, n))
      return true;
  return false;
}

const char *memo_asr_service_message(uint32_t code, const uint8_t *detail,
                                     size_t length) {
  // Read the provider's reason, not just the number: quota rejections can
  // concern different resources and require different actions.
  if (contains(detail, length, "quota exceeded")) {
    if (contains(detail, length, "concurrency"))
      return "并发额度受限，请核对资源";
    if (contains(detail, length, "audio_duration"))
      return "语音时长额度不足，请核对资源";
    return "配额受限，请核对资源与额度";
  }
  switch (code) {
  case 45000001:
    return "识别请求参数无效";
  case 45000002:
    return "未收到音频，请重新录音";
  case 45000081:
    return "音频上传超时，请检查网络";
  case 45000151:
    return "音频格式不正确，请检查编码";
  case 55000031:
    return "识别服务繁忙，请稍后重试";
  default:
    return "请检查资源配置或额度";
  }
}
