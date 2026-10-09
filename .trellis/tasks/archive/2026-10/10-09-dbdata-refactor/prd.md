# 重构 DragonBones 数据管理：以「套」为单位的 *.dbdata 资源 + 全局单例 Factory

## Goal

把「数据源描述」与「解析/构建/缓存」两件事拆开：

- 新增 `*.dbdata`（`DragonBonesData` 资源），**一套龙骨 = 一个资源**，只描述数据源（`ske_file` + `texture_source`），不含解析与骨架构建。
- `DragonBonesFactory` 从「多套数据的 Resource」重构为**全局单例**，负责解析、构建、缓存真正的运行时数据。
- 自动导入（EditorImportPlugin / EditorPlugin）产物从 `*.dbfactory` 改为 `*.dbdata`。
- `DragonBonesArmatureView` 改用 `*.dbdata`，并桥接该资源与单例工厂。
- 删除旧 `*.dbfactory` 格式（硬切）。

## Background

现状（`src/factory.h:50`）：`class DragonBonesFactory : public Resource, private dragonBones::BaseFactory`。一个 Resource 承载**任意多套**数据，用三个扁平 `PackedStringArray`（`dragon_bones_ske_file_list` / `texture_atlas_json_file_list` / `scattered_texture_dir_list`）描述来源；缓存键是 ske JSON **内容里内嵌的 `name`**（不是文件名）。

已确认的缺陷（本次要消除）：

| 缺陷 | 证据 |
|---|---|
| **单工厂内**同名静默冲突：`loadDragonBonesData` 发现同名直接返回旧数据、**丢弃新文件**且无任何警告 | `src/factory.cpp` 的 `loadDragonBonesData()`（L92-99）；`BaseFactory::addDragonBonesData` 同名时 `DRAGONBONES_ASSERT(false, ...)` 后 return（release 下 `assert` 被 NDEBUG 剥除 → 静默 return） |
| 现实撞名案例：`demo/.../Dragon/Dragon_ske.json` 与 `Dragon_scattered/Dragon_ske.json` 内部 name 都是 `Dragon`，但骨架数据不同 | 实测 diff：`/armature/0/skin/0/slot/0/display/0/transform/skX` 13.92 vs 82.74 |
| 查询 API 用「回退到第一套」掩盖问题（查不到就取 map 首项） | `src/factory.cpp` 的 `create_armature()`（L505-509）、`get_loaded_dragon_bones_armature_name_list()`（L452-459）、`get_loaded_dragon_bones_skin_name_list()`（L462-497） |
| 散图回归多套并存：扁平 `scattered_texture_dir_list` 无归属，多套时语义模糊 | `src/factory.cpp` 的 `set_scattered_texture_dir_list()`（L434-443）自注 |

> **本次重构会把这个冲突从「潜在」变成「必然」**：现状每个 `.dbfactory` 是独立 Resource，各自私有继承一份 `BaseFactory`，因此两个同名资源天然隔离、互不冲突；一旦改为**全局单例工厂**，所有 `.dbdata` 共享同一个 `_dragonBonesDataMap`，撞名会立刻导致第二套数据被静默丢弃。这正是 R3 必须在同一改动内解决的原因。

> 已核实的**非**缺陷（不要写进设计前提）：图集模式的 ske JSON **不含**内嵌图集——`demo/.../Dragon_ske.json`、`Dragon_scattered/Dragon_ske.json` 均无 `textureAtlas` 键，`BaseFactory::parseDragonBonesData` 里那段 while 循环因此空转（其触发条件见 `thirdparty/dragonBones/parser/JSONDataParser.cpp:1950` 的 `TEXTURE_ATLAS` 分支）。

## Requirements

### R1 — 新资源 `*.dbdata`（类 `DragonBonesData`）

- 一套一个资源，只持有两个数据源字段：`ske_file`（字符串）与 `texture_source`（字符串）。
- **两种管理形态，由 `is_imported` 区分**：
  - 自动导入（`is_imported = true`）：两字段**只存文件名 / 文件夹名**，加载时在同目录解析；字段只读。
  - 用户手动创建（`is_imported = false`）：字段是用户自选的完整路径，可自由编辑。
- `texture_source` 的**形态由枚举字段决定**（用户已确认「单字段 + 模式枚举」）：
  - 模式「图集文件」→ `texture_source` 是 `*_tex.json`，检查器 hint 为 `PROPERTY_HINT_FILE` + `"*.json"`。
  - 模式「散图目录」→ `texture_source` 是 `*_texture/` 目录，检查器 hint 为 `PROPERTY_HINT_DIR`。
  - hint 通过 `_validate_property` 按枚举动态切换，保证两种形态在检查器里都能正确选择。
- 该资源**不**处理解析、不构建骨架、不持有运行时数据；只做数据源描述 + 桥接。

### R2 — 全局单例 `DragonBonesFactory`

- 从 `Resource, private dragonBones::BaseFactory` 重构为**全局单例**（符合 `BaseFactory` 头文件自述「通常只需要一个全局工厂实例」，`thirdparty/dragonBones/factory/BaseFactory.h:36/47`）。
- 只负责：解析数据、构建骨架、缓存真正的运行时数据（`DragonBonesData`/`TextureAtlasData` maps）。
- **不暴露给脚本**（用户已确认「内部单例，不暴露」）——与已有 `godot::DragonBones` 内部单例同一层级，只有 View 与 `.dbdata` 桥接它。因此不再 `GDREGISTER_CLASS`、不再继承 `Resource`。

### R3 — 缓存命名防冲突（用户第 5 点）

- `.dbdata` 在单例工厂中的缓存名必须**唯一且可复现**，不能直接用 ske JSON 内嵌 name（现状即因此静默丢弃数据）。
- 期望：唯一键由资源自身身份推导（`.dbdata` 的 `res://` 路径或其 UID），使 `Dragon/` 与 `Dragon_scattered/` 两个同名 `Dragon` 可共存且各自正确。

### R4 — 自动导入改产 `*.dbdata`

- EditorImportPlugin / EditorPlugin 的自动生成产物从 `*.dbfactory` 改为 `*.dbdata`，`is_imported = true`。
- 识别规则沿用：`*_ske.json` / `*_ske.dbbin` + 存在 `*_tex.json`（图集）或 `*_texture/` 目录（散图）。
- **删除 `get_all_imported_factories` 及编辑器「文件移动跟随」逻辑**：自动导入形态的资源与数据同目录，用户整体移动目录即可；用户手动创建的资源由用户自管。
- 项目设置 `Godot_DragonBones/auto_generate_dbfactory` **改名**（破坏性变更，需在 PR 显式说明）。

### R5 — `DragonBonesArmatureView` 改用 `*.dbdata`

- `factory` 属性（`Ref<DragonBonesFactory>`）替换为 `Ref<DragonBonesData>` 资源。
- **移除 `instantiate_dragon_bones_data_name`**（单套资源下「选哪套数据」由资源本身决定）。
- View 通过该资源向单例工厂请求构建与卸载。
- **不引入额外引用计数**：在 `~DragonBonesData()` 里通知工厂卸载该套即可（`Resource` 的 Ref 计数已足够）。

### R6 — 硬切旧格式（用户已确认）

- 删除 `.dbfactory` 的 FileProcessor 与相关常量；旧工程需重新导入生成 `.dbdata`。
- **但 `.dbdata` 仍需自己的 loader/saver**（引擎不注册就无法使用该扩展名，见 design §3.5）；格式沿用现有 ConfigFile 文本，字段精简为 `ske_file` / `texture_source_type` / `texture_source` / `imported`。
- 打包位置**维持现状**（`convert_to_imported_path()` 的隐藏路径）；**不做压缩**（实测收益不划算，见 design §6.4）。

## Constraints

- 仓库现有约定必须遵守（`.trellis/spec/`）：类绑定与注册级别（`class-binding.md`）、目录结构（`directory-structure.md`）、错误处理（`error-handling.md`）、内存与生命周期（`memory-and-lifetime.md`）、类型转换（`type-conversion.md`）、质量约定（`quality-guidelines.md`）、编辑器插件门控（`editor-plugin.md`）。
- 支持 Godot 4.3 → current 全范围（`compatibility_minimum = 4.3`）。`PROPERTY_HINT_FILE_PATH` 仅 4.5+ 存在且在 4.4/4.3 的 API 里查不到，**不可用于实现 `texture_source`**。
- 命名冲突：`.dbdata` 的类名为 `DragonBonesData`，与 `dragonBones::DragonBonesData` 同名（用户已确认该命名）。在同时 `using namespace godot` 与 `using namespace dragonBones` 的翻译单元中，用两个别名区分（用户指定：`GDDragonBonesData` / `DBDragonBonesData`）。
- 编辑器代码必须在 `TOOLS_ENABLED` 下双向门控；`template_release` 必须仍能编译。
- `demo/` 下的改动不提交（除非用户自行提交）。

## Acceptance Criteria

- [ ] 新建 `*.dbdata` 资源可在检查器中分别选择「图集文件（`*_tex.json`）」与「散图目录（`*_texture/`）」，两种模式 hint 均正确。
- [ ] `Dragon/`（图集）与 `Dragon_scattered/`（散图）两个同内部 name 为 `Dragon` 但数据不同的资源**可同时加载且各自渲染正确**，无静默丢数据、无 `Can not add same name data` 断言。
- [ ] **自动导入形态**的资源把自身与源文件一起移动到新目录后，仍能正确加载（字段只存名字，同目录解析）。
- [ ] **用户手动创建**的资源可选择任意路径的 ske / 图集 json / 散图目录并正确加载。
- [ ] 打包后的游戏能正确加载 `.dbdata` 及其数据文件（导出链路验证）。
- [ ] `DragonBonesFactory` 变为全局单例后，`DragonBonesArmatureView` 仍能通过 `*.dbdata` 正常实例化骨架；demo 场景可用。
- [ ] 自动导入开启时，识别到龙骨资源会生成 `*.dbdata`（不再生成 `*.dbfactory`）。
- [ ] 旧 `*.dbfactory` 的 saver/loader/常量已彻底移除，无死代码残留。
- [ ] `template_debug` 与 `template_release` 均编译通过；Web 构建（linkage 敏感）无重复符号。
- [ ] demo 冒烟 + 图集/散图两条路径实测渲染正确；既有 doctest 全绿。
- [ ] `doc_classes/` 与 README（中英成对）同步更新。

## Out of Scope

- 不做「一套资源含多套数据」的聚合形态（本次明确以「套」为最小单位）。
- 不改动 DragonBones 运行时（`thirdparty/`）的解析/构建语义，除非为实现 R3 的缓存键所必需（届时需单独说明并说明其副作用）。
- 不引入新的自动测试框架；验证以编译 + demo 实跑 + 既有 doctest 为准。

## Notes

- 用户原话要点：`*.dbdata` 本身不处理数据解析/骨架构建，它被 `DragonBonesArmatureView` 使用，取代 `DragonBonesFactory` 资源的位置，并桥接 View 与重构后的单例工厂。
- 需要重点确认的兼容面：`.dbdata` 资源在**导出的游戏包**里如何被加载（现有 `DragonBonesExportPlugin` 会把 ske/atlas 源文件以 md5 隐藏路径打入 pck，`src/editor/dragon_bones_editor_plugin.cpp` 的 `DragonBonesExportPlugin::_export_file()`（L55-79））——该链路必须随之迁移，否则导出后运行期取不到源文件。
