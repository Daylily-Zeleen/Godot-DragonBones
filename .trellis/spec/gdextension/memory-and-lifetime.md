# Memory & Lifetime

> 本文件记述内存与对象生命周期约定：分配器桥接、`memnew`/`memdelete`、`Ref<>`、对象池、原始指针归属、RID。

## Overview

Two allocators meet in this codebase and must never cross. The vendored DragonBones runtime is forced onto Godot's allocator through macros in `src/godot_dragon_bones.h:40-55`, and every cross-boundary allocation must go through `memnew`/`memdelete` (or `Ref<>`), never `new`/`delete`.

```cpp
#define DRAGONBONES_MALLOC(size)            memalloc(size)
#define DRAGONBONES_REALLOC(ptr, new_size)  memrealloc(ptr, new_size)
#define DRAGONBONES_FREE(ptr)               memfree(ptr)
#define DRAGONBONES_NEW(T)                  memnew(T)
#define DRAGONBONES_DELETE(ptr)             godot::memdelete(ptr)
#define RAPIDJSON_MALLOC(size) DRAGONBONES_MALLOC(size)   // same, kept in sync
```
— `src/godot_dragon_bones.h:40-55`

> **Rule**: never add a raw `new`/`delete` in `src/`. If you must allocate, use `memnew`/`memdelete`, `memnew_arr`/`memdelete_arr`, or `Ref<>`.

## Ownership Model

| Kind | Declaration | Who frees | Evidence |
|------|-------------|-----------|----------|
| Runtime object | raw `dragonBones::*` pointer | the runtime (`Armature`), or a pool | `src/bone.h:44`, `src/slot.h:87` |
| Godot wrapper handle | `Ref<T>` | ref-counting | `src/armature_view.h:56`, `src/slot.h:87` |
| User-visible Godot object | raw `DragonBonesArmature *` | `dbClear()` → `memdelete(this)` | `src/armature.cpp:519-521` |
| Pooled render helper | raw `DragonBonesMeshDisplay *` | static pool | `src/mesh_display.h:93-96` |
| RenderingServer mesh | `RID` | explicit `free_rid` | `src/armature_view.cpp:645-648` |

### Rule: wrapper handles hold raw runtime pointers

A `DragonBones*` wrapper stores a non-owning pointer to its runtime counterpart. The runtime owns the lifetime; the wrapper must never `memdelete` it.

```cpp
class DragonBonesSlot : public RefCounted {
	Slot_GD *slot{ nullptr }; // 生命周期由 dragonBones::Armature 管理
```
— `src/slot.h:87`

Correspondingly, `DragonBonesBone` comments the same contract at `src/bone.h:44`.

### Rule: user-visible armatures must not be pooled

`DragonBonesArmature::dbClear()` deletes `this` rather than returning to a pool, because the pointer is handed to users who may compare it:

```cpp
void DragonBonesArmature::dbClear() {
	armature_instance = nullptr;
	// 不能回池重复利用，因为要直接暴露给用户，用户可以直接比较指针导致非预期情形。
	memdelete(this);
}
```
— `src/armature.cpp:518-522`

Do not add a pool for `DragonBonesArmature`. Do not `memdelete` one yourself — `DragonBonesArmatureView::~DragonBonesArmatureView` calls `armature->release()` instead (`src/armature_view.cpp:637-641`), and the README documents the same rule for users (`README.md:80`).

### Rule: `Display::release()` must be implemented by every subclass

`Display` is a pure-virtual base whose `release()` frees the subclass's own storage. Subclasses must also delete through the **first base pointer** — the multi-inheritance note is explicit:

```cpp
virtual void release(); // NOTE: 子类要在此出处理自身的内存管理 （多继承的情况下必须用指在开头的指针才能 memdelete）
```
— `src/mesh_display.h:68`

`Slot_GD::_disposeDisplay` is the caller that relies on this (`src/slot.cpp:111-118`).

## Ref Counting

- Create with `.instantiate()`: `saver.instantiate();` (`src/dragon_bones_registration.cpp:80`), `DragonBonesFactory...` (`src/factory.cpp:484,535,549`).
- Clear with `.unref()`: `saver.unref()` (`src/dragon_bones_registration.cpp:100`), `display_texture.unref()` (`src/texture_atlas_data.h`), `texture_override.unref()` (`src/armature.cpp:534`).
- Module-level holders are file-static `Ref<>` (`src/dragon_bones_registration.cpp:50-51`) so the saver/loader outlive any single scene.
- `Ref<Texture2D>` members must be released in `_onClear()`-style cleanup: `src/armature.cpp:534`, and the runtime subclass pattern in `src/texture_atlas_data.h:96-99`.

## Object Pools

Two pools exist; both are cleared at module teardown.

| Pool | Clear call | Evidence |
|------|-----------|----------|
| `dragonBones::BaseObject` | `BaseObject::clearPool()` | `src/dragon_bones_registration.cpp:96` |
| `DragonBonesMeshDisplay` | `DragonBonesMeshDisplay::clear_pool()` | `src/dragon_bones_registration.cpp:97` |

`DragonBonesMeshDisplay::release()` guards against double-free before returning to the pool (`src/mesh_display.cpp:111-120`); `from_pool()` pops or allocates (`src/mesh_display.cpp:124-130`). If you add fields to `DragonBonesMeshDisplay`, clear them in `release()` — a pooled object is reused across armatures.

> **Gotcha**: `Slot_GD::_disposeDisplay` deliberately treats `isRelease == false` identically to `true`, because the base class leaks otherwise. Keep this behavior and its comment:
> ```cpp
> void Slot_GD::_disposeDisplay(void *value, bool _isRelease) {
> 	/**
> 		基类里错误处理：
> 		isRelease == false 时和 true 时一样没有保持对指针的引用，将导致内存泄漏。
> 		因此这里统一以 true 时一样处理
> 	*/
> ```
> — `src/slot.cpp:111-116`

## RenderingServer RIDs

RIDs are manually owned and must be freed exactly once.

- Per-mesh cache: `LocalVector<RID> draw_meshes` (`src/armature_view.h:69`); lazily created via `get_draw_mesh()`, surfaces cleared each draw, all freed with `free_rid` in the destructor (`src/armature_view.cpp:645-648`).
- Debug RID: `#ifdef DEBUG_ENABLED` guarded member, created on `set_debug`, freed in the destructor (`src/armature_view.cpp:650-654`).
- Cached blend materials: `Ref<CanvasItemMaterial>` map cleaned by a registered static callback (`src/armature_view.cpp:57-59,631`).

## Cleanup Callbacks

Static tables that outlive any single instance register a teardown hook rather than relying on destructors:

```cpp
void DragonBones::add_clean_static_callback(CleanCallback *p_func);
static LocalVector<CleanCallback *> clean_callbacks;
```
— `src/dragon_bones.h:66-71`; invoked from `~DragonBones` (`src/dragon_bones.cpp:44,50-56`).

Register a callback (like `src/armature.cpp:152` and `src/armature_view.cpp:631`) whenever you add file-static state that holds Godot objects.

## Checklist

- [ ] No `new`/`delete` introduced in `src/`.
- [ ] Every new raw pointer's owner is stated in a comment (runtime / pool / `memdelete(this)`).
- [ ] Every new `Ref<>` member is cleared on teardown.
- [ ] Every new RID is freed in the destructor and only once.
- [ ] New static state registers a `add_clean_static_callback` hook.
