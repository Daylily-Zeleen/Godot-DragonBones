# GDExtension Binding Layer Guidelines

> 本文档记述 `src/`（不含 `src/editor/`）的编码约定：Godot 类绑定、内存与生命周期、错误处理、类型转换。
> The English body below is the normative convention set; it describes how to write code in this layer.

**Applies to**: `src/*.cpp`, `src/*.h`, `register_types.cpp`. Not `src/editor/` (see `editor/`), not `thirdparty/` (vendored, do not follow these rules).

---

## Layer Overview

This is a thin Godot wrapper over the vendored DragonBones C++ runtime. Two class families never mix:

| Family | Identity | Rule |
|--------|----------|------|
| **Godot wrapper** (`DragonBones*`) | `namespace godot`, `GDCLASS(...)`, registered via `GDREGISTER_*` | Follow this spec. |
| **Runtime subclass** (`Slot_GD`, `DragonBonesTextureData`, ...) | derives from `dragonBones::*`, uses `BIND_CLASS_TYPE_A/B` | Follow runtime conventions; never add `GDCLASS`. |

Public API surface registered in `src/dragon_bones_registration.cpp:56-76`: `DragonBonesFactory` (Resource) and `DragonBonesArmatureView` (Node2D) are user-instantiable (`GDREGISTER_CLASS`); `DragonBonesBone/Slot/Armature/UserData/EventObject` are handles only (`GDREGISTER_ABSTRACT_CLASS`); resource formats are plumbing (`GDREGISTER_INTERNAL_CLASS`).

---

## Pre-Development Checklist

Before writing code in this layer:

- [ ] Read [directory-structure.md](./directory-structure.md) — where the file belongs and its include order.
- [ ] Read [class-binding.md](./class-binding.md) before adding/renaming any bound method, property, signal, or enum.
- [ ] Read [memory-and-lifetime.md](./memory-and-lifetime.md) before touching any raw `dragonBones::*` pointer or `Ref<>` member.
- [ ] Read [error-handling.md](./error-handling.md) before choosing how a failure surfaces.
- [ ] Read [type-conversion.md](./type-conversion.md) before converting `String`/`Vector2`/`Transform2D` across the boundary.
- [ ] Searched for an existing wrapper method before adding a new one — ~40 methods on `DragonBonesArmatureView` already forward to `DragonBonesArmature` (`src/armature_view.cpp:657-802`). See [code-reuse-thinking-guide.md](../guides/code-reuse-thinking-guide.md).
- [ ] Confirmed the change compiles on the full CI matrix, not only your host platform (see [../build/ci-and-release.md](../build/ci-and-release.md)).

## Quality Check

After implementing, before commit:

- [ ] `scons platform=<host> target=template_debug debug_symbols=yes` succeeds.
- [ ] Web builds are re-verified when the change touches linking, sources globbing, or any header included by broadly-shared files — Web is the only platform that links with `--whole-archive` and therefore the only one that surfaces duplicate-symbol errors (see [../build/build-systems.md](../build/build-systems.md)).
- [ ] Every new public method is either bound in `_bind_methods()` or intentionally internal (say which, in the PR).
- [ ] Every new bound member has a matching entry in `doc_classes/<Class>.xml`; setter names in XML match the names actually bound (e.g. `flip_x` → `set_flip_x_`).
- [ ] Ownership of every new pointer is documented, or the pointer is not raw.
- [ ] No new `#ifdef` other than `TOOLS_ENABLED` / `DEBUG_ENABLED` / Godot version guards — the existing three, and nothing platform-specific by hand.

---

## Topic Files

| Guide | Description |
|-------|-------------|
| [Directory Structure](./directory-structure.md) | File layout, include ordering, license header, namespaces |
| [Class Binding](./class-binding.md) | `GDCLASS`, `_bind_methods`, properties, signals, enums, register level |
| [Memory & Lifetime](./memory-and-lifetime.md) | `memnew`/`memdelete`, `Ref<>`, pools, raw runtime pointers, RID ownership |
| [Error Handling](./error-handling.md) | `ERR_FAIL_*` selection, `Error` returns, editor-hint guards |
| [Type Conversion](./type-conversion.md) | `String` ↔ `std::string`, math types, version-gated containers |
| [Quality Guidelines](./quality-guidelines.md) | Formatting, Godot-version support policy, known tech debt |
