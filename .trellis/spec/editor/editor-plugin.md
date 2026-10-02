# Editor Plugin

> 本文件记述编辑器插件层的三类角色、门控方式与导入/导出契约。

## Overview

Three editor classes, registered as internal at the EDITOR init level under `TOOLS_ENABLED` (`src/dragon_bones_registration.cpp:54-62`):

| Class | Base | Role |
|-------|------|------|
| `DragonBonesExportPlugin` | `EditorExportPlugin` | Injects the `.dbfactory` sidecar into exported builds |
| `DragonBonesImportPlugin` | `EditorImportPlugin` | Imports `*_ske.json` / `*_ske.dbbin` into a `DragonBonesFactory` resource |
| `DragonBonesEditorPlugin` | `EditorPlugin` | Wires the two above into the editor, watches the filesystem, keeps factories in sync on move/reimport |

Declarations: `src/editor/dragon_bones_editor_plugin.h:42-102`. They are added with `EditorPlugins::add_by_type<DragonBonesEditorPlugin>()` (`src/dragon_bones_registration.cpp:61`) and removed symmetrically (`:91`).

## TOOLS_ENABLED Gating

Every editor-only declaration **and** its definition is wrapped, including the `#endif` comment:

```cpp
#ifdef TOOLS_ENABLED
#include <editor/dragon_bones_editor_plugin.h>
#endif //TOOLS_ENABLED
```
— `src/dragon_bones_registration.cpp:35-37`

The build only compiles `src/editor/*.cpp` when debug features are enabled:

```python
if env.debug_features:
    env.Append(CPPDEFINES=["TOOLS_ENABLED"])
    sources += Glob("src/editor/*.cpp")
```
— `SConstruct:94-96`

Consequences to respect:

- Editor code is **excluded from `template_release`**. Never reference an editor symbol from a non-guarded path.
- `TOOLS_ENABLED` grants access to editor-only engine APIs (`EditorPlugin`, `EditorImportPlugin`, `EditorFileSystemDirectory`), which do not exist in a release build of the engine.
- Editor-only members on runtime classes follow the same rule: `DragonBonesArmatureProxy` (`src/armature.h:235-255`), the factory's editor ctor/dtor and static registry (`src/factory.h:121-134`), `_validate_property` (`src/armature_view.h:83-85`).

## Import Pipeline Contract

`DragonBonesImportPlugin::try_import` is the single entry point for turning source files into a factory. Its contract (`src/editor/dragon_bones_editor_plugin.cpp:173-188`):

1. Reuse the caller-provided factory if given, otherwise `ret.instantiate()`.
2. Mark it imported: `ret->imported = true;` — this flag makes the factory's file lists read-only in the inspector.
3. Load the **texture atlas first** (`load_texture_atlas_json_file_list`), then the **skeleton** (`load_dragon_bones_ske_file_list`).
4. Return `{}` on any `Error != OK`.

```cpp
Error err = ret->load_texture_atlas_json_file_list(Array::make(tex_atlas_file));
ERR_FAIL_COND_V(err != OK, {});

err = ret->load_dragon_bones_ske_file_list(Array::make(ske_file));
ERR_FAIL_COND_V(err != OK, {});
```
— `src/editor/dragon_bones_editor_plugin.cpp:181-185`

> **Rule**: callers of `try_import` must check the returned `Ref` before dereferencing it. It returns an empty `Ref` on failure.

The loader/saver pair registered at SCENE level (`ResourceFormatLoaderDragonBones` / `ResourceFormatSaverDragonBones`, `src/dragon_bones_registration.cpp:75-76`) is the runtime half of this contract — the `.dbfactory` file format is defined by `DragonBonesFactoryFileProcessor` (`src/factory.h:138-148`).

## Resource Path Rules

The generated resource name is derived from the source path by hash, so it is stable across machines:

```cpp
static String get_imported_file_name(const String &p_path) { return p_path.md5_text() + ".dbimport"; }
```
— `src/factory.h:58`

Imported intermediates go under the project's data directory, honouring `use_hidden_project_data_directory`:

```cpp
return vformat("res://%s/imported/%s", use_hidden_directory ? ".godot" : "godot", get_imported_file_name(p_path));
```
— `src/factory.h:59-62`

Rules:

- Do not hand-construct `.godot/imported/...` paths anywhere else; call `convert_to_imported_path`.
- Runtime lookup falls back to the imported path only outside the editor: the editor keeps the original files available (`src/factory.cpp:64-69`).
- `SRC_JSON_EXT` / `SRC_BIN_EXT` / `SAVED_EXT` (`src/factory.h:76-79`) are the single source of truth for the extensions; do not re-type the literals.

## Filesystem Watching

`DragonBonesEditorPlugin` keeps the factory registry in sync when files move:

- `_on_file_system_dock_files_moved` collects moved paths into `moved_factory_files`.
- `_reimport_moved_factory_files` re-imports and re-saves each moved factory.
- `_on_filesystem_changed` rescans directories (`EditorFileSystemDirectory`) and reimports factories.
- `clear_reimporting_flag` clears `DragonBonesFactory::editor_reimporting`, which `get_file_data` checks to suppress expected "file not found" errors during reimport (`src/factory.cpp:78-83`).

The static registry is a function-local static: `DragonBonesFactory::get_all_imported_factories()` (`src/factory.h:126-129`); entries are removed in `~DragonBonesFactory` (`src/factory.cpp:446-453`).

> **Gotcha**: the auto-generation setting is read by name — `Godot_DragonBones/auto_generate_dbfactory` (`src/editor/dragon_bones_editor_plugin.cpp:191`), defaulted to `true` in the demo project (`demo/project.godot:11-13`). Changing this string is a project-settings migration, not a rename.

## Known Defects

Documented so they are not copied as patterns. Each is a real bug in the current editor layer:

| Defect | Location |
|--------|----------|
| Inner-scope `String key = new_path;` shadows the outer `key`, so `erase(key)` uses `old_path` and a factory found under `new_path` stays registered under the wrong key | `src/editor/dragon_bones_editor_plugin.cpp:275-291` |
| `factory->take_over_path(new_path)` dereferences the result of `try_import` without checking it; `try_import` returns an empty `Ref` on failure | `src/editor/dragon_bones_editor_plugin.cpp:289-290` |
| The `memnew(DragonBonesFactory)` fallback leaks if the subsequent import fails | `src/editor/dragon_bones_editor_plugin.cpp:286` |
| Unused local `ext_low` | `src/editor/dragon_bones_editor_plugin.cpp:157` |

## Checklist

- [ ] New editor code wrapped in `TOOLS_ENABLED` in both header and source.
- [ ] Registration and de-registration kept symmetric.
- [ ] `try_import` result checked before use.
- [ ] Paths built through `convert_to_imported_path`, not by string concatenation.
- [ ] Extension literals come from the `DragonBonesFactory` constants.
- [ ] `template_release` still builds.
