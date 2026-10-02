# Class Binding

> 本文件记述 Godot 类的绑定约定：`GDCLASS`、`_bind_methods`、属性、信号、枚举与注册级别。

## Overview

Every Godot-facing class declares `GDCLASS(Class, Base)` as the first line inside the class body, and a `static void _bind_methods();` declaration. Registration happens centrally in `src/dragon_bones_registration.cpp`, never via static initializers in the class file.

## Class Declaration

```cpp
namespace godot {
class DragonBonesX : public RefCounted {
	GDCLASS(DragonBonesX, RefCounted)
	...
protected:
	static void _bind_methods();
	_DEFINE_TO_STRING()
};
} //namespace godot
VARIANT_ENUM_CAST(godot::DragonBonesX::SomeEnum);
```

| Class | Base | Evidence |
|-------|------|----------|
| `DragonBonesArmature` | `Object` + `Display` + `dragonBones::IArmatureProxy` | `src/armature.h:58-59` |
| `DragonBonesArmatureView` | `Node2D` | `src/armature_view.h:43-44` |
| `DragonBonesBone` | `RefCounted` | `src/bone.h:40-41` |
| `DragonBonesSlot` | `RefCounted` | `src/slot.h:83-84` |
| `DragonBonesUserData` | `RefCounted` | `src/event_object.h:40-41` |
| `DragonBonesEventObject` | `RefCounted` | `src/event_object.h:74-75` |
| `DragonBonesFactory` | `Resource` + **private** `dragonBones::BaseFactory` | `src/factory.h:50-51` |
| `ResourceFormatSaverDragonBones` | `ResourceFormatSaver` + **protected** `DragonBonesFactoryFileProcessor` | `src/factory.h:150-151` |

Private/protected runtime inheritance (`private dragonBones::BaseFactory`) is the pattern that keeps the runtime API out of the script-visible surface — use it rather than composition when the wrapper must satisfy a runtime interface.

Classes that expose nothing define `_bind_methods()` empty and inline: `static void _bind_methods() {}` (`src/armature.h:239`, `src/factory.h:153,166`).

**Do not use `GDCLASS` on runtime subclasses.** `Slot_GD` uses `BIND_CLASS_TYPE_A(Slot_GD)` (`src/slot.h:44`); `DragonBonesTextureData`/`DragonBonesTextureAtlasData` use `BIND_CLASS_TYPE_B` (`src/texture_atlas_data.h:42,85`). The internal singleton `DragonBones` has no binding at all (`src/dragon_bones.h:41`).

## Registration Levels

`src/dragon_bones_registration.cpp:56-76` groups by init level. Pick the narrowest that works:

| Level | Macro | Use for | Evidence |
|-------|-------|---------|----------|
| SCENE, user-instantiable | `GDREGISTER_CLASS` | `DragonBonesFactory`, `DragonBonesArmatureView` | `:65-66` |
| SCENE, handle-only | `GDREGISTER_ABSTRACT_CLASS` | Bone/Slot/Armature/UserData/EventObject | `:68-72` |
| SCENE, plumbing | `GDREGISTER_INTERNAL_CLASS` | `ResourceFormatSaver/LoaderDragonBones` | `:74-75` |
| EDITOR, `TOOLS_ENABLED` | `GDREGISTER_INTERNAL_CLASS` | Export/Import/EditorPlugin, ArmatureProxy | `:54-62` |

Editor plugins additionally need `EditorPlugins::add_by_type<...>()` at EDITOR level (`:60`) and `EditorPlugins::remove_by_type<...>()` at uninit (`:89-93`).

## Methods

```cpp
ClassDB::bind_method(D_METHOD("play", "animation_name", "loop_count"), &DragonBonesArmature::play, DEFVAL(-1));
```
— `src/armature.cpp:76`

- `D_METHOD` argument names must match the actual parameter intent (`p_` prefix dropped): `D_METHOD("fade_in", "animation_name", "time", "loop", "layer", "group", "fade_out_mode")` (`src/armature.cpp:81`).
- Defaults use `DEFVAL(...)`; list only the trailing optional parameters.
- Property-setter trampolines are exposed under the `_`-suffixed name while the real method keeps its extra argument — declare `set_flip_x_(bool)` forwarding to `set_flip_x(bool, bool)` (`src/armature.h:206-207`), bind the trampoline (`src/armature.cpp:106`) and use it in `ADD_PROPERTY` (`src/armature.cpp:119`).
- `Callable`-taking iteration is bound as `for_each_armature_` over the templated `for_each_armature` (`src/armature.cpp:66-67`, decl `src/armature.h:112-142`).

## Properties

Use explicit `PropertyInfo` — never the implicit type form:

```cpp
ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture_override", PROPERTY_HINT_RESOURCE_TYPE, Texture2D::get_class_static()), "set_texture_override", "get_texture_override");
```
— `src/armature.cpp:122`

- Hints carry real constraints: `PROPERTY_HINT_RANGE, "0.0,1.0,0.0001"` (`src/armature.cpp:116`), `PROPERTY_HINT_FILE, "*.dbjson,*.json,*.dbbin"` via `vformat("%d/%d:%s", ...)` (`src/factory.cpp:424`).
- Group related properties with `ADD_GROUP(name, prefix)` — `ADD_GROUP("Flip", "flip_")` (`src/armature.cpp:118`).
- Read-only collections bind a stub setter whose body fails: `ERR_FAIL_MSG("\"ints\" is readonly.")` (`src/event_object.cpp:56-58`), property marked `PROPERTY_USAGE_READ_ONLY` (`src/event_object.cpp:158`).
- Editor-only properties are hidden behind `PROPERTY_USAGE_EDITOR` (`src/armature.cpp:115-116`).

**Dynamic properties** — synthesize via `_set`/`_get`/`_get_property_list` only on the two editor-facing wrappers (`DragonBonesArmatureView` `src/armature_view.cpp:241-286`; `DragonBonesArmature`/`DragonBonesArmatureProxy` `src/armature.cpp:576-633,698-769`). Look up names with `SNAME("...")`; push `DICTIONARY` properties with `PROPERTY_USAGE_STORAGE` for state that must round-trip.

## Signals

```cpp
ADD_SIGNAL(MethodInfo("event_dispatched", PropertyInfo(Variant::OBJECT, "event_object", PROPERTY_HINT_NONE, "", PROPERTY_HINT_NONE, DragonBonesEventObject::get_class_static())));
```
— `src/armature.cpp:124`

Emit with the cached `StringName` macro: `emit_signal(SNAME("event_dispatched"), event_object);` (`src/armature.cpp:164`).

## Enums

Plain unscoped `enum` in the public section (`src/armature.h:75-83`, `src/armature_view.h:47-51`, `src/bone.h:48-52`, `src/event_object.h:81-92`), cast **outside** the namespace closing brace (`src/armature.h:257`, `src/armature_view.h:203`, `src/bone.h:101`, `src/event_object.h:135`), and one `BIND_ENUM_CONSTANT` per value at the end of `_bind_methods` (`src/armature.cpp:126-132`).

> **Gotcha**: `DragonBonesArmatureView::AnimFadeOutMode` is only a `using` alias of the Armature enum (`src/armature_view.h:53`) and its `BIND_ENUM_CONSTANT` block is commented out (`src/armature_view.cpp:624-630`). Only `DragonBonesArmature` owns the registration — do not re-register the same constants from the View (duplicate enum registration).

## `_bind_methods` Must Stay in Sync

The `_bind_methods` block, the header declaration, and `doc_classes/<Class>.xml` are one contract:

- Every bound method needs a `<method>` entry; the doc `setter`/`getter` attributes must match the bound names exactly (`flip_x` → `set_flip_x_`, `active` → `is_active`).
- Renaming a bound method is a **breaking API change** — rename in `_bind_methods`, the header, `doc_classes/`, and the demo scene/script together.

### Convention: `_to_string`

Use `_DEFINE_TO_STRING()` (`src/godot_dragon_bones.h:71-72`) rather than hand-writing it. The macro yields `vformat("<%s#%s>", get_class_static(), get_instance_id())` and is already applied in `src/armature.h:157,240`, `src/armature_view.h:77`, `src/slot.h:101`, `src/event_object.h:50`.
