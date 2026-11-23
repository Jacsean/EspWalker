"""语音合成（TTS）适配层。

返回：PCM16 / 16 kHz / 单声道字节流（可直接下发 ESP32 播放）。
未配置引擎时返回 None。

真实项目可接入：
  - 本地：edge-tts（输出 mp3，需 ffmpeg 转 PCM16）、piper、espeak-ng；
  - 云端：OpenAI TTS、讯飞、阿里等。
"""


def synthesize(text, config):
    """config 结构（config.json 中 tts 段）：{"engine": "none"}。"""
    engine = config.get("engine", "none")
    if engine in ("none", ""):
        return None
    if engine == "edge_tts":
        return _edge_tts(text, config)
    print(f"[tts] unknown engine: {engine}")
    return None


def _edge_tts(text, config):
    """edge-tts 输出 mp3；ESP32 需要 PCM16，需用 ffmpeg 转码：
       ffmpeg -i out.mp3 -f s16le -acodec pcm_s16le -ar 16000 -ac 1 out.pcm
    此占位保留接入点。"""
    print("[tts] edge-tts outputs mp3; transcode to PCM16 before playback")
    return None
