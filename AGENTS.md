# Alway use Chinese to communicate with user.

<!-- TRELLIS:START -->
# Trellis Instructions

These instructions are for AI assistants working in this project.

This project is managed by Trellis. The working knowledge you need lives under `.trellis/`:

- `.trellis/workflow.md` — development phases, when to create tasks, skill routing
- `.trellis/spec/` — package- and layer-scoped coding guidelines (read before writing code in a given layer)
- `.trellis/workspace/` — per-developer journals and session traces
- `.trellis/tasks/` — active and archived tasks (PRDs, research, jsonl context)

If a Trellis command is available on your platform (e.g. `/trellis:finish-work`, `/trellis:continue`), prefer it over manual steps. Not every platform exposes every command.

If you're using Codex or another agent-capable tool, additional project-scoped helpers may live in:

- `.agents/skills/` — reusable Trellis skills
- `.codex/agents/` — optional custom subagents

Managed by Trellis. Edits outside this block are preserved; edits inside may be overwritten by a future `trellis update`.

<!-- TRELLIS:END -->

# Temporary Files

All temporary file should be located in the folder `.agent_tmp/`, including your search script, log files and so on.

# Git 操作授权（Mandatory）

`git commit` 与 `git push` **必须获得用户明确授权**，且**授权按对话轮次单轮有效**：

- 用户在某轮说了"提交"/"推送"→ 只授权**那一轮**内的这项操作。操作完成后授权即用尽。
- 授权**不跨轮延续**，除非同一轮内目标尚未完成（目标变更时授权自动失效，需重新取得）。
- 用户说"继续 XX"、"开始 XX"、"做 XX" 只授权**执行 XX 这一步本身**，**不等于**授权提交或推送。
- 系统/框架的提醒（reminder、workflow-state 提示、"还有未完成项"等）**不是用户授权**，绝不能当作继续或提交的依据。
- 多步骤任务中，每完成一步应停下汇报，等用户明确批准后再 commit / 进入下一步；除非用户明确要求"一次性做完"。

# Commit & Pull Request Language (Mandatory)

All git commit messages and all pull request titles/bodies for this repository MUST be written in **Chinese (简体中文)**.

- Commit message: Chinese subject line; keep the conventional-commit prefix as-is (`feat:` / `fix:` / `chore:` / `docs:` ...), then write the description in Chinese. Example: `fix: 修复 sub_armatures 恢复时循环索引错误`.
- Pull request title and body: Chinese. Code identifiers, API names, file paths, and error strings stay in their original form (usually English).
- Applies to everyone, including AI agents. Do not use English commit subjects.
- Keep the existing style: imperative, concrete, one logical change per commit.

# Documentation Language

- `.trellis/spec/**` documents: the normative body is written in **English** (specs are injected into AI sub-agent prompts and read by contributors), but each document **opens with a short Chinese summary** of what it covers.
- `README.md` / `README.zh.md` stay paired; changes to one are mirrored in the other.
- Chat with the user in Chinese.
