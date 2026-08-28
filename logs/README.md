# logs/ — AI Coding 日志

## 需要你做的事

1. **导出 AI 对话日志**：按 [《AI Coding 日志归集与提交手册》](https://github.com/open-vela/docs/blob/dev-ai-contest-2026/zh-cn/contest_2026/ai_coding_log_guide.md) 操作
2. **替换 `manifest.json` 中的 `<YOUR_GITHUB_LOGIN>`** 为你的 GitHub 用户名
3. **创建目录结构**：`logs/<github_login>/YYYY-MM-DD/`
4. **提交 JSONL 文件**到对应日期目录

## 目录结构

```
logs/
├── manifest.json                          # 会话清单（需更新 github_login）
└── <github_login>/                        # 你的 GitHub 用户名
    └── YYYY-MM-DD/                        # 日期
        └── claude-code__<session_id>.jsonl  # AI 对话日志
```

## 示例

```
logs/
├── manifest.json
└── mrpil/
    └── 2026-08-28/
        └── claude-code__abc123.jsonl
```

## 工具

- `claude-code` / `opencode` / `codex` / `kiro`
- 每个 `.jsonl` 每行一个事件
