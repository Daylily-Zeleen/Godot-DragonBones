# Thinking Guides

> **用途**：写代码前先想清楚，避免"没想到"造成的 bug 与技术债。

---

## Why Thinking Guides

**Most bugs come from "didn't think of that"**, not from lack of skill. In this repository the recurring categories are:

- Forgetting a second build system (`SConstruct` vs `CMakeLists.txt`) or a second platform (`Web` behaves differently).
- Forgetting that a raw pointer's owner is the vendored runtime, not this plugin.
- Forgetting that `doc_classes/*.xml`, `_bind_methods`, and the header form one contract.
- Assuming a change is local when it reaches across the Godot ↔ DragonBones boundary.

Guides here help you **ask the right questions before coding**. They are checklists, not code specs — concrete rules live in the layer directories.

---

## Available Guides

| Guide | Purpose | When to Use |
|-------|---------|-------------|
| [Code Reuse Thinking Guide](./code-reuse-thinking-guide.md) | Find existing wrappers before adding new code | Before adding any method, helper, or constant |
| [Cross-Layer Thinking Guide](./cross-layer-thinking-guide.md) | Trace data and ownership across the Godot ↔ runtime boundary | Anything touching both sides, or more than one platform |

---

## Quick Reference: Thinking Triggers

### When to Think About Cross-Layer Issues

- [ ] The change touches both a `DragonBones*` wrapper and a `dragonBones::*` runtime object
- [ ] Data changes representation at the boundary (String↔std::string, Godot math ↔ runtime math)
- [ ] You are adding a field to a serialized structure (`.dbfactory`, `armature_settings`, `sub_armatures`)
- [ ] You are adding or renaming a bound method/property/signal/enum
- [ ] The change touches source globbing, linkage, or a widely-included header
- [ ] You are about to add a `#ifdef` for a platform

→ Read [Cross-Layer Thinking Guide](./cross-layer-thinking-guide.md)

### When to Think About Code Reuse

- [ ] You are about to write a null guard that `DragonBonesArmatureView` already has ~40 of
- [ ] You are about to hand-write a conversion `to_gd_str`/`to_std_str` already does
- [ ] You are about to write a literal `StringName`, extension string, or path fragment
- [ ] You notice the same pattern in a third file
- [ ] **You are modifying any constant, setting name, or file extension**

→ Read [Code Reuse Thinking Guide](./code-reuse-thinking-guide.md)

### When Verifying AI Cross-Review Results

AI reviewers produce false positives on this codebase in predictable ways:

1. **Trust-boundary confusion** — treating `doc_classes/*.xml` or `thirdparty/` content as project code to "fix". The vendored runtime is not ours to refactor.
2. **Ignoring stated intent** — `hasDBEventListener` always returning `true` and the no-op listener methods are deliberate stubs (`src/armature.h:90-94`, `src/dragon_bones.h:52-57`), not oversights.
3. **Platform-blindness** — a claim that "the build passes" when only the host platform was built. Web differs (see the duplicate-symbol case).
4. **Citation drift** — line numbers from an older revision. Re-read the cited range before acting on any finding.

**Verification rule**: every CRITICAL/WARNING finding must be re-checked against the current source before it is acted on.

---

## Pre-Modification Rule (CRITICAL)

> **Before changing ANY value, search first.**

```bash
# the setting name
grep -rn "auto_generate_dbfactory" .

# the extension literals
grep -rn '"dbfactory"\|SRC_JSON_EXT\|SRC_BIN_EXT' .

# the library name
grep -rn "libgddragonbones" .

# the addon directory
grep -rn "godot_dragon_bones.daylily-zeleen" . --include=*.py --include=*.sh --include=*.yml --include=SConstruct --include=*.gdextension
```

This habit prevents the "forgot to update the other place" class of bug, which in this repo has three known surfaces: the second build system, `generate_xcframework.sh`, and the `.gdextension` manifest.

---

## How to Use This Directory

1. **Before coding**: skim the relevant guide.
2. **During coding**: if something feels repetitive or crosses a boundary, check both guides.
3. **After a bug**: add the insight to the relevant guide — and if it is a concrete rule, add it to the matching layer spec instead.

---

**Core Principle**: 30 minutes of thinking saves 3 hours of debugging.
