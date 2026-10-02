# Directory Structure

> 本文件记述 `src/` 的文件布局、头文件约定与 include 顺序。

## Overview

One wrapper class pair per file, named in `snake_case` after the primary Godot class.

```
src/
├── godot_dragon_bones.h     # umbrella prelude: allocator macros, str helpers, SNAME
├── dragon_bones.h/.cpp      # internal singleton over dragonBones::DragonBones
├── dragon_bones_registration.h/.cpp  # module init/uninit, GDREGISTER_*
├── armature.h/.cpp          # DragonBonesArmature (+ editor-only ArmatureProxy)
├── armature_view.h/.cpp     # DragonBonesArmatureView (Node2D)
├── bone.h/.cpp              # DragonBonesBone
├── slot.h/.cpp              # Slot_GD (runtime subclass) + DragonBonesSlot (wrapper)
├── factory.h/.cpp           # DragonBonesFactory, ResourceFormat{Loader,Saver}, file processor
├── mesh_display.h/.cpp      # DrawData, Display base, DragonBonesMeshDisplay + pool
├── event_object.h/.cpp      # DragonBonesUserData, DragonBonesEventObject
├── texture_atlas_data.h     # runtime TextureData subclasses (header-only)
└── editor/                  # TOOLS_ENABLED only — see ../editor/
```

Registration is split by level in `src/dragon_bones_registration.cpp:56-76`; the C entry point is `register_types.cpp` (`godot_dragon_bones_library_init`, minimum level `MODULE_INITIALIZATION_LEVEL_SCENE`, `register_types.cpp:9-16`).

**Precedent**: internal runtime classes live with their wrapper when tightly coupled — `Slot_GD` is declared in `src/slot.h:43-81` directly above `DragonBonesSlot` (`src/slot.h:83-114`).

## Naming Conventions

| Element | Convention | Evidence |
|---------|-----------|----------|
| Files | `snake_case`, one class-pair per file | `src/armature_view.h` |
| Bound method names | `snake_case` verb-first | `play_from_progress`, `set_ik_constraint_bend_positive` — `src/armature.h:175,189` |
| Runtime overrides | keep the runtime's exact `camelCase` | `dbInit`, `dbClear`, `_buildArmature`, `_onClear` — `src/armature.h:101-104`, `src/factory.h:65-75` |
| Parameters | `p_` prefix; by-ref outputs `r_` | `p_animation_name` `src/armature.h:162`; `r_factory` `src/factory.h:140` |
| Boolean params | `b_` when not `p_` — mixed, see Known Inconsistencies | `b_reset` vs `p_recursively` `src/armature.h:176-177` |
| Members | bare name, brace-init, no trailing `_` | `armature{ nullptr }`, `active{ true }` — `src/armature_view.h:56-67` |
| `_`-prefixed members | only inside runtime-derived classes, mirroring the runtime | `_textureScale` `src/slot.h:48` |
| Setter/getter | `set_x` / `get_x`; booleans `is_x` | `set_active` / `is_active` `src/armature_view.h:99-100` |

The dual naming rule is intentional: wrapping requires matching the runtime interface exactly, while the Godot-facing surface must be `snake_case`.

## Header Conventions

**`#pragma once`** in every header — never include guards. Evidence: `src/armature.h:31`, `src/bone.h:31`, `src/factory.h:31`.

**License block**: identical 33-line MIT header at the top of every `.h`/`.cpp` in `src/`. Copy it from `src/armature.h:1-30`. (`register_types.cpp` has none — it is the only exception.)

**Include order** — own header first, then blank-line-separated groups:

1. Own header — `#include "armature.h"` (`src/armature.cpp:31`)
2. Vendored runtime — `<dragonBones/...>` (`src/armature.cpp:33`)
3. Godot — `<godot_cpp/...>` (`src/armature.cpp:34-38`)
4. Project-local quoted — `"armature_view.h"` (`src/armature.cpp:40-43`)

Every header begins with `#include <godot_dragon_bones.h>` as the umbrella prelude (`src/armature.h:33`, `src/bone.h:33`, `src/factory.h:33`), except `src/dragon_bones_registration.h` which uses `<godot_cpp/godot.hpp>`.

**`using namespace` only in `.cpp`** — `using namespace godot;` / `using namespace dragonBones;` (`src/armature.cpp:45-46`, `src/factory.cpp:52-53`). Some `.cpp` instead open `namespace godot {` (`src/event_object.cpp:37`, `src/mesh_display.cpp:39`); both are in use.

**Namespaces**: all declarations wrapped in `namespace godot { ... } //namespace godot` (`src/armature.h:54,256`).

**Forward declarations over includes** for pointer members and cross-class returns: `class Slot_GD *slot` (`src/armature.h:62`), `class DragonBonesArmatureView *armature_view` (`src/armature.h:65`), `class DragonBonesArmature *get_child_armature();` (`src/slot.h:113`).

**`friend class`** grants the factory/armature access to attach wrappers: `src/armature.h:66`, `src/slot.h:50-53`, `src/factory.h:117,133`.

**Endif comments**: every `#endif` carries a trailing comment naming its guard — `#endif // TOOLS_ENABLED`, `#endif // DEBUG_ENABLED`, `#endif // GODOT_VERSION_MAJOR ...` (`src/armature.h:52`, `src/armature.cpp:60`).

## Where New Code Goes

| Adding | Location |
|--------|----------|
| New user-facing node/resource | new `src/<name>.h/.cpp` pair + `GDREGISTER_CLASS` in `src/dragon_bones_registration.cpp` |
| New handle/wrapper over runtime object | new pair + `GDREGISTER_ABSTRACT_CLASS` |
| Resource load/save plumbing | `src/factory.h/.cpp` (add to `GDREGISTER_INTERNAL_CLASS` block) |
| Tiny conversion helper | `_FORCE_INLINE_` free function in `src/godot_dragon_bones.h` (shared) or file-local in the `.cpp` (`src/bone.cpp:36,45`) |
| Editor-only behavior | `src/editor/`, guarded by `TOOLS_ENABLED` |

## Known Inconsistencies

Documented as-is; do not treat either side as the convention to propagate.

- `src/bone.cpp:31-32` includes its own headers with angle brackets (`<armature.h>`) while every other `.cpp` uses quotes.
- `src/bone.h:60` and `src/factory.h:83` hand-write `_to_string()` instead of using the `_DEFINE_TO_STRING()` macro; `DragonBonesEventObject` has none at all.
- `using SlotsDictionary = ...` aliases sit at **global** scope in `src/armature.h:45-51`, outside `namespace godot`.
- `SConstruct:34,37` duplicates `import os`; `SConstruct:38` unconditionally shells out to `chcp 65001` (Windows-only concern, runs on Linux CI too).
