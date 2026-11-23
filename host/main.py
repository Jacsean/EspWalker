#!/usr/bin/env python3
"""Esp32Walker 中心端（PC 上位机）。

职责（对应 docs/01-architecture.md 的分层）：
  - TCP 服务端：接收 ESP32-S3 的 hello / status / proximity / wake / event / ack，
    以及 JPEG / PCM 二进制帧；
  - 指令下发：move / stop / capture / led / query / config；
  - 语音对话编排：wake → rec 采集 PCM → ASR → LLM → TTS → play 回传。

运行：
  python main.py --config config.json
"""
import argparse
import json
import os
import socket
import threading
import time

from protocol import FrameDecoder, encode_json, encode_bin, FRAME_JPEG, FRAME_PCM_UP, FRAME_PCM_DOWN

import llm
import asr
import tts


class Client:
    """一个 ESP32 连接。"""

    def __init__(self, sock, addr, cfg):
        self.sock = sock
        self.addr = addr
        self.cfg = cfg
        self.decoder = FrameDecoder(self._on_json, self._on_bin)
        self.send_lock = threading.Lock()
        self.recording = False
        self.pcm = bytearray()

        sp = cfg.get("llm", {}).get("system_prompt")
        self.chat_history = [{"role": "system", "content": sp}] if sp else []

    def send_json(self, obj):
        with self.send_lock:
            self.sock.sendall(encode_json(obj))

    def send_bin(self, ftype, payload):
        with self.send_lock:
            self.sock.sendall(encode_bin(ftype, payload))

    def run(self):
        while True:
            try:
                data = self.sock.recv(4096)
            except OSError:
                break
            if not data:
                break
            self.decoder.feed(data)
        print(f"[server] ESP32 {self.addr} disconnected")

    # ---------- 上行处理 ----------
    def _on_json(self, text):
        try:
            msg = json.loads(text)
        except json.JSONDecodeError:
            print(f"[recv] {text}")
            return
        t = msg.get("t")

        if t == "hello":
            print(f"[hello] fw={msg.get('fw')} chassis={msg.get('chassis')} cap={msg.get('cap')}")
        elif t == "ack":
            print(f"[ack] id={msg.get('id')} code={msg.get('code')} msg={msg.get('msg')}")
        elif t == "status":
            print(f"[status] moving={msg.get('moving')} dist={msg.get('dist_cm')}cm")
        elif t == "proximity":
            print(f"[proximity] zone={msg.get('zone')} dist={msg.get('dist_cm')}cm level={msg.get('level')}")
        elif t == "wake":
            print(f"[wake] word={msg.get('word')}")
            threading.Thread(target=self._voice_flow, args=(5000,), daemon=True).start()
        elif t == "event":
            print(f"[event] {msg}")
        else:
            print(f"[recv] {msg}")

    def _on_bin(self, ftype, payload):
        if ftype == FRAME_JPEG:
            os.makedirs("out", exist_ok=True)
            path = os.path.join("out", f"img_{int(time.time() * 1000)}.jpg")
            with open(path, "wb") as f:
                f.write(payload)
            print(f"[image] saved {path} ({len(payload)} bytes)")
        elif ftype == FRAME_PCM_UP:
            if self.recording:
                self.pcm.extend(payload)
        elif ftype == FRAME_PCM_DOWN:
            pass
        else:
            print(f"[bin] type=0x{ftype:02x} len={len(payload)}")

    # ---------- 语音对话链路 ----------
    def _voice_flow(self, duration_ms=5000):
        if self.recording:
            return
        self.recording = True
        self.pcm = bytearray()
        self.send_json({"t": "rec", "id": 0, "on": True, "ms": duration_ms})
        time.sleep(duration_ms / 1000 + 0.3)
        self.recording = False
        self.send_json({"t": "rec", "id": 0, "on": False})

        pcm = bytes(self.pcm)
        if len(pcm) < 1600:  # 不足 50 ms 视为无有效语音
            print("[voice] no speech detected")
            return

        text = asr.transcribe(pcm, 16000, self.cfg.get("asr", {}))
        if not text:
            print(f"[voice] ASR not configured, got PCM {len(pcm)} bytes (configure asr.engine)")
            return
        print(f"[voice] you said: {text}")

        self.chat_history.append({"role": "user", "content": text})
        reply = llm.chat(self.chat_history, self.cfg.get("llm", {}))
        if not reply:
            print("[voice] LLM not configured, skip reply (configure llm.enabled)")
            return
        self.chat_history.append({"role": "assistant", "content": reply})
        print(f"[bot] {reply}")

        audio = tts.synthesize(reply, self.cfg.get("tts", {}))
        if audio:
            self.send_json({"t": "play", "id": 0, "bytes": len(audio)})
            self.send_bin(FRAME_PCM_DOWN, audio)


def main():
    parser = argparse.ArgumentParser(description="Esp32Walker center host")
    parser.add_argument("--config", default="config.json")
    args = parser.parse_args()

    cfg = {}
    if os.path.exists(args.config):
        with open(args.config, encoding="utf-8") as f:
            cfg = json.load(f)

    listen = cfg.get("listen", {"host": "0.0.0.0", "port": 8888})
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((listen["host"], listen["port"]))
    srv.listen(1)
    print(f"[server] listening {listen['host']}:{listen['port']}")

    current = [None]

    def accept_loop():
        while True:
            sock, addr = srv.accept()
            print(f"[server] ESP32 connected from {addr}")
            c = Client(sock, addr, cfg)
            current[0] = c
            threading.Thread(target=c.run, daemon=True).start()

    threading.Thread(target=accept_loop, daemon=True).start()

    print("commands: move <vx> <vy> <w> [ms] | stop | capture | led <0|1> | "
          "query | config <stop_cm> <slow_cm> | talk [ms] | quit")
    while True:
        try:
            line = input("> ")
        except (EOFError, KeyboardInterrupt):
            break
        parts = line.strip().split()
        if not parts:
            continue
        cmd = parts[0].lower()
        c = current[0]

        if cmd == "quit":
            break
        if c is None:
            print("no ESP32 connected")
            continue

        if cmd == "move":
            vx = float(parts[1]) if len(parts) > 1 else 0.0
            vy = float(parts[2]) if len(parts) > 2 else 0.0
            w = float(parts[3]) if len(parts) > 3 else 0.0
            ms = int(parts[4]) if len(parts) > 4 else 0
            c.send_json({"t": "move", "id": 0, "vx": vx, "vy": vy, "omega": w, "ms": ms})
        elif cmd == "stop":
            c.send_json({"t": "stop", "id": 0})
        elif cmd == "capture":
            c.send_json({"t": "capture", "id": 0})
        elif cmd == "led":
            c.send_json({"t": "led", "id": 0, "on": len(parts) < 2 or parts[1] == "1"})
        elif cmd == "query":
            c.send_json({"t": "query", "id": 0})
        elif cmd == "config":
            if len(parts) >= 3:
                c.send_json({"t": "config", "id": 0,
                             "prox_stop_cm": int(parts[1]), "prox_slow_cm": int(parts[2])})
            else:
                print("usage: config <stop_cm> <slow_cm>")
        elif cmd == "talk":
            ms = int(parts[1]) if len(parts) > 1 else 5000
            threading.Thread(target=c._voice_flow, args=(ms,), daemon=True).start()
        else:
            print("unknown command")


if __name__ == "__main__":
    main()
