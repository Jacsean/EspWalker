"""Esp32Walker 帧协议编解码（与固件 components/comm/comm.c 一致）。

同一 TCP 流上混合两类帧：
  - 文本帧：一行 UTF-8 JSON，以 '\\n' 结束；
  - 二进制帧：0xAA 0x57 | type | seq | len(4B 大端) | payload。
"""
import json
import struct

MAGIC = b"\xaa\x57"

FRAME_JPEG = 0x01
FRAME_PCM_UP = 0x02
FRAME_PCM_DOWN = 0x03


def encode_json(obj) -> bytes:
    """对象 → 一行 JSON（自动补 \\n）。"""
    return json.dumps(obj, ensure_ascii=False).encode("utf-8") + b"\n"


def encode_bin(ftype: int, payload: bytes) -> bytes:
    """二进制帧编码。"""
    return MAGIC + bytes([ftype, 0]) + struct.pack(">I", len(payload)) + payload


class FrameDecoder:
    """增量解析：从字节流中切出 JSON 行与二进制帧。"""

    def __init__(self, on_json, on_bin):
        self.on_json = on_json
        self.on_bin = on_bin
        self._state = "json"  # json | magic1 | hdr | payload
        self._json_buf = bytearray()
        self._hdr = bytearray()
        self._payload = bytearray()
        self._payload_len = 0
        self._ftype = 0

    def feed(self, data: bytes):
        for b in data:
            self._byte(b)

    def _byte(self, b: int):
        if self._state == "json":
            if b == 0xAA:
                self._state = "magic1"
            elif b == 0x0A:
                if self._json_buf:
                    self.on_json(self._json_buf.decode("utf-8", errors="replace"))
                    self._json_buf.clear()
            else:
                self._json_buf.append(b)
        elif self._state == "magic1":
            if b == 0x57:
                self._state = "hdr"
                self._hdr = bytearray()
            else:
                self._json_buf.append(0xAA)
                self._json_buf.append(b)
                self._state = "json"
        elif self._state == "hdr":
            self._hdr.append(b)
            if len(self._hdr) == 6:
                self._ftype = self._hdr[0]
                self._payload_len = int.from_bytes(self._hdr[2:6], "big")
                self._payload = bytearray()
                self._state = "payload"
        elif self._state == "payload":
            if len(self._payload) < self._payload_len:
                self._payload.append(b)
            if len(self._payload) >= self._payload_len:
                self.on_bin(self._ftype, bytes(self._payload))
                self._state = "json"
