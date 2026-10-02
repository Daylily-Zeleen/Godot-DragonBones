# Error Handling

> 本文件记述错误处理约定：`ERR_FAIL_*` 宏选型、`Error` 返回值、编辑器模式守卫、运行时桥接。

## Overview

There is no exception handling and no `assert()` in `src/`. Failures are reported through Godot macros; recoverable operations return `godot::Error`. The vendored runtime mostly returns pointers (possibly `nullptr`), so the wrapper is responsible for converting runtime failure into a Godot-visible failure.

## Macro Selection

Pick by whether the function can return and what it returns.

| Situation | Macro | Evidence |
|-----------|-------|----------|
| Null argument, `void` function | `ERR_FAIL_NULL(p)` | `src/armature.cpp:162` |
| Null member, function returns a value | `ERR_FAIL_NULL_V(member, default)` | `src/armature_view.cpp:660,664,668` |
| Boolean precondition | `ERR_FAIL_COND(cond)` | `src/armature_view.cpp:206` |
| Boolean precondition + return | `ERR_FAIL_COND_V(cond, ret)` | `src/armature_view.cpp:211`, `src/factory.cpp:388` |
| Null + explanation | `ERR_FAIL_NULL_V_MSG(p, ret, msg)` | `src/factory.cpp:165` |
| Condition + explanation | `ERR_FAIL_COND_V_MSG(cond, ret, msg)` | `src/factory.cpp:121` |
| Unconditional failure | `ERR_FAIL_MSG(msg)` | readonly setters, `src/event_object.cpp:57,75,94` |
| Build-mode-gated failure | `ERR_FAIL_V_MSG(ret, msg)` | `src/armature_view.cpp:237` |
| Skip one loop iteration | `ERR_CONTINUE(cond)` | `src/armature.cpp:656` |
| Skip iteration + message | `ERR_CONTINUE_MSG(cond, msg)` | `src/factory.cpp:261,310,313` |

Always return a **type-correct** default: `ERR_FAIL_NULL_V(armature, {})`, `(armature, PackedStringArray())`, `(armature, Ref<Texture2D>())` (`src/armature_view.cpp:664,718,798`).

### Rule: guard every accessor that crosses into the runtime

`DragonBonesArmatureView` delegates to a nullable `armature` pointer and guards each entry point (`src/armature_view.cpp:657-802`). Follow that pattern for any new wrapper method that forwards to an owned runtime object:

```cpp
bool DragonBonesArmatureView::has_animation(const String &p_animation_name) const {
	ERR_FAIL_NULL_V(armature, false);
	...
}
```
— `src/armature_view.cpp:659-661`

## `Error` Returns

File/parse operations return `godot::Error` and accumulate the first failure instead of failing fast:

```cpp
ERR_CONTINUE_MSG(raw_data.is_empty(), (err = ERR_PARSE_ERROR, vformat(...)));
```
— `src/factory.cpp:261` (also `:310`, `:313`)

- Use the comma-operator idiom when a macro needs both a side effect and a message; it is the established form for `ERR_CONTINUE_MSG` here.
- Report `OK` only when every input succeeded; otherwise return the recorded `err`.
- Propagate to callers as `Variant(err)` where the Godot API demands it: `return err;` from `ResourceFormatLoaderDragonBones::_load` on parse failure (`src/factory.cpp:651-654`).

Prefer `Error` over `bool` + out-param for any new file/serialization API.

## Logging

| Macro | Use |
|-------|-----|
| `ERR_PRINT(msg)` | Non-fatal, user-relevant failure (file open failed) — `src/factory.cpp:84` |
| `ERR_PRINT_ONCE(msg)` | Unsupported-but-continue cases — `src/slot.cpp:87` |
| `WARN_PRINT` / `WARN_PRINT_ED` | Recoverable misuse, editor-visible — `src/slot.cpp:438,442`, `src/armature_view.cpp:222,225` |

There is no `print_line` in `src/` — do not add one for diagnostics; use `ERR_PRINT` / `WARN_PRINT` so the message reaches the Godot console with the right severity.

> **Gotcha**: suppress expected failures during editor reimport so reimport does not spam errors. `get_file_data` returns an empty array without printing while `editor_reimporting` is set:
> ```cpp
> #ifdef TOOLS_ENABLED
> 	if (editor_reimporting) {
> 		// 编辑器执行重新导入时获取不到文件属于预期，因此不打印错误
> 		return {};
> 	}
> #endif // TOOLS_ENABLED
> ERR_PRINT(vformat("Open \"%s\" failed: \"%s\"", fp, UtilityFunctions::error_string(FileAccess::get_open_error())));
> ```
> — `src/factory.cpp:78-85`

## Editor-Mode Guards

Runtime event dispatch and mesh work must not run in the editor. Guard with the engine singleton early-return:

```cpp
if (Engine::get_singleton()->is_editor_hint()) {
	return;
}
```
— `src/armature.cpp:157-160` (`dispatchDBEvent`), `src/armature_view.cpp:494-497` (`dispatch_event`), `src/armature_view.cpp:282` (`_get_property_list`)

Whenever a setter triggers editor-facing side effects, notify the inspector:

```cpp
#ifdef TOOLS_ENABLED
	notify_property_list_changed();
#endif // TOOLS_ENABLED
```
— `src/armature_view.cpp:140-142,159-161`

## Known Gaps

Documented so they are not mistaken for the intended pattern — each is a latent bug, not a convention:

- `DragonBonesArmature::is_playing()` calls into the animation without a null guard, unlike its siblings (`src/armature.cpp:319-321` vs the guarded `has_animation` at `:206-208`).
- `DragonBonesEventObject`'s constructor reads `p_origin->time/type/name/data` in the initializer list before the body's `ERR_FAIL_NULL(p_origin)` runs (`src/event_object.cpp:164-169`).
- `DragonBonesArmature::set_settings` has an inner loop that increments the outer index: `for (size_t j = 0; j < slot_names.size(); ++i)` (`src/armature.cpp:649`) — this restores `sub_armatures` incorrectly.

If you fix one of these, move the entry from this section into the corresponding rule above.

## Checklist

- [ ] Chose the macro by return type and severity — not `ERR_FAIL_COND` where `_V` is needed.
- [ ] Returned a type-correct default value.
- [ ] New file/serialization APIs return `Error`, not `bool`.
- [ ] No `print_line` / `assert` added.
- [ ] Editor-only side effects guarded by `is_editor_hint()` and `TOOLS_ENABLED`.
