# Build & Release Guidelines

> 本文件记述构建与发布约定：SCons/CMake 双构建、CI 平台矩阵、产物与版本号同步。

**Applies to**: `SConstruct`, `CMakeLists.txt`, `.github/workflows/build.yml`, `generate_xcframework.sh`, `misc/copy_dir.py`, `version`, `demo/addons/godot_dragon_bones.daylily-zeleen/godot_dragon_bones.gdextension`.

---

## Pre-Development Checklist

- [ ] Read [build-systems.md](./build-systems.md) before touching `SConstruct`, `CMakeLists.txt`, or the sources glob.
- [ ] Read [ci-and-release.md](./ci-and-release.md) before changing CI, versioning, or artifact naming.
- [ ] Identified whether the change affects **all** targets or one platform — the matrix has 22 entries.
- [ ] For anything touching source globbing or linkage: plan a Web re-verification (see the duplicate-symbol rule below).

## Quality Check

- [ ] `scons platform=<host> target=template_debug debug_symbols=yes` succeeds.
- [ ] `target=template_release` succeeds (this is the build where `TOOLS_ENABLED` is off).
- [ ] `scons -n` source list contains **no** `thirdparty/godot-cpp/` entries (see the rule in [build-systems.md](./build-systems.md)).
- [ ] Web build verified when sources, linkage, or widely-included headers changed.
- [ ] `version` and the `.gdextension` version line stay in sync (the build rewrites the latter from the former; never edit the `.gdextension` version by hand).
- [ ] CMake, if touched, still configures: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`.

---

## Topic Files

| Guide | Description |
|-------|-------------|
| [Build Systems](./build-systems.md) | SCons vs CMake, source globbing rule, post-build pipeline, output naming |
| [CI & Release](./ci-and-release.md) | Platform matrix, toolchains, artifacts, versioning, xcframework |
