import json
import os

settings_path = os.path.expandvars(r"%APPDATA%\Zed\settings.json")

data = {
    "edit_predictions": {"allow_data_collection": "no"},
    "icon_theme": {"mode": "dark", "light": "Zed (Default)", "dark": "Zed (Default)"},
    "agent_servers": {
        "your_agent": {"type": "custom", "command": "path_to_executable"},
        "opencode": {"type": "registry"},
        "codex-acp": {"type": "registry"},
    },
    "language_models": {
        "openai_compatible": {},
        "lmstudio": {"api_url": "http://127.0.0.1:1234"},
    },
    "agent": {
        "use_modifier_to_send": True,
        "tool_permissions": {
            "tools": {
                "delete_path": {
                    "always_allow": [{"pattern": "^X:/scratch/_error_probe_backup/"}]
                },
                "terminal": {"default": "allow"},
            }
        },
        "default_profile": "write",
        "default_model": {
            "provider": "opencode",
            "model": "go/qwen3.6-plus",
            "enable_thinking": False,
        },
        "favorite_models": [
            {
                "provider": "deepseek",
                "model": "deepseek-v4-pro",
                "enable_thinking": True,
                "effort": "max",
            },
            {
                "provider": "zed.dev",
                "model": "gpt-5.4",
                "enable_thinking": True,
                "effort": "high",
            },
            {
                "provider": "opencode",
                "model": "go/deepseek-v4-pro",
                "enable_thinking": True,
                "effort": "max",
            },
            {
                "provider": "opencode",
                "model": "go/deepseek-v4-flash",
                "enable_thinking": True,
                "effort": "max",
            },
            {"provider": "opencode", "model": "go/kimi-k2.6", "enable_thinking": False},
            {
                "provider": "opencode",
                "model": "go/qwen3.6-plus",
                "enable_thinking": False,
            },
            {
                "provider": "opencode",
                "model": "go/qwen3.7-plus",
                "enable_thinking": False,
            },
        ],
        "model_parameters": [],
    },
    "cli_default_open_behavior": "existing_window",
    "ui_font_size": 16,
    "buffer_font_size": 15,
    "theme": {"mode": "dark", "light": "Ayu Light", "dark": "Ayu Dark"},
}

os.makedirs(os.path.dirname(settings_path), exist_ok=True)
with open(settings_path, "w") as f:
    json.dump(data, f, indent=2)
    f.write("\n")

print(f"Fixed: {settings_path}")
