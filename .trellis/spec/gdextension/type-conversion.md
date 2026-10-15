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
— `src/slot.cpp:278`, the wrap at `:281`.

> **Warning**: the failure is **non-deterministic** — the bogus index reads whatever the heap happens to hold. The garbage then feeds the weight math, so vertices land wherever that garbage points; observed results ranged from parts collapsing toward their bones' joint positions to explosive stretching. Do not dismiss a mesh-rendering bug just because a rerun looks fine.

### Every wrap site must be ≥ 32-bit

Audit these whenever a buffer layout changes. All current sites are correct:

| Site | Variable | Width |
|------|----------|-------|
| `src/slot.cpp:172` | `int vertexOffset` | 32 — OK |
| `src/slot.cpp:278` | `int32_t weightFloatOffset` | 32 — **regression fixed** |
| `src/slot.cpp:318` | `int32_t vertexOffset` | 32 — **regression fixed** |
| `thirdparty/dragonBones/animation/TimelineState.cpp:624,648` | `int colorOffset` | 32 — OK |
| `thirdparty/dragonBones/animation/TimelineState.cpp:851` | `unsigned vertexOffset` | 32 — OK |

`int16_t`-valued fields that need **no** wrap are counts and indices, not offsets (`MeshVertexCount`, `MeshTriangleCount`, per-vertex `boneCount`, `boneIndex`, `MeshVertexIndices`) — their ranges are bounded by the mesh itself.

> **Note**: the `slot.cpp` regression came from `7549e92` ("一些可读性优化"), which rewrote `int weightFloatOffset` as `auto weightFloatOffset`; `auto` deduces `int16_t` from the buffer. Prefer an explicit width over `auto` for anything read out of these buffers.

### Load-time guard

The wrap above only recovers offsets up to **65535**. Beyond that the true value is gone: `85878` is stored as `20342`, and `20342` is itself a legitimate offset — so scanning a buffer **cannot** distinguish a truncated value from a valid one. The judgment therefore has to be made from the source data, not from the packed buffers.

`src/packed_offset_check.cpp` recomputes the offsets from the skeleton JSON before the data is accepted. It runs in **debug builds only** (`#ifdef DEBUG_ENABLED`, `src/factory.cpp:309`): `DragonBonesFactory::load_data` calls `check_packed_offsets()` and fails the load with `ERR_INVALID_DATA` when a peak crosses `PACKED_OFFSET_LIMIT`:

```
DragonBones data "<path>" exceeds the packed-offset limit (65535):
floatArray peak 87778 (limit 65535) at "headwear";
intArray peak 70115 (limit 65535) at "eyelash_up_c2_2".
Parts would be distorted if loaded; split the asset or reduce vertex count.
```

The check lives in **our** layer and is a pure read of the skeleton file — the vendored runtime is not modified, and no runtime intermediate state is needed. Three properties are load-bearing:

- **Peak offset, not buffer total.** One large mesh can push `floatArray` past 65535 while its own `MeshFloatOffset` is still 0 — narrow mode does not misread that, so flagging it would reject valid assets.
- **Recompute from the source, not from the buffers.** A truncated value in a buffer is indistinguishable from a valid one (see above).
- **Same accumulation order as the parser.** Offsets are `.size()` snapshots taken in parse order, so `check_packed_offsets` must mirror it exactly: per armature, all `skin` (slot → display) first, then all `animation`.

Release builds skip the check — a truncated offset then renders as stretched parts rather than a diagnosable failure, so never treat a release-mode rendering complaint as "the data is fine" just because debug accepted it. Covered so far: `floatArray` (`MeshFloatOffset` / `WeightFloatOffset`) and `intArray` (`colorOffset`). `.dbbin` is deliberately **not** covered — it never passes through the JSON packer, and its 16-bit offsets are written by the official exporter. `PACKED_OFFSET_LIMIT` (`src/packed_offset_check.h`) is the single definition point.

### When it triggers

An offset exceeds 32767 once the shared `floatArray` crosses ~32 K floats — roughly 4 000 plain-mesh vertices, or ~3 400 skinned vertices. Whether a data set is affected depends on how much geometry precedes each slot, not on the size of any single part.

All six shipped demo assets are currently inside the limit (`test/idle_ske.json`: `floatArray` peak 41666, `intArray` peak 30571; `test2`: 38800 / 31859; `Sheep` and `龙` far smaller; `Dragon` uses no packed meshes). An earlier export of `test/idle_ske.json` was far past it and is what motivated the check; re-exporting brought it back under.

This asymmetry matters: an offset above 32767 but at or below 65535 renders **correctly** thanks to the wrap, so the guard must stay silent for it — only values above 65535 are unrecoverable. Do not "tighten" the limit to 32767.

To verify a data set, re-derive it independently (`.agent_tmp/sim_offsets2.py`, `.agent_tmp/sim_color.py`) and confirm the numbers match what the error message reports. The replay has to follow the parser's real growth order:

- `_parseMesh` grows `_intArray` by `4 + 3 * triangleCount`, and `_floatArray` by `vertexCount * 2` (vertices) plus `vertexCount * 2` (uvs).
- With `weights` present it additionally grows `_intArray` by `1 + 1 + weightBoneCount + vertexCount + weightCount`, and `_floatArray` by `weightCount * 3`.
- `_parseSlotColorFrame` adds 8 per **explicit** colour block (one reusing the default block adds none).
- `MeshFloatOffset` / `WeightFloatOffset` / `colorOffset` are snapshots of the respective buffer size **at that moment**, so every later slot inherits all earlier growth.

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
- [ ] A new offset class written into a packed buffer is also covered by `check_packed_offsets` (`src/packed_offset_check.cpp`, debug builds) — the `+65536` wrap cannot recover values above 65535, so the check must catch them at load time.
