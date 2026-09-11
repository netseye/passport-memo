#!/usr/bin/env python3
"""Integration test: libopus packets + the firmware Ogg muxer + FFmpeg decode.

Requires system libopus and ffmpeg. This does not exercise Espressif's encoder.
"""
import ctypes as C
import ctypes.util
import math
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
lib = C.CDLL(ctypes.util.find_library('opus') or '/opt/homebrew/lib/libopus.dylib')
lib.opus_encoder_create.argtypes = [C.c_int, C.c_int, C.c_int, C.POINTER(C.c_int)]
lib.opus_encoder_create.restype = C.c_void_p
lib.opus_encoder_ctl.argtypes = [C.c_void_p, C.c_int]
lib.opus_encode.argtypes = [C.c_void_p, C.POINTER(C.c_int16), C.c_int, C.c_void_p, C.c_int]
lib.opus_encoder_destroy.argtypes = [C.c_void_p]
class ReplayInfo(C.Structure):
    _fields_ = [(name, C.c_uint32) for name in ('frames', 'samples', 'data_crc', 'record_id')] + [('text_hash', C.c_uint8 * 32)]

Read = C.CFUNCTYPE(C.c_bool, C.c_void_p, C.c_size_t, C.c_void_p, C.c_size_t)
Write = C.CFUNCTYPE(C.c_bool, C.c_void_p, C.c_void_p, C.c_size_t)
with tempfile.TemporaryDirectory(prefix='memo-ogg-') as directory:
    tmp = Path(directory)
    subprocess.run(['cc', '-shared', '-fPIC', '-Imain', 'main/memo_ogg.c', 'main/memo_replay_format.c', 'main/memo_replay_export.c', '-o', str(tmp/'ogg.so')], cwd=ROOT, check=True)
    ogg = C.CDLL(str(tmp/'ogg.so'))
    ogg.memo_ogg_headers.argtypes = [C.c_void_p, C.c_size_t, C.c_uint32, C.c_uint16]
    ogg.memo_ogg_headers.restype = C.c_size_t
    ogg.memo_ogg_page.argtypes = [C.c_void_p, C.c_size_t, C.c_void_p, C.c_size_t, C.c_uint32, C.c_uint32, C.c_uint64, C.c_uint8]
    ogg.memo_ogg_page.restype = C.c_size_t
    ogg.memo_replay_crc.argtypes = [C.c_uint32, C.c_void_p, C.c_size_t]
    ogg.memo_replay_crc.restype = C.c_uint32
    ogg.memo_replay_export.argtypes = [C.POINTER(ReplayInfo), Read, Write, C.c_void_p]
    for frames in [1, 5, 50, 150]:
        error = C.c_int()
        encoder = lib.opus_encoder_create(16000, 1, 2048, C.byref(error))
        assert encoder and error.value == 0
        try:
            for request, value in [(4002, 16000), (4006, 0), (4010, 0), (4016, 0)]:
                assert lib.opus_encoder_ctl(encoder, request, C.c_int(value)) == 0
            delay = C.c_int()
            assert lib.opus_encoder_ctl(encoder, 4027, C.byref(delay)) == 0
            assert delay.value * 3 == 312, delay.value
            page = C.create_string_buffer(2048)
            encoded = C.create_string_buffer(1275)
            n = ogg.memo_ogg_headers(page, len(page), 12345, 312)
            data = bytearray(page.raw[:n])
            packets = bytearray()
            for frame in range(frames + 1):
                padding = frame == frames
                pcm = (C.c_int16*320)(*[0 if padding else int(8000 * math.sin(2*math.pi*440*(frame*320+i)/16000)) for i in range(320)])
                size = lib.opus_encode(encoder, pcm, 320, encoded, len(encoded))
                assert size == 40, size
                packets.extend(encoded.raw[:size])
                granule = frames * 960 + 312 if padding else (frame + 1) * 960
                n = ogg.memo_ogg_page(page, len(page), encoded, size, 12345, frame+2, granule, 4 if padding else 0)
                assert n
                data.extend(page.raw[:n])
            stream = tmp/'sample.ogg'
            stream.write_bytes(data)
            decoded = subprocess.run(['ffmpeg', '-v', 'error', '-i', str(stream), '-ar', '16000', '-ac', '1', '-f', 's16le', '-'], capture_output=True, check=True).stdout
            assert len(decoded) == frames * 640, (frames, len(decoded))
            for interrupted in (False, True):
                raw = bytes(packets[:-40] if interrupted else packets)
                samples = frames * 320 - (104 if interrupted else 0)
                info = ReplayInfo(frames=len(raw)//40, samples=samples,
                                  data_crc=ogg.memo_replay_crc(0xffffffff, raw, len(raw)) ^ 0xffffffff,
                                  record_id=1)
                exported = bytearray()
                @Read
                def read(_ctx, offset, dest, count):
                    if offset + count > len(raw): return False
                    C.memmove(dest, raw[offset:offset+count], count)
                    return True
                @Write
                def write(_ctx, source, count):
                    exported.extend(C.string_at(source, count))
                    return True
                assert ogg.memo_replay_export(C.byref(info), read, write, None) == 0
                stream.write_bytes(exported)
                playback = subprocess.run(['ffmpeg', '-v', 'error', '-i', str(stream), '-ar', '16000', '-ac', '1', '-f', 's16le', '-'], capture_output=True, check=True).stdout
                assert len(playback) == samples * 2, (frames, interrupted, len(playback))
                if not interrupted:
                    assert playback == decoded
                    if frames == 150 and len(sys.argv) == 2:
                        Path(sys.argv[1]).write_bytes(exported)
            print(f'{frames*20} ms: {len(data)} Ogg bytes, {len(decoded)} decoded PCM bytes; duration and EOS PASS')
            print('  Replay export: identical decoded samples; interrupted capture trim PASS')
        finally:
            lib.opus_encoder_destroy(encoder)
