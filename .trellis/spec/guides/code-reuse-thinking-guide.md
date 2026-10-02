# Code Reuse Thinking Guide

> **用途**：写新代码前先搜索，避免重复实现、常量漂移与契约分叉。

---

## The Problem

**Duplicated code is the #1 source of inconsistency bugs.** In this repository duplication shows up in four concrete ways, each with a known cost:

| Duplicated thing | Cost | Where |
|------------------|------|-------|
| Source list / exclusion rules | Build works in one system, fails in the other | `SConstruct` vs `CMakeLists.txt` |
| Null-guard forwarding | ~40 near-identical methods to keep in sync | `src/armature_view.cpp:659-804` |
| Path/extension literals | Rename misses a copy, import breaks | `src/factory.h`, `generate_xcframework.sh`, `SConstruct` |
| Binding + doc + header triples | API silently undocumented or renamed in one place only | `_bind_methods`, `doc_classes/`, class headers |

---

## Step 1: Search First

```bash
# a method you plan to add
grep -rn "has_animation\|get_animations" src/

# a constant / setting / extension
grep -rn "SRC_JSON_EXT\|SAVED_EXT\|auto_generate_dbfactory" .

# a helper that might already exist
grep -rn "to_gd_str\|to_std_str\|to_gd_transform\|to_db_transform" src/

# a guard pattern that may already be established
grep -rn "ERR_FAIL_NULL_V(armature" src/
```

## Step 2: Ask These Questions

| Question | If yes |
|----------|--------|
| Does an equivalent wrapper method already exist on `DragonBonesArmature`? | Forward from the View, don't reimplement |
| Is this conversion already a helper? | Use `to_gd_str` / `to_std_str` / the file-local transform helpers |
| Is this literal defined as a constant? | Use the constant (`src/factory.h:76-79`) |
| Would this rule need to be added to a second build file? | Add it to both, in the same change |
| Is this the same null-guard I wrote two files ago? | Follow the existing guard shape; do not invent a third style |

---

## Common Duplication Patterns

### Pattern 1: The View Forwards, It Does Not Reimplement

`DragonBonesArmatureView` exposes the armature API to scene scripts by forwarding to its owned `DragonBonesArmature`, guarding the pointer each time:

```cpp
bool DragonBonesArmatureView::has_animation(const String &p_animation_name) const {
	ERR_FAIL_NULL_V(armature, false);
	return armature->has_animation(p_animation_name);
}
```
— `src/armature_view.cpp:659-662`

**Rule**: new armature functionality that scene scripts need is added to `DragonBonesArmature` **and** forwarded from the View. Never implement animation/IK/slot logic in the View — it owns no runtime state.

### Pattern 2: One Owner per Constant

Extensions live only in `DragonBonesFactory`:

```cpp
static constexpr char SRC_JSON_EXT[] = "json";
static constexpr char SRC_BIN_EXT[] = "dbbin";
static constexpr char SAVED_EXT[] = "dbfactory";
```
— `src/factory.h:76-79`

**Bad**: re-typing `".dbfactory"` in the saver, the import plugin, and `generate_xcframework.sh`.
**Good**: reference the constant in C++; where a shell/CI file needs the literal, it is a documented boundary — search for it before renaming.

### Pattern 3: One Build List, Two Build Systems

Both `SConstruct` and `CMakeLists.txt` enumerate sources and exclude `thirdparty/godot-cpp`. Adding a source directory to one only is a latent CI failure.

**Rule**: any change to source globbing, exclusions, or include paths is applied to **both** files in the same commit.

### Pattern 4: Binding Triples

A bound API is defined in three places that must agree:

1. the C++ declaration (`src/armature.h:160-207`)
2. `_bind_methods` — `ClassDB::bind_method(D_METHOD(...))` and `ADD_PROPERTY` (`src/armature.cpp:65-124`)
3. `doc_classes/<Class>.xml` — `<method>` entries and `setter`/`getter` attributes

**Rule**: if the same method name appears in 2 of these 3 places, update the third before finishing.

---

## When to Abstract

**Abstract when**:
- The same logic appears 3+ times (e.g. a conversion used across files → promote the file-local helper into `src/godot_dragon_bones.h`)
- The logic is subtle enough to have its own bug class (type-index macros, allocator bridges)
- A second file genuinely needs it

**Don't abstract when**:
- It is used once
- It is a trivial one-liner
- It would hide an ownership decision (`memnew`/`memdelete` should stay visible at the call site — an ownership-hiding macro is how leaks start)
- The "duplication" is the null guard on a forwarding wrapper; that shape is intentional and greppable

## After Batch Modifications

When you change something that appears in multiple places:

1. **Search**: re-run the grep for the old value across `src/`, `.github/`, `SConstruct`, `CMakeLists.txt`, `generate_xcframework.sh`, and the demo project.
2. **Check the generated side**: `.gdextension` version and `gen/` are build-generated — do not "fix" them by hand.
3. **Check the docs side**: `doc_classes/` for any renamed binding.
4. **Check the second platform**: Web for anything touching linkage.

## Checklist Before Commit

- [ ] Searched for existing similar code before writing new code
- [ ] No `DragonBonesArmature` logic reimplemented in the View
- [ ] Constants referenced, not re-typed
- [ ] Glob/exclusion changes applied to both build systems
- [ ] Binding changes reflected in `_bind_methods` and `doc_classes/`
- [ ] No hand-edits to generated files (`gen/`, `.gdextension` version)
