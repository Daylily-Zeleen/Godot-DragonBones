# Editor Plugin Layer Guidelines

> 本文件记述 `src/editor/` 的编码约定：编辑器插件、导入/导出插件、TOOLS_ENABLED 门控。

**Applies to**: `src/editor/dragon_bones_editor_plugin.h/.cpp`, plus the `TOOLS_ENABLED` blocks inside `src/factory.*`, `src/armature.*`, `src/armature_view.*`, and the EDITOR registration block in `src/dragon_bones_registration.cpp`.

---

## Pre-Development Checklist

- [ ] Read [editor-plugin.md](./editor-plugin.md) — class roles and the import/export pipeline contract.
- [ ] Confirmed the code is genuinely editor-only; if it must run in an exported game, it does **not** belong in this layer.
- [ ] Wrapped every new declaration and definition in `#ifdef TOOLS_ENABLED` with a matching `#endif // TOOLS_ENABLED`.
- [ ] Checked the change also builds for `template_debug` **with** tools and for `template_release` **without** them (see [../build/ci-and-release.md](../build/ci-and-release.md)).

## Quality Check

- [ ] Editor symbols are unreachable in a release build — verify by building `target=template_release` (there `env.debug_features` is false, so `TOOLS_ENABLED` and `src/editor/*.cpp` are excluded; `SConstruct:94-96`).
- [ ] Registration and de-registration are symmetric: every `add_by_type<X>()` has a matching `remove_by_type<X>()` (`src/dragon_bones_registration.cpp:61` vs `:91`).
- [ ] No editor plugin dereferences a pointer returned by an import/parse call without checking it (see Known Defects).
- [ ] Resource paths written by the import plugin remain stable across reimport — see the path contract below.

---

## Topic Files

| Guide | Description |
|-------|-------------|
| [Editor Plugin](./editor-plugin.md) | Class roles, TOOLS_ENABLED gating, import/export contract, resource path rules |
