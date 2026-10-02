# Type Conversion

> 本文件记述 Godot ↔ DragonBones 类型转换约定：字符串、数学类型、版本门控容器、字符串常量缓存。

## Overview

Every conversion happens at the boundary between a `DragonBones*` wrapper and the runtime. Conversion helpers are `_FORCE_INLINE_` free functions or file-local statics — never member functions, never macros that hide allocation.

## String Conversion

Two helpers, in `namespace godot` inside the umbrella header:

```cpp
_FORCE_INLINE_ String to_gd_str(const std::string &p_std_str) {
	return String::utf8(p_std_str.c_str());
}

_FORCE_INLINE_ std::string to_std_str(const String &p_gd_str) {
	return p_gd_str.utf8().get_data();
}
```
— `src/godot_dragon_bones.h:67-73`

> **Rule**: all `String` ↔ `std::string` traffic goes through these. Never call `.utf8()` / `String::utf8()` inline at a call site, and never assume ASCII — DragonBones asset names are frequently CJK (see `demo/dragonbones_demo/assets/龙/`).

Usage pattern — convert at the call, keep the boundary thin:

```cpp
return getArmature()->getArmatureData()->getAnimation(to_std_str(p_animation_name)) != nullptr;
```
— `src/armature.cpp:210`

When building containers, convert the key: `ret[to_gd_str(slot.first)] = slot.second;` (`src/armature.cpp:402`).

## C-String Bridge for Runtime Parsers

The runtime parsers take `const char *`. When handing file bytes to them, NUL-terminate explicitly:

```cpp
PackedByteArray raw_data;
raw_data.resize(file->get_length() + 1);
file->get_buffer(raw_data.ptrw(), file->get_length());
raw_data.set(file->get_length(), 0x00);
```
— `src/factory.cpp:72-75`

This `length + 1` + explicit terminator pattern is deliberate and is the only correct way to pass file contents into `parseDragonBonesData` / `parseTextureAtlasData`.

## Math Types

`Vector2`, `Transform2D`, `Rect2`, and `Color` cross the boundary constructor-by-constructor — there is no generic converter. Keep the construction at the boundary and do not leak runtime math types into a public signature:

```cpp
dict[to_gd_str(constraint.first)] = Vector2(constraint.second->target->transform.x, constraint.second->target->transform.y);
```
— `src/armature.cpp:459`

Tiny per-file converters are acceptable when used more than once in that file — `src/bone.cpp:36` (`to_gd_transform`) and `:45` (`to_db_transform`). Promote one to `src/godot_dragon_bones.h` only when a second file needs it.

## Version-Gated Containers

Godot 4.4 added `TypedDictionary`. The codebase selects the richest available container via a preprocessor guard at the top of `src/armature.h:43-52`:

```cpp
#if GODOT_VERSION_MAJOR > 4 || (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4)
#include "godot_cpp/variant/typed_dictionary.hpp"
using SlotsDictionary = godot::TypedDictionary<godot::String, godot::DragonBonesSlot>;
using BonesDictionary = godot::TypedDictionary<godot::String, godot::DragonBonesBone>;
using ConstraintsDictionary = godot::TypedDictionary<godot::String, godot::Vector2>;
#else // ...
using SlotsDictionary = godot::Dictionary;
#endif // GODOT_VERSION_MAJOR > 4 || (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4)
```

The real names — `Glyph`, `RID`, `safe_ref`, and the `RenderingServer`/`ResourceLoader` APIs — change between Godot minor versions. When a signature differs:

- Prefer the version-guard + alias approach above and keep **one** call-site shape via the alias.
- If only a return type differs, avoid pinning it by hand — use a `decltype` on the engine's own virtual: `virtual decltype(EditorImportPlugin()._get_priority()) _get_priority() const override;` (`src/editor/dragon_bones_editor_plugin.h:72`).
- Never introduce a platform-specific guard for this; only `TOOLS_ENABLED`, `DEBUG_ENABLED`, and Godot version guards are allowed.

> **Rule**: a new API must compile against the whole supported range. Update the declared minimum in `demo/addons/godot_dragon_bones.daylily-zeleen/godot_dragon_bones.gdextension:26` (`compatibility_minimum = 4.3`) if you raise the floor; do not silently drop support for a version you did not intend to drop.

## Cached String Constants

Use `SNAME(...)` for any `StringName` built from a literal at runtime — it caches the `StringName` in a function-local static:

```cpp
#define SNAME(sn) ([] {static const ::godot::StringName ret{sn}; return ret; }())
```
— `src/godot_dragon_bones.h:74`

Required for `emit_signal(SNAME("event_dispatched"), ...)` (`src/armature.cpp:164`, `src/armature_view.cpp:497`) and dynamic property lookups in `_set`/`_get` (`src/armature_view.cpp:242,256`, `src/armature.cpp:577,584,703,727`).

Do **not** construct `StringName("literal")` inline in these paths — it defeats the cache and allocates per call.

## Checklist

- [ ] All string conversion via `to_gd_str` / `to_std_str`.
- [ ] Runtime parser input is NUL-terminated (`length + 1`).
- [ ] No runtime math type in a public signature.
- [ ] New container APIs use a version guard + alias, not a hand-rolled branch per call site.
- [ ] Literal `StringName`s use `SNAME(...)`.
- [ ] Supported Godot range declared in `.gdextension` still matches reality.
