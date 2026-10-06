# Build Systems

> 本文件记述两套构建系统的分工、源码收集规则与构建后处理流程。

## Overview

Two build systems coexist with different roles:

| System | Role | Evidence |
|--------|------|----------|
| **SCons** (`SConstruct`) | Authoritative. Produces shipping binaries; used by CI. | `.github/workflows/build.yml:264-271` |
| **CMake** (`CMakeLists.txt`) | Secondary, tooled for clangd (`CMAKE_EXPORT_COMPILE_COMMANDS ON`). Builds godot-cpp from the submodule and links `godot::cpp`. | `CMakeLists.txt:8,51` |

Changes to source plumbing must be made in **both**. A rule added to one and not the other produces a build that works locally and fails in CI, or vice versa.

## Godot Version Floor & godot-cpp 10.x

`thirdparty/godot-cpp` tracks its `master` branch (godot-cpp **10.x**), which is versioned independently of Godot. Two things changed from the older per-Godot branches:

1. **`api_version` is mandatory.** The `gdextension/` directory no longer contains a single `extension_api.json`; it ships `extension_api-4-3.json` … `extension_api-4-7.json`, and `gdextension_interface.json` (the interface is generated from JSON, not checked in as a header). Supported values: `4.3`–`4.7` — **4.2 or older cannot be targeted by godot-cpp 10.x**.
2. **The target version IS the compatibility floor.** The chosen API file's version numbers are written into the generated `godot_cpp/core/version.hpp`, and `GDExtensionBinding::init()` refuses to load the extension in any Godot older than that.

Both systems therefore pin `4.3`:

| System | Mechanism |
|--------|-----------|
| SCons | `env = SConscript("thirdparty/godot-cpp/SConstruct", {"api_version": API_VERSION})` — `SConstruct:81`, with `API_VERSION` defaulting to `4.3` at `SConstruct:61` |
| CMake | `set(GODOTCPP_API_VERSION "4.3" ...)` — `CMakeLists.txt:30-32`, consumed by godot-cpp's own CMake |

Verify the effective floor at any time:

```bash
grep GODOT_VERSION_MINOR thirdparty/godot-cpp/gen/include/godot_cpp/core/version.hpp   # -> 3
grep compatibility_minimum demo/addons/godot_dragon_bones.daylily-zeleen/godot_dragon_bones.gdextension
```

These two must agree, and so must `README.md` / `README.zh.md`.

### Compatibility rule (verified against godot-cpp source)

The C interface is **append-only**, so the restriction is one-directional:

- A build targeting 4.3 loads in 4.3 and every later version.
- A build targeting 4.7 refuses to load in 4.3–4.6.

Evidence: every interface function in the 4.2 header still exists on master (the dozens added since are all tagged `"since": "4.4"`–`"4.7"` in `gdextension/gdextension_interface.json`; the only removals are `deprecated` markers, and the interface functions are looked up by name at runtime via `p_get_proc_address`). Newer functions are guarded in godot-cpp by `#if GODOT_VERSION_MINOR >= N`.

> **Rule**: raising `api_version` raises the minimum Godot version users need. Treat it as a user-visible change: update `.gdextension`, both READMEs, and this pin together.

## Build Profile (`build_profile.json`)

`build_profile.json` trims godot-cpp's generated bindings to the classes this extension actually uses (25 listed classes expand to 40 generated sources, versus 920 without a profile). Both build systems apply it automatically — **no `build_profile=` argument needed**.

```bash
scons platform=windows arch=x86_64 target=template_debug     # profile applied automatically
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug                 # profile applied automatically
```

Verify it took effect:

```bash
scons -n platform=<host> arch=x86_64 target=template_debug | grep -c 'gen.src.classes'   # -> 40, not 920
```

### SCons path

`SConstruct` seeds godot-cpp's own `build_profile` option through `ARGUMENTS`, because `build_profile` is a godot-cpp option, not this project's:

```python
from SCons.Script import ARGUMENTS

_build_profile = Dir("#").File("build_profile.json").abspath
if os.path.isfile(_build_profile):
    ARGUMENTS.setdefault("build_profile", _build_profile)
```
— `SConstruct:63-72`

`setdefault` gives the right precedence: an explicit `build_profile=<path>` on the command line still wins, while the in-repo profile applies by default.

> **Gotcha**: a project-root `custom.py` is **not** read. godot-cpp's SConstruct resolves its default `custom.py` relative to *its own* SConscript directory (`thirdparty/godot-cpp/`), so `thirdparty/godot-cpp/custom.py` works but the repo-root one is silently ignored. This was verified: a root `custom.py` setting `dev_build = True` produced no `.dev.` in the output suffix, while the same file inside `thirdparty/godot-cpp/` did. To inject project-level option defaults, export a `customs` list (godot-cpp does `customs += Import("customs")`); entries must be **absolute** Python file paths, since they resolve against godot-cpp's directory.

### CMake path

godot-cpp 10.x supports the profile natively through `GODOTCPP_BUILD_PROFILE` (`thirdparty/godot-cpp/cmake/godotcpp.cmake:167`, applied at `:292` via `build_profile_generate_trimmed_api`). This project only sets the cache variable before `add_subdirectory`:

```cmake
if(NOT DEFINED GODOTCPP_BUILD_PROFILE)
	set(GODOTCPP_BUILD_PROFILE ${CMAKE_CURRENT_SOURCE_DIR}/build_profile.json CACHE FILEPATH "...")
endif()
```
— `CMakeLists.txt:34-36`

A successful configure logs `-- Using build profile to trim api file`. Set `-DGODOTCPP_BUILD_PROFILE=` (empty) to build the full binding set.

> **History**: an earlier revision of this project hand-rolled the bridge (call `generate_trimmed_api` via `execute_process`, then pass the result through `GODOT_CUSTOM_API_FILE`) because the pinned godot-cpp 4.3 branch had no CMake profile support. That code is gone — do not reintroduce it if the native option is available.

### Windows CMake toolchain

godot-cpp's CMake expects an **MSVC-frontend** compiler on Windows. Use `clang-cl`, not `clang++`:

```bash
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
```

`cmake/godotcpp.cmake` sets `IS_MSVC=1` only when `CMAKE_CXX_COMPILER_FRONTEND_VARIANT` is `MSVC`; plain `clang++` is treated as GCC-like, so godot-cpp adds `-lstdc++`, and the MSVC linker then fails with `could not open 'stdc++.lib'`. A working configure prints `-- Using clang-cl`.

`CMakeLists.txt:39-41` also disables `GODOTCPP_USE_STATIC_CPP` on Windows (`-static -lstdc++` is not meaningful there).

### Both systems must agree

The profile is the single source of truth for which bindings exist. Changing `build_profile.json` changes both the header set under `gen/include/godot_cpp/classes/` and the compiled sources — so a class added to `src/` requires the profile to list it (or one of its ancestors, since profiles include parents and a fixed always-included set).

## Upgrading godot-cpp: what breaks

Both upgrades from the old per-version branch to 10.x have now been done; these are the four breakages encountered, kept here so the next bump is mechanical.

| Symptom | Cause | Fix |
|---------|-------|-----|
| `error C2062: unexpected type` in vendored `thirdparty/dragonBones/animation/TimelineState.h` (via `DRAGONBONES_NEW_ARR`) | godot-cpp 10.x made its allocation macros self-qualifying: `memnew_arr` now expands to `::godot::memnew_arr_template<...>`, so this project's `godot::memnew_arr(...)` produced `godot::::godot::...` | Use the macro unprefixed — `src/godot_dragon_bones.h:52` |
| `error C2079: uses undefined class godot::Ref<...>` in `src/event_object.h` / `src/bone.h` | `binder_common.hpp` now only forward-declares `class Ref;` (`:135`); previously `ref.hpp` arrived transitively | Include `<godot_cpp/classes/ref.hpp>` explicitly wherever `Ref<>` is used as a member |
| `'godot_cpp/templates/vmap.hpp' file not found` | `VMap` was removed upstream (no replacement) | `DrawDataStore` is now `std::map<int, LocalVector<DrawData>>` (`src/mesh_display.h:57-63`) — `std::map` iterates in ascending key order, matching what `_draw()` relied on |
| CMake: `could not open 'stdc++.lib'` | plain `clang++` on Windows is not detected as an MSVC frontend, so godot-cpp adds `-lstdc++` | Configure with `-DCMAKE_CXX_COMPILER=clang-cl` |

Also note `godot-cpp`'s generated `src/templates/*.cpp` is now compiled (`hashfuncs.cpp` etc.), so `gen/` output is not header-only.

### Verifying an upgrade

```bash
# 1. API floor still hits the intended Godot version
grep GODOT_VERSION_MINOR thirdparty/godot-cpp/gen/include/godot_cpp/core/version.hpp

# 2. Every include this project uses still exists upstream
grep -rho "godot_cpp/[a-z_/]*\.hpp" src register_types.cpp | sort -u

# 3. Both build systems produce an artifact
scons platform=<host> arch=x86_64 target=template_debug
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_C_COMPILER=clang-cl && cmake --build build
```

## SCons

Imports godot-cpp's own build and inherits all its options:

```python
env = SConscript("thirdparty/godot-cpp/SConstruct")
lib_name = "libgddragonbones"
```
— `SConstruct:69-70`

Source collection:

```python
env.Append(CPPPATH=["src/", "thirdparty/"])
sources = Glob("src/*.cpp") + Glob("register_types.cpp")
...
add_sources_recursively("src/", sources, ["editor"])
# godot-cpp is built and linked separately by its own SConstruct (env.GodotCPP()),
# so it must not be globbed in here again (duplicate symbols at link time).
add_sources_recursively("thirdparty/", sources, ["godot-cpp"])
```
— `SConstruct:81-82,103-106`

### Rule: never glob `thirdparty/godot-cpp`

The recursive helper `add_sources_recursively` (`SConstruct:93-100`) walks `thirdparty/` and adds every `*.cpp`. `godot-cpp` must stay in its exclusion list. godot-cpp's sources are compiled by its own `env.GodotCPP()` builder and linked as `thirdparty/godot-cpp/bin/libgodot-cpp*.a`; globbing them again compiles the same translation units twice.

> **Why this matters (real regression)**: when godot-cpp was moved into `thirdparty/` without updating the exclusion, 970 duplicate translation units were compiled. Every platform except **Web** still linked, because the linker only pulls archive members needed for undefined symbols. Web uses `--whole-archive`, so it failed with `wasm-ld: error: duplicate symbol: godot::GDExtensionBinding::initdata` — defined in both `thirdparty/godot-cpp/src/godot.o` and `.../libgodot-cpp.*.a`. The failure appeared only on 2 of 22 CI jobs.

Verify after any change to the glob:

```bash
scons -n platform=<host> arch=x86_64 target=template_debug   # inspect the printed source list
```

The list must contain `src/**`, `register_types.cpp`, and `thirdparty/dragonBones/**` — and **no** `thirdparty/godot-cpp/**`.

### Editor Sources

```python
if env.debug_features:
    env.Append(CPPDEFINES=["TOOLS_ENABLED"])
    sources += Glob("src/editor/*.cpp")
```
— `SConstruct:122-124`

`src/editor/*.cpp` is compiled only in debug-feature builds. See [../editor/index.md](../editor/index.md).

### Documentation Target

`doc_classes/*.xml` is compiled into generated C++ only for editor/template_debug builds, with a graceful fallback below godot-cpp 4.3:

```python
return env.GodotCPPDocData(generated_doc_data_file, source=env.Glob("doc_classes/*.xml"))
...
except AttributeError:
    print("Not including class reference as we're targeting a pre-4.3 baseline.")
```
— `SConstruct:114,117-118`

`gen/` is gitignored — never hand-edit the generated file.

### Output Naming

Output goes to `./bin` (`SConstruct:85`), matching godot-cpp's `.{platform}.{target}.{arch}` suffix scheme:

| Platform | Artifact |
|----------|----------|
| macOS | `{lib_name}.{platform}.{target}.framework/...` (`SConstruct:136-140`) |
| iOS | `{lib_name}.{platform}.{target}[.simulator].a` (`SConstruct:141-151`) |
| others | `{lib_name}{suffix}{SHLIBSUFFIX}` (`SConstruct:153-156`) |

The suffix (`env["suffix"]`, including `.dev` for dev builds) comes from godot-cpp, not from this repo.

### Post-Build Pipeline

A `complete` pseudo-target runs `on_complete` after linking (`SConstruct:250-252`):

1. Copy the library into `demo/addons/godot_dragon_bones.daylily-zeleen/bin/`, rewriting `.dev.` out of the name (`SConstruct:180-206`).
2. Copy `README.md`, `README.zh.md`, `LICENSE` into the addon (`SConstruct:207-210`).
3. Rewrite README image paths relative to the addon (`SConstruct:212-226`).
4. Rewrite the `.gdextension` `version = "..."` line from the `version` file (`SConstruct:228-246`).

Because of step 4, **the `.gdextension` version field is generated** — editing it by hand is overwritten on the next build.

> **Gotcha**: `NoCache(sources)` disables SCons caching for extension sources (`SConstruct:248`). Combined with the currently-commented-out `actions/cache` step in CI, `SCONS_CACHE` / `SCONS_CACHE_LIMIT` are exported but effectively unused. Do not assume CI builds are incremental.

## CMake

Builds godot-cpp from the submodule via `add_subdirectory` and links its `godot::cpp` target. It does **not** consume a prebuilt static library.

```cmake
add_subdirectory("${CPP_BINDINGS_PATH}" godot-cpp)
...
target_link_libraries(${PROJECT_NAME}
	PRIVATE
	godot::cpp
)
```
— `CMakeLists.txt:51,257-260`

> **Why**: an earlier revision linked a prebuilt `libgodot-cpp.${SYSTEM_NAME}.${BUILD_TYPE}.${BITS}` from `${CPP_BINDINGS_BUILD_PATH}` (default `./thirdparty/godot-cpp/build`) — a directory the SCons workflow never creates. That link step could not resolve, so a plain CMake configure+build failed. Building from source also guarantees the generated bindings match this build's Godot version and build profile. `godot::cpp` carries its own include dirs as `PUBLIC`, which is why `CMakeLists.txt` no longer lists `${CPP_BINDINGS_PATH}/include` or the gen include path itself.

Source globbing excludes godot-cpp for the same duplicate-symbol reason as SCons:

```cmake
# NOTE: godot-cpp is a submodule that is built and linked separately (see target_link_libraries
# below), so it must be excluded here, otherwise its sources are compiled twice and produce
# duplicate symbols at link time.
file(GLOB_RECURSE SOURCES src/*.c** thirdparty/dragonBones/*.c**)

file(GLOB_RECURSE HEADERS src/*.h** thirdparty/dragonBones/*.h** thirdparty/rapidjson/*.h**)
```
— `CMakeLists.txt:207-209`

`rapidjson` is header-only, so it appears in the headers glob only.

Key properties: C++17 (`CMakeLists.txt:133-135`), exceptions disabled by default (`:184-189`), MSVC `/WX /MD[d] /utf-8` with warnings suppressed (`:142,144-155,200`), output name mirroring godot-cpp (`:254-257`), `BUILD_SHARED` toggling SHARED/STATIC (`:22-24,185-189`).

### Rule: `if(NOT DEFINED X)`, never `if(X STREQUAL "")`

This is the CMake equivalent of an unset-variable check, and getting it wrong silently produces empty paths.

```cmake
# WRONG: when CPP_BINDINGS_PATH is undefined, `if(<var> STREQUAL "")` compares the
# *literal name* against "", which is false - so the default below would never run.
if(CPP_BINDINGS_PATH STREQUAL "")
	set(CPP_BINDINGS_PATH ./thirdparty/godot-cpp CACHE STRING "...")
endif()

# RIGHT
if(NOT DEFINED CPP_BINDINGS_PATH)
	set(CPP_BINDINGS_PATH ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/godot-cpp CACHE STRING "...")
endif()
```
— see `CMakeLists.txt:10-24` for the current form

Proven behavior (`cmake -P`):

```
-- branch NOT taken (undefined vs empty compares equal)   # if(MY_UNDEFINED_VAR STREQUAL "")
-- quoted form -> branch TAKEN                            # if("${REAL}" STREQUAL "")
```

A literal path also guards against `CMAKE_CURRENT_SOURCE_DIR` surprises from `add_subdirectory` and is required for a clean out-of-tree build. `CPP_BINDINGS_BUILD_PATH` no longer exists — godot-cpp's build tree is owned by CMake (`${CMAKE_BINARY_DIR}/godot-cpp`).

### Known Defects

| Defect | Location |
|--------|----------|
| WASI arch guard matches `"wams32"` (typo) — never true | `CMakeLists.txt:112` |
| `elsE()` wrong case | `CMakeLists.txt:147` |
| STATIC branch omits `register_types.cpp` while SHARED includes it | `CMakeLists.txt:213` vs `:215` |
| `TOOLS_ENABLED` is defined **only** in the Debug branch, so the `editor` target compiles `src/editor/*.cpp` **without** the macro and never registers the plugin | `CMakeLists.txt:118-125` |
| Error strings misspelled `"Unsupport architechture"` | `CMakeLists.txt:84,99,107,113` |

CMake is not used by CI, so these do not fail the matrix — but do not copy them.

> **Gotcha**: SCons and CMake gate the editor layer differently, and only SCons is correct.
>
> - SCons: `TOOLS_ENABLED` is injected **and** `src/editor/*.cpp` is added together under `env.debug_features` (`SConstruct:120-122`). The two are inseparable.
> - CMake: `src/editor/dragon_bones_editor_plugin.cpp` is picked up by the `GLOB_RECURSE` regardless of target, but `add_definitions(-DTOOLS_ENABLED=1)` sits inside `if(CMAKE_BUILD_TYPE MATCHES Debug)` (`CMakeLists.txt:118-120`). The `else()` branch that sets `TARGET editor` (`:123-124`) gets no such define.
>
> `src/editor/dragon_bones_editor_plugin.h` has no `TOOLS_ENABLED` guard (only `#pragma once`, `:31`), so the editor classes compile either way — but `src/dragon_bones_registration.cpp:54-63` registers them only `#ifdef TOOLS_ENABLED`. Result: a CMake `editor` build produces a plugin whose editor features are silently never registered.
>
> If you rely on the CMake build for editor work, define `TOOLS_ENABLED` for the `editor` target too. Do not "fix" this by adding `#ifdef TOOLS_ENABLED` guards to the header — the SCons path depends on those translation units being compiled whenever the define is present.

## Line Endings & Ignore Rules

`.gitignore` covers build output, but note that it lists `*.obj` / `*.os` — the MSVC and some POSIX object extensions. GCC/Clang/emcc emit `*.o`, which is also ignored; if you add a new toolchain, verify its object extension is covered before it lands in a commit.

| File | Scope | Content |
|------|-------|---------|
| `.gitignore` | repo root | build output, generated files, `*.o`/`*.obj`/`*.os`, `.sconsign.dblite`, `compile_commands.json`, `gen/`, `bin/` |
| `demo/.gitignore` | demo project | Godot 4 `.godot/` |
| `.gitattributes` | repo root | **only** the Trellis `merge=union` rule for journals |
| `demo/.gitattributes` | demo project | `* text=auto eol=lf` |

> **Gotcha**: EOL normalization exists **only** under `demo/`. Verified with `git check-attr`:
>
> ```
> src/armature.cpp: text: unspecified
> src/armature.h:   text: unspecified
> SConstruct:       text: unspecified
> CMakeLists.txt:   text: unspecified
> ```
>
> The demo rule `* text=auto eol=lf` does **not** inherit upward. On a Windows checkout with no `core.autocrlf`, C++ sources and build files can therefore be committed with CRLF, producing whole-file diffs and noisy reviews (the generated `.gdextension` already triggers a "CRLF will be replaced by LF" warning). If you add a root-level `* text=auto eol=lf`, land it as its own commit — it will renormalize the tree.

## C++ Unit Tests (`tests`)

doctest-based C++ unit tests live in `tests/` and are **off by default**; they must be enabled in **both** build systems with the same switch name and the same macro.

| System | Enable | Mechanism |
|--------|--------|-----------|
| SCons | `tests=yes` | `env.Append(CPPDEFINES=["GDDB_TESTS_ENABLED"])` + `Glob("tests/*.cpp")` + `CPPPATH=["tests/"]` — `SConstruct:129-132` |
| CMake | `-DGODOT_DRAGONBONES_TESTS=ON` | `target_compile_definitions(... GDDB_TESTS_ENABLED)` + `target_sources(... tests/*.cpp)` + `target_include_directories(... tests)` — `CMakeLists.txt:224-228` |

Build and run:

```bash
# SCons (default build compiles no tests); the build already copies the library
# into demo/addons/godot_dragon_bones.daylily-zeleen/bin/ via the `complete` target.
scons -Q --silent platform=windows target=template_debug arch=x86_64 -j1 tests=yes
"D:/Dev/godot/godot/bin/Godot_v4.3-stable_win64_console.exe" --path demo --gddb-run-tests
```

- Exit code is 0 on success, non-zero on failure — usable in CI.
- `tests/` is a separate directory not covered by the `src/`/`thirdparty/` globs, so the default build is untouched. Verify with `scons -n ... | grep -c "tests/"` (expect 0).
- The `--gddb-run-tests` entry is invoked from `src/dragon_bones_registration.cpp` at `MODULE_INITIALIZATION_LEVEL_SCENE`, guarded by `#ifdef GDDB_TESTS_ENABLED` (mirrors GodotJS-Ext's `--jsb-run-tests`).
- `thirdparty/doctest/` is header-only; the `thirdparty/` glob never picks up a `.cpp` there, but it is on `CPPPATH` so `#include <doctest/doctest.h>` resolves.

> **Gotcha**: `OS::execute` merges stdout (and stderr, with `p_read_stderr=true`) into a **single** `String` element of the output `Array`, not one entry per line — `core/core_bind.cpp:411-424`. Tests that spawn a child and inspect output must read `output[0]`, not iterate lines.

> **Gotcha**: a trap's exit code is **negative** on both platforms — `0x80000003` (`STATUS_BREAKPOINT`) on Windows, a signal on POSIX. Do **not** treat `rc < 0` as "failed to spawn"; only `OS::execute`'s own `-1` means the launch failed. See `tests/initialized_buffer_test.h`'s `check_death_case_traps`.

> **Gotcha**: **never use a Godot container (or any heap-allocating type) in a namespace-scope static initializer** inside this extension. Static initialization runs during `DllMain`/library load, *before* Godot's memory system is up; allocating there throws and the whole library fails to load — on Windows this surfaces as `Error 1114: DLL 初始化例程失败` (the DLL is valid; only loading fails). `tests/test_death.h` therefore registers death cases through an **intrusive POD linked list** (`DeathCase { const char *name; DeathBody body; DeathCase *next; }`) whose nodes live in each TU's static storage and whose head is a zero-initialized pointer — no constructor, no allocation.

> **Gotcha**: `--gddb-run-tests` finishes with `std::exit(exit_code)` rather than `SceneTree::quit()`, because `SceneTree` is not in `build_profile.json` (adding it pulls in `Node` and a dependency chain). `std::exit` skips engine shutdown, so the run ends with benign at-exit noise (`Pages in use exist at exit in PagedAllocator: ...`, `BUG: Unreferenced static string to 0: ...`) — these are expected and not test failures.

### Running tests in CI

CI (`.github/workflows/build.yml` job `tests`) runs the suite on a **fresh checkout**, where `demo/.godot/` does not exist (it is gitignored). The order matters:

1. `scons target=template_debug platform=linux arch=x86_64 tests=yes` — the shipped DLL must be the `tests=yes` build, or `try_run()` is not compiled in.
2. Run the engine once with **`--import`** — on a fresh project this *only generates* `.godot/extension_list.cfg`; the extension is loaded at the **next** startup, not this one.
3. Run `--headless --path demo --gddb-run-tests` — the extension now loads, `try_run()` fires, and the process exits with the test result.

> **Gotcha**: step 2 exits **non-zero** and that is expected: `demo.gd` references extension classes, but the editor scan/import phase runs *before* the extension is loaded, so script compilation fails. The step only needs to produce `extension_list.cfg`; assert on the file, not the exit code.

> **Gotcha**: if `--gddb-run-tests` does not trigger the entry point (extension not loaded, or the DLL was built without `tests=yes`), the process does **not** exit — it falls through to running the demo's main scene, which is a game loop that never quits. Locally this looks like a hang; in CI it would burn up to the 6h job ceiling. The CI step is wrapped in `timeout-minutes: 10`, and asserts on `[doctest] Status: SUCCESS!` so a silently-not-run entry cannot pass as green.

> **Gotcha**: the shipped DLL under `demo/addons/.../bin/` is overwritten by *every* build. After a plain `scons ... ` (no `tests=yes`), that DLL no longer contains the test entry point, and `--gddb-run-tests` will appear to hang. Rebuild with `tests=yes` before running tests.

### Death tests

`CRASH_BAD_INDEX` / `ERR_FAIL` are unrecoverable in-process, so cases that must trap run in a **child process**. `tests/test_death.h` provides this generically:

```cpp
// in any test TU, at namespace scope:
GDDB_DEATH_CASE("my-case") { /* code that must trap */ }
```

The generic entry point (`tests/test_runner.h`'s `try_run`) knows **nothing** about individual cases: it dispatches `--gddb-death=<name>` by looking the name up in the registry. Adding a case therefore touches only the test TU that declares it — never `try_run`. (`try_run` is `register_types`' only hook and stays a fixed two-branch shell: dispatch a death case, or run doctest.)

> **Gotcha (why the parent must judge on output markers, not exit codes)**: on POSIX, `OS::execute` with an output array takes the **`popen`** path, and that path does `*r_exitcode = WEXITSTATUS(rv)` **without** the `WIFEXITED` guard used by the fork path (`drivers/unix/os_unix.cpp:878` vs `:909`). For a signal-terminated child the two disagree, so the exit code alone cannot distinguish "trapped correctly" from "returned normally". The parent therefore asserts on the child's **stdout markers** — `[gddb] death case: <name>` present, `[gddb] death case did NOT trap: <name>` absent.

> **Gotcha (must `fflush` before trapping)**: a trap kills the process immediately, so any **unflushed** stdio buffer is discarded. Without an explicit flush, the child's "entering" marker never reaches the parent's pipe and the death test fails with a missing-marker error that has nothing to do with the code under test. Reproduced on Linux: unflushed stdout yielded only `Illegal instruction`, flushed stdout yielded the marker. `try_run_death_case` calls `godot::_err_flush_stdout()` (`fflush(stdout)`) after each marker print.

> **Gotcha (static-init safety)**: the registry is an intrusive POD linked list, not a Godot container — a namespace-scope constructor allocating via Godot's allocator runs during `DllMain` and makes the whole library fail to load (`Error 1114`). See the earlier gotcha above.

## Checklist

- [ ] Source-glob change applied to **both** `SConstruct` and `CMakeLists.txt`.
- [ ] godot-cpp not globbed in either.
- [ ] `scons -n` source list inspected.
- [ ] Both `template_debug` and `template_release` build.
- [ ] Web rebuilt if linkage/headers/globs changed.
- [ ] No hand-edit to generated artifacts (`gen/`, `.gdextension` version).
- [ ] Test-related change verified in both systems (SCons `tests=yes/no`, CMake `GODOT_DRAGONBONES_TESTS=ON/OFF`).
