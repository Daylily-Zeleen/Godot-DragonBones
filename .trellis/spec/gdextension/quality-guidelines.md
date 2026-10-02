# Quality Guidelines

> 本文件记述本层的质量约定：格式化规则、Godot 版本支持策略、禁用模式、已知技术债。

## Overview

Formatting is enforced by `.clang-format` at the repo root (LLVM base, tuned for Godot). Everything else in this file is a judgment rule backed by what the code actually does today.

## Formatting

`.clang-format` defines:

| Setting | Value | Line |
|---------|-------|------|
| `BasedOnStyle` | `LLVM` | `.clang-format:6` |
| `AccessModifierOffset` | `-4` | `.clang-format:7` |
| `IndentWidth` | `4` | `.clang-format:102` |
| `UseTab` | `Always` — indentation is **tabs** | `.clang-format:175` |
| `ColumnLimit` | `0` — no wrapping | `.clang-format:62` |
| `AlignAfterOpenBracket` | `DontAlign` | `.clang-format:8` |
| `ContinuationIndentWidth` | `8` | `.clang-format:67` |

Run `clang-format -i <file>` before committing C++ changes. Do not reformat files unrelated to your change — the `ColumnLimit: 0` setting means diffs stay honest only if you limit the edit to touched lines.

Indentation is `tab`, 4-wide. There is **no root `.editorconfig`** and no root `.gitattributes` EOL rule — the only `.editorconfig` in the tree belongs to the `thirdparty/godot-cpp` submodule and declares `root = true`, so it does **not** apply to this repository's files. Editors that honour `.clang-format` (VS Code C/C++, clangd) get the indent settings right; other editors must be configured manually.

## Godot Version Support

Declared support lives in `demo/addons/godot_dragon_bones.daylily-zeleen/godot_dragon_bones.gdextension:26` (`compatibility_minimum = 4.2`), and the code must compile across that range:

- **4.2 → current**: no `TypedDictionary`; containers fall back to `godot::Dictionary` via the guard in `src/armature.h:43-52`.
- **4.3+**: `GodotCPPDocData` is available; the build degrades gracefully below it via `AttributeError` fallback (`SConstruct:89-90`).

Rules:

- New API must compile on the whole range. Never raise the floor implicitly — if a change genuinely requires a newer Godot, bump `compatibility_minimum` in the same PR and say so.
- Only three conditional macros exist in this repo: `TOOLS_ENABLED`, `DEBUG_ENABLED`, and Godot version guards. Do not introduce platform (`_WIN32`, `__EMSCRIPTEN__`, `__ANDROID__`) guards by hand — the build system already handles per-platform differences; a manual platform branch is a defect trigger for the CI matrix.

## Forbidden Patterns

### `new` / `delete`

```cpp
// Don't
auto *slot = new Slot_GD();
delete slot;
```
Everything crossing the Godot allocator boundary uses `memnew` / `memdelete` / `memnew_arr` / `memdelete_arr`, or `Ref<>`. See [memory-and-lifetime.md](./memory-and-lifetime.md).

### `assert()` and `print_line()`

Not present anywhere in `src/`. Use `ERR_FAIL_*` / `ERR_PRINT` / `WARN_PRINT` so failures reach the Godot console with correct severity. See [error-handling.md](./error-handling.md).

### `GDCLASS` on a runtime subclass

`Slot_GD` and the `TextureData` subclasses are runtime types; adding `GDCLASS` there would double-register and break the type-index contract. Use `BIND_CLASS_TYPE_A/B` (`thirdparty/dragonBones/core/DragonBones.h:94-119`).

### Unguarded forward into the runtime

Every wrapper entry point that touches an owned runtime pointer must null-guard first (`src/armature_view.cpp:659-804` is the reference). Unguarded calls produce crashes rather than errors — see the `is_playing()` gap documented in [error-handling.md](./error-handling.md).

### Hand-rolling a version or platform branch per call site

Use one alias + one guard as in `src/armature.h:43-52`, or `decltype` on the engine virtual as in `src/editor/dragon_bones_editor_plugin.h:72`.

## Testing Requirements

There is **no automated test suite** in this repository — no `test/`, no doctest/Catch2/GUT, no test step in CI. `.github/workflows/build.yml` only compiles.

Therefore verification for a change in this layer means:

1. **Compile** the affected target(s). Minimum: `scons platform=<host> target=template_debug debug_symbols=yes`.
2. **Compile Web** when the change touches source globbing, linkage, or any widely-included header — Web is the only platform using `--whole-archive` and therefore the only one where duplicate symbols surface (a real regression of exactly this kind shipped undetected to every non-Web platform; see [../build/build-systems.md](../build/build-systems.md)).
3. **Run the demo** (`demo/project.godot`) and exercise the changed path — the demo scene drives `DragonBonesArmatureView` with animation, flip, debug, and process-mode controls (`demo/dragonbones_demo/demo.gd`).
4. State in the PR which of the above you ran, and on which platform.

Do not claim a fix without running the changed path. Do not add a test framework as a side effect of a bug fix; if a permanent regression test is genuinely warranted, propose it as its own task.

## Code Review Checklist

- [ ] Bound API matches `doc_classes/<Class>.xml` (names, setters, getters, enum constants).
- [ ] No new raw pointer without a documented owner.
- [ ] No new `new`/`delete`, `assert`, `print_line`, or manual platform guard.
- [ ] Conversions go through `to_gd_str`/`to_std_str`; literals use `SNAME(...)`.
- [ ] Error macros return type-correct defaults.
- [ ] `clang-format` applied to touched files only.
- [ ] Web build re-verified if sources/linkage/headers changed.
- [ ] PR title and body written in Chinese (see `AGENTS.md`).

## Known Tech Debt

Documented reality, not endorsement. Fixing these is welcome but must be its own task — do not bundle opportunistic cleanups into an unrelated change.

| Item | Location |
|------|----------|
| Inner loop increments the outer index when restoring `sub_armatures` | `src/armature.cpp:649` |
| No-op normalization statements `if (p_name == "") { p_name = ""; }` ×3 | `src/armature_view.cpp:131-133,150-152,169-171` |
| `set_instantiate_skin_name` omits `notify_property_list_changed()` unlike its siblings | `src/armature_view.cpp:168-178` |
| `get_global_rect` applies the inverse transform | `src/armature_view.cpp:757` |
| `_to_string` hand-written instead of the macro; missing on `DragonBonesEventObject` | `src/bone.h:60`, `src/factory.h:83` |
| `get_strings` indexes without `resize`; `get_floats` copies with `sizeof(int)` | `src/event_object.cpp:68,85-87` |
| `TextureData::operator==` can deref null after a partial null check | `src/texture_atlas_data.h:134` |
| `DragonBones` listener API are intentional no-op stubs, undocumented as such | `src/armature.h:90-94`, `src/dragon_bones.h:52-57` |
| ~40 View methods hand-forward to Armature with repeated null guards | `src/armature_view.cpp:659-804` |
| Dead/commented code: UserData index API, binary save path, View fade-out binds | `src/event_object.cpp:97-160`, `src/factory.cpp:493-518`, `src/armature_view.cpp:624-630` |
| `b_reset` vs `p_recursively` in one signature | `src/armature.h:176-177` |
| `String` setter params by value instead of `const String &` | `src/armature_view.h:105-112` |
