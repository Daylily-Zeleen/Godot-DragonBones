# Bootstrap Task: Fill Project Development Guidelines

**You (the AI) are running this task. The developer does not read this file.**

This project was initialized with `trellis init` using a **fullstack web template**
(`backend/` = ORM/database/API, `frontend/` = React/hooks/TypeScript). That template
does not describe this repository — a Godot 4 C++ GDExtension plugin with no web
frontend, no database, and no HTTP API.

**Your job**: replace the template with spec files that describe this codebase as it
actually is. Every future AI session auto-loads spec files listed in per-task jsonl
manifests. A wrong spec is worse than no spec: sub-agents will write code that looks
out of place.

---

## Status

- [x] Remove non-applicable template layers (`backend/`, `frontend/`)
- [x] Create layer structure matching the real code (`gdextension/`, `editor/`, `build/`)
- [x] Fill gdextension guidelines (binding, memory, errors, conversions, structure, quality)
- [x] Fill editor plugin guidelines
- [x] Fill build & release guidelines
- [x] Rewrite `guides/` for this project
- [x] Enforce Chinese commit / PR message language in `AGENTS.md`
- [x] Add code examples backed by real `file:line` citations
- [x] No placeholder text remains

---

## Final Spec Structure

```
.trellis/spec/
├── gdextension/          # src/ (excluding src/editor/) — the Godot binding layer
│   ├── index.md          # Pre-Development Checklist + Quality Check
│   ├── directory-structure.md
│   ├── class-binding.md
│   ├── memory-and-lifetime.md
│   ├── error-handling.md
│   ├── type-conversion.md
│   └── quality-guidelines.md
├── editor/               # src/editor/ + TOOLS_ENABLED code
│   ├── index.md
│   └── editor-plugin.md
├── build/                # SConstruct, CMakeLists, CI, packaging
│   ├── index.md
│   ├── build-systems.md
│   └── ci-and-release.md
└── guides/               # cross-cutting thinking checklists
    ├── index.md
    ├── code-reuse-thinking-guide.md
    └── cross-layer-thinking-guide.md
```

### Why these layers

| Layer | Source of truth | Rationale |
|-------|-----------------|-----------|
| `gdextension/` | `src/*.cpp`, `src/*.h`, `register_types.cpp` | The bulk of the project's own code; distinct conventions from the vendored runtime |
| `editor/` | `src/editor/*`, `TOOLS_ENABLED` blocks | Gated entirely differently (excluded from `template_release`) |
| `build/` | `SConstruct`, `CMakeLists.txt`, `.github/workflows/build.yml` | Two build systems and a 22-job matrix; a separate discipline from writing C++ |

`thirdparty/` is **deliberately not covered**: `dragonBones/` and `rapidjson/` are vendored
upstream code, and `godot-cpp/` is a submodule. Specs must not tell agents to "fix"
third-party sources.

---

## What was removed and why

| Removed | Reason |
|---------|--------|
| `backend/database-guidelines.md` | No database, no ORM, no migrations. |
| `backend/error-handling.md`, `logging-guidelines.md` | Replaced by `gdextension/error-handling.md`, which documents the real `ERR_FAIL_*` rules instead of HTTP error responses. |
| `backend/directory-structure.md` | Replaced by `gdextension/directory-structure.md`. |
| `frontend/*` (all six files) | There is no frontend. No React, no TypeScript, no state management. |
| Template-style `backend/index.md`, `frontend/index.md` | Superseded by per-layer indexes with real checklists. |

## What was rewritten in place

`guides/` kept its two guides, but their content was Trellis's own internal documentation
(about `src/templates/*/commands/trellis/`, `docs-site` version routing, `packages/cli/...`).
That content is irrelevant here; both guides were rewritten around this project's real
boundaries.

---

## How the spec was filled

### Step 1: Existing convention files

| Source | Used for |
|--------|----------|
| `README.md` | Supported versions, build commands, the "don't free `DragonBonesArmature` yourself" rule |
| `.clang-format` | Formatting rules (tabs, `ColumnLimit: 0`, LLVM base) |
| `AGENTS.md` | Language rules |
| `.gitignore`, `.gitattributes` | Generated vs tracked artifacts |

### Step 2: Codebase analysis

Every rule is backed by a citation into the current tree — a class declaration, a macro
invocation, a build step, or a CI job. Analysis covered:

- The two class families (`DragonBones*` Godot wrappers vs `BIND_CLASS_TYPE_*` runtime subclasses)
- The registration levels in `src/dragon_bones_registration.cpp`
- Ownership: `memnew`/`memdelete`, `Ref<>`, the mesh pool, manual RIDs, `clean_callbacks`
- Error macro selection across `src/`
- Type conversion through `to_gd_str`/`to_std_str` and the Godot-version container guard
- The SCons/CMake source-glob rule and the Web-only `--whole-archive` duplicate-symbol trap
- The 22-job CI matrix, artifact naming, and the version file as single source of truth

### Step 3: Documented reality, not ideals

Known bugs and inconsistencies are recorded under "Known Inconsistencies" / "Known Defects" /
"Known Tech Debt" sections rather than silently normalized, so agents do not copy a latent
bug as if it were the convention. Examples: the `sub_armatures` loop-index bug
(`src/armature.cpp:649`), the shadowed `key` in the editor plugin
(`src/editor/dragon_bones_editor_plugin.cpp:275-291`), and the missing null guard in
`is_playing()` (`src/armature.cpp:319-321`).

---

## Ground rules for future updates

- Specs describe `src/`, `src/editor/`, and the build files. Not `thirdparty/`.
- Every normative rule cites a real `file:line`.
- The normative body is English; each document opens with a short Chinese summary.
- Commit messages and PR titles/bodies are Chinese — see the root `AGENTS.md`.
- Fixing an item listed under "Known Tech Debt" means moving it out of that section into the
  corresponding rule, not deleting it silently.

---

## Completion

Checklist is complete. To close this task:

```bash
python ./.trellis/scripts/task.py finish
python ./.trellis/scripts/task.py archive 00-bootstrap-guidelines
```

After archive, every new developer who joins this project will get a
`00-join-<slug>` onboarding task instead of this bootstrap task.
