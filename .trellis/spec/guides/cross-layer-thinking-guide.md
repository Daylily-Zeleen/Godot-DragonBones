# Cross-Layer Thinking Guide

> **用途**：在跨越 Godot ↔ DragonBones 运行时边界、或跨越平台边界前，先理清数据与所有权流向。

---

## The Problem

**Most bugs happen at layer boundaries**, not within layers. This repository has four boundaries that repeatedly cause bugs:

| Boundary | Failure mode | Real evidence |
|----------|--------------|---------------|
| Godot wrapper ↔ DragonBones runtime | Ownership assumed wrong → use-after-free or leak | `src/slot.h:87` comment; `src/armature.cpp:518-522` |
| C++ binding ↔ script/doc surface | Method renamed in one place → silent API break | `_bind_methods` vs `doc_classes/` |
| Build systems ↔ platforms | Rule applied to one platform/system only | duplicate symbols on Web only |
| Editor ↔ runtime | Editor-only symbol referenced in a release build | `TOOLS_ENABLED` gating |

---

## Boundary 1: Godot Wrapper ↔ DragonBones Runtime

### Ownership rules (memorize these)

| Object | Owner | You must |
|--------|-------|----------|
| `dragonBones::Armature` / `Slot` / `Bone` / `TextureData` | the runtime (pools, `Armature`) | never `memdelete`; hold raw pointers only |
| `DragonBonesArmature` | itself, via `dbClear()` → `memdelete(this)` | never free it yourself; call `release()` |
| `DragonBonesSlot` / `Bone` / `EventObject` / `UserData` | `Ref<>` ref-counting | hold as `Ref<>` |
| `DragonBonesMeshDisplay` | the static pool | always go through `from_pool()` / `release()` |
| `RID`s | the owning node | free exactly once, in the destructor |

A wrapper's raw pointer to a runtime object is **non-owning**:

```cpp
class DragonBonesSlot : public RefCounted {
	Slot_GD *slot{ nullptr }; // 生命周期由 dragonBones::Armature 管理
```
— `src/slot.h:87`

### Data flow for the common case

```
*_ske.json / *_tex.json
  → DragonBonesImportPlugin::try_import        (editor)      :171-188
  → factory.load_texture_atlas_json_file_list  (atlas FIRST) :301-315
  → factory.load_dragon_bones_ske_file_list                  :252-268
  → DragonBonesFactoryFileProcessor::save_factory_file_cfg   :545-558
  → *.dbfactory (Resource)

  → DragonBonesArmatureView (Node2D) owns a Ref<DragonBonesFactory>
  → factory.create_armature(...)  → DragonBonesArmature
  → per-frame: _notification(INTERNAL_PROCESS) → advance(delta)
  → _draw() → DragonBonesMeshDisplay → append_draw_data → RenderingServer meshes
  → events: runtime dispatchDBEvent → emit_signal(SNAME("event_dispatched"))
```

At each arrow ask: who owns the object, what type is it on each side, and what happens on failure?

### Boundary checklist

- [ ] Traced which side owns every pointer involved.
- [ ] Confirmed type conversion goes through `to_gd_str`/`to_std_str` (not ad-hoc).
- [ ] Confirmed runtime parser input is NUL-terminated (`length + 1`, `src/factory.cpp:72-75`).
- [ ] Confirmed failure paths return a type-correct default or `Error`, not a crash.
- [ ] Checked the pointer handed to scripts is long-lived and not pooled.
- [ ] Checked `is_editor_hint()` guards where the runtime must not run in the editor.

---

## Boundary 2: C++ Binding ↔ Script / Doc Surface

A bound API is a **contract with three writers**: the C++ header, `_bind_methods`, and `doc_classes/<Class>.xml`. Scripts and the inspector read the union of these.

### Checklist: before renaming or adding a bound member

- [ ] Searched every occurrence: `grep -rn "<name>" src/ doc_classes/ demo/`
- [ ] Updated the header declaration.
- [ ] Updated `_bind_methods` — `D_METHOD` argument names included.
- [ ] Updated `doc_classes/<Class>.xml`, including `setter`/`getter` attributes which must match the **bound** names (`flip_x` → `set_flip_x_`, `active` → `is_active`).
- [ ] Updated the demo scene/script if it uses it (`demo/dragonbones_demo/demo.gd`, `demo.tscn`).
- [ ] Treated it as a breaking change if the member was already released.

> **Gotcha**: the property setter is often a `_`-suffixed trampoline while the real method has extra arguments — `set_flip_x_(bool)` forwards to `set_flip_x(bool, bool)` (`src/armature.h:206-207`). Bind and document the trampoline, not the multi-arg method.

> **Gotcha**: `DragonBonesArmatureView::AnimFadeOutMode` is only a `using` alias (`src/armature_view.h:53`); its `BIND_ENUM_CONSTANT` block is commented out (`src/armature_view.cpp:624-630`). Registering the same constants from both classes is a duplicate-registration bug.

---

## Boundary 3: Build Systems ↔ Platforms

### Checklist: before changing sources, linkage, or a widely-included header

- [ ] Applied the change to **both** `SConstruct` and `CMakeLists.txt`.
- [ ] Verified the SCons source list: `scons -n platform=<host> arch=x86_64 target=template_debug` contains no `thirdparty/godot-cpp/**`.
- [ ] Built `template_debug` **and** `template_release` (the latter is where `TOOLS_ENABLED` is off).
- [ ] Built **Web**. This is the only platform that links with `--whole-archive`, so it is the only one that fails on duplicate symbols.

> **Warning**: a source-glob regression that double-compiles `godot-cpp` passes on Linux, macOS, Windows, iOS, and Android — 20 of 22 CI jobs — and fails only on Web. Do not conclude "the build is fine" from a host-only build when the change touches linkage. Details in [../build/build-systems.md](../build/build-systems.md).

### Checklist: modifying engine-version-dependent code

- [ ] Confirmed the change compiles across the supported range, not just your local Godot.
- [ ] Used one alias + one guard (`src/armature.h:43-52`) or `decltype` (`src/editor/dragon_bones_editor_plugin.h:72`) rather than branching per call site.
- [ ] If support genuinely narrows, updated `compatibility_minimum` in the `.gdextension` and said so explicitly.

---

## Boundary 4: Editor ↔ Runtime

Editor code compiles only when `env.debug_features` is set; `template_release` excludes `src/editor/*.cpp` entirely (`SConstruct:94-96`).

### Checklist: adding editor-facing behavior

- [ ] Every declaration **and** definition wrapped in `#ifdef TOOLS_ENABLED` with `#endif // TOOLS_ENABLED`.
- [ ] No non-guarded path references an editor symbol.
- [ ] Editor plugin registration and removal kept symmetric (`src/dragon_bones_registration.cpp:61` vs `:91`).
- [ ] Runtime side effects guarded by `Engine::get_singleton()->is_editor_hint()` (`src/armature.cpp:157-160`).
- [ ] Inspector refresh triggered when a setter changes what the inspector shows (`notify_property_list_changed()`, `src/armature_view.cpp:140-142`).
- [ ] Verified `template_release` still builds.

---

## Serialization Boundary

Structures that round-trip through `.dbfactory` or scene files have a schema split across writer and reader, and both must change together:

| Structure | Writer / Reader |
|-----------|-----------------|
| `armature_settings` / `sub_armatures` | `get_settings` / `set_settings` — `src/armature.cpp:637-693` |
| Factory file lists | `_save` / `_load` — `src/factory.cpp:600-612`, `:649-686` |
| `.dbfactory` config schema | `save_factory_file_cfg` / `parse_factory_file_cfg` — `src/factory.cpp:521-558` |

### Checklist: changing a serialized structure

- [ ] Updated both the writer and the reader.
- [ ] Decided the compatibility story: old files without the new field, and new files read by an older build.
- [ ] Confirmed the property's `PROPERTY_USAGE_*` flags mean it is actually stored (`PROPERTY_USAGE_STORAGE`).
- [ ] Round-tripped a real file through save + load, both in-editor and at runtime.

> **Warning**: `set_settings` currently has an inner-loop index bug — `for (size_t j = 0; j < slot_names.size(); ++i)` increments the outer `i` (`src/armature.cpp:649`), so `sub_armatures` restore is unreliable. If you touch this path, fix the loop rather than writing around it.

---

## When to Write a Flow Document

Write a dedicated flow doc when a feature:

- spans 3+ of the boundaries above,
- adds a new serialized field,
- introduces a new class registered with a different init level, or
- has caused a bug before.
