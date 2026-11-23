"""语音识别（ASR）适配层。

输入：PCM16 / 16 kHz / 单声道字节流。
返回：识别文本；未配置引擎时返回 None（调用方据此降级提示）。

真实项目可接入：
  - 本地：openai-whisper、FunASR、Sherpa-ONNX；
  - 云端：OpenAI Whisper API、讯飞、阿里等。
"""
import urllib.request


def transcribe(pcm_bytes, sample_rate, config):
    """config 结构（config.json 中 asr 段）：{"engine": "none"}。"""
    engine = config.get("engine", "none")
    if engine in ("none", ""):
        return None
    if engine == "openai_whisper":
        return _openai_whisper(pcm_bytes, config)
    print(f"[asr] unknown engine: {engine}")
    return None


def _openai_whisper(pcm_bytes, config):
    """OpenAI Whisper 接口需要编码后的音频文件（webm/mp3 等），
    原始 PCM 需先用 ffmpeg 转码。此处仅作占位说明，保留接入点。"""
    print("[asr] openai_whisper needs a transcoded audio file; "
          "transcode raw PCM (e.g. ffmpeg) before calling /audio/transcriptions")
    return None
