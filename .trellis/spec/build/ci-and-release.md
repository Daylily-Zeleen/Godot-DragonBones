# CI & Release

> 本文件记述 CI 平台矩阵、工具链、产物与版本发布约定。

## Overview

One workflow: `.github/workflows/build.yml`. It triggers on `push`, `pull_request`, and `merge_group` (`:32`) with `concurrency` keyed on workflow+ref and `cancel-in-progress` (`:34-36`). A 22-entry matrix fans out builds; a second job merges artifacts into a single release bundle.

## Platform Matrix

`strategy.fail-fast: false` (`:42-43`) — one platform failing does not cancel the rest. That is why a Web-only regression can sit alongside 20 green jobs.

| Platform | Arch | Runner |
|----------|------|--------|
| Linux | x86_64, x86_32 | ubuntu-latest |
| macOS | universal | macos-latest |
| iOS | arm64 | macos-latest |
| Windows | x86_64, x86_32 | windows-latest |
| Android | arm64, x86_64, arm32, x86_32 | ubuntu-latest |
| Web | wasm32 | ubuntu-latest |

Each with both `template_debug` and `template_release`, except where the matrix differs — 22 jobs total (`:44-199`).

## Toolchain

| Dependency | Pinned value |
|-----------|--------------|
| Python | `3.x` |
| SCons | `4.4` (`pip install scons==4.4`) |
| Emscripten | `EM_VERSION: 3.1.39` |
| Android NDK | `r23c` |
| mingw64 | Windows only |

Pinnings are exact on purpose: SCons and Emscripten version drift changes output. Do not float them without re-verifying the full matrix.

`submodules: recursive` on checkout (`:222`) — a change to `.gitmodules` affects every job.

## Artifacts

Per-matrix artifact: `${repo}-${sha}-${platform}-${target}-${arch}`, path `./artifact/`, retention 14 days (`:300-306`). Staged by `misc/copy_dir.py` (`:296`), which recursively copies `demo/addons` → `artifact/addons`.

The merge job (`needs: build`) checks out **without** submodules, reads `BUILD_VERSION` from the `version` file, and merges with `actions/upload-artifact/merge@v7`, `delete-merged: true`, retention 90 days (`:308-325`).

## Versioning

Single source of truth: the `version` file (currently one line, `v2.0.3-dev`).

- The build rewrites the `.gdextension` `version = "..."` line from this file (`SConstruct:200-218`). **Never edit that field by hand** — it is generated.
- CI derives the merged artifact name from the same file (`build.yml:317,322`).
- Runtime compatibility is separate: `compatibility_minimum = 4.2` in `demo/addons/godot_dragon_bones.daylily-zeleen/godot_dragon_bones.gdextension:26`. Bumping the marketing version does not change the supported Godot range.

## iOS Packaging

iOS is the only platform needing two compiles plus a packaging step:

```bash
scons arch=universal ios_simulator=yes platform=ios target=<target> debug_symbols=<yes|no>
scons arch=arm64 ios_simulator=no platform=ios target=<target> debug_symbols=<yes|no>
./generate_xcframework.sh <target> debug_symbols=<yes|no>
```

`generate_xcframework.sh` builds two xcframeworks: the extension itself, and the godot-cpp dependency (`generate_xcframework.sh:6-13`). The godot-cpp paths there must match wherever the submodule lives — they were updated from `./godot-cpp/...` to `./thirdparty/godot-cpp/...` when the submodule moved. The `.gdextension` `[dependencies]` block on iOS points at the godot-cpp xcframework, so the two must agree.

## Known CI Debt

| Item | Location |
|------|----------|
| `actions/cache` step is commented out, but `SCONS_CACHE` / `SCONS_CACHE_LIMIT` are still exported by every build step | `.github/workflows/build.yml:253-262` vs `:202,268-269,277-278,287-288` |
| `NoCache(sources)` in SCons means extension sources never hit the cache anyway | `SConstruct:220` |
| Library name hard-coded as `libgddragonbones`; the demo addon directory name is repeated as a literal in several places | `SConstruct:42,58-59`, `generate_xcframework.sh:8,13` |

## Checklist

- [ ] Toolchain pins unchanged, or the full matrix re-verified.
- [ ] New platform entries added to *both* the build step's conditionals and the artifact naming.
- [ ] Artifact names remain unique per matrix entry (else the merge step collides).
- [ ] `version` bumped in the file, not in the `.gdextension`.
- [ ] iOS godot-cpp paths in `generate_xcframework.sh` still resolve.
- [ ] Web-specific flags (`--whole-archive`) accounted for when changing linkage — it is the only platform that surfaces duplicate symbols.
