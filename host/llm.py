"""LLM 对话编排：OpenAI 兼容接口（可换通义千问 / 讯飞 / Kimi 等兼容端点）。

config 结构（config.json 中 llm 段）：
  {
    "enabled": true,
    "base_url": "https://api.openai.com/v1",
    "api_key": "...",
    "model": "gpt-4o-mini",
    "system_prompt": "你是一只可爱的桌面机器人，回答简短友好。"
  }
"""
import json
import urllib.request
import urllib.error


def chat(messages, config):
    """多轮对话。返回回复文本；未启用或出错时返回 None。"""
    if not config.get("enabled"):
        return None

    base = config.get("base_url", "https://api.openai.com/v1").rstrip("/")
    url = base + "/chat/completions"
    body = json.dumps(
        {
            "model": config.get("model", "gpt-4o-mini"),
            "messages": messages,
            "temperature": 0.7,
        }
    ).encode("utf-8")

    req = urllib.request.Request(
        url,
        data=body,
        headers={
            "Content-Type": "application/json",
            "Authorization": "Bearer " + config.get("api_key", ""),
        },
    )
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            data = json.loads(resp.read().decode("utf-8"))
        return data["choices"][0]["message"]["content"].strip()
    except (urllib.error.URLError, KeyError, IndexError, json.JSONDecodeError) as e:
        print(f"[llm] error: {e}")
        return None
