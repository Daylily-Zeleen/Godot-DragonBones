# Type Conversion

> 本文件记述 Godot ↔ DragonBones 类型转换约定：字符串、数学类型、版本门控容器、字符串常量缓存，以及运行时的 16 位窄缓冲区（`intArray` / `frameIntArray`）偏移读取契约。

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

## Narrow Runtime Buffers (`int16_t *`)

The vendored runtime stores every per-data offset in **16-bit** buffers, but the offsets themselves are 16-bit *unsigned* counters squeezed into `int16_t` elements:

| Buffer | Declaration | Written by |
|--------|-------------|------------|
| `DragonBonesData::intArray` | `const int16_t *` — `thirdparty/dragonBones/model/DragonBonesData.h:120` | `JSONDataParser::_intArray` (`std::vector<std::int16_t>`, `parser/JSONDataParser.h:185`) |
| `DragonBonesData::frameIntArray` | `const int16_t *` — `DragonBonesData.h:128` | `JSONDataParser::_frameIntArray` (`JSONDataParser.h:187`) |

A value above 32767 therefore lands as a **negative** `int16_t`, and the runtime compensates with a manual wrap. That wrap only works if the receiving variable is ≥ 32-bit wide:

```cpp
// Don't — `auto` deduces int16_t, so `+= 65536` overflows back to negative.
// The later (size_t) cast then yields a ~4e9 offset and reads heap garbage.
auto weightFloatOffset = intArray[weightData->offset + (unsigned)BinaryOffset::WeightFloatOffset];
if (weightFloatOffset < 0) {
	weightFloatOffset += 65536;
}

// Do — widen first, then wrap.
int32_t weightFloatOffset = intArray[weightData->offset + (unsigned)BinaryOffset::WeightFloatOffset];
if (weightFloatOffset < 0) {
	weightFloatOffset += 65536;
}
```
— `src/slot.cpp:279` (fixed), the wrap at `:282`.

> **Warning**: the failure is **non-deterministic** — the bogus index reads whatever the heap happens to hold. Symptom is either "part collapsed onto the armature origin" (garbage reads as 0) or "explosive stretching" (garbage reads as ~1e5). Do not dismiss a mesh-rendering bug just because a rerun looks fine.

### Every wrap site must be ≥ 32-bit

Audit these whenever a buffer layout changes. All current sites are correct — the list is the contract, not a to-do:

| Site | Variable | Width |
|------|----------|-------|
| `src/slot.cpp:172` | `int vertexOffset` | 32 — OK |
| `src/slot.cpp:279` | `int32_t weightFloatOffset` | 32 — **regression fixed** |
| `src/slot.cpp:318` | `int vertexOffset` | 32 — OK |
| `thirdparty/dragonBones/animation/TimelineState.cpp:624,648` | `int colorOffset` | 32 — OK |
| `thirdparty/dragonBones/animation/TimelineState.cpp:851` | `unsigned vertexOffset` | 32 — OK |

`int16_t`-valued fields that need **no** wrap are counts and indices, not offsets (`MeshVertexCount`, `MeshTriangleCount`, per-vertex `boneCount`, `boneIndex`, `MeshVertexIndices`) — their ranges are bounded by the mesh itself.

### When it triggers

An offset exceeds 32767 once the shared `floatArray` crosses ~32 K floats — roughly 4 000 plain-mesh vertices, or ~3 400 skinned vertices. `demo/dragonbones_demo/assets/test/idle_ske.json` is past that line: 3 of its 37 skinned slots (`arm_L`=35288, `front_hair`=33112, `headwear`=37966) and 2 `MeshFloatOffset`s (`arm_L`=33832, `headwear`=37118) are over the limit. `Sheep_ske.json` (~1 600 floats) and `龙_ske.json` (~1 900) stay under it, which is why the bug stayed invisible on the smaller assets.

### Verifying a data set

Replay the parser's write order offline instead of guessing from file size — offsets are cumulative across every preceding slot in the same skin:

- `JSONDataParser::_parseMesh` (`parser/JSONDataParser.cpp:609-620`) grows `_intArray` by `4 + 3 * triangleCount` per mesh, and `_floatArray` by `vertexCount * 2` (vertices) plus `vertexCount * 2` (uvs).
- When `weights` is present, `_parseMesh` (`:639-716`) additionally grows `_intArray` by `1 + 1 + weightBoneCount + vertexCount + weightCount`, and `_floatArray` by `weightCount * 3`.
- `MeshFloatOffset` / `WeightFloatOffset` are snapshots of the respective buffer size **at that moment**, so any later slot inherits every earlier slot's growth. Sum them in slot order and flag anything above 32767.

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
- [ ] Every `intArray` / `frameIntArray` offset read is ≥ 32-bit before its `+= 65536` wrap — never `auto`.
