#pragma once
#include <stddef.h>
#include <stdint.h>
/* One complete packet per page: no continuation or hidden buffering. */
size_t memo_ogg_page(uint8_t *out, size_t capacity, const uint8_t *packet, size_t bytes,
                     uint32_t serial, uint32_t sequence, uint64_t granule,
                     uint8_t flags);
size_t memo_ogg_headers(uint8_t *out, size_t capacity, uint32_t serial,
                        uint16_t preskip);
