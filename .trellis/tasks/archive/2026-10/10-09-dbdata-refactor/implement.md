# 执行计划

**前置**：`design.md` §8 的 10 项决策已全部确认，§9 列出两项暂不处理的已知风险。可直接进入实现。

## 阶段 0：准备与基线

- [ ] 0.1 确认 `design.md` §8 全部开放点，把结论写回 `design.md`。
- [ ] 0.2 记录基线：`scons ... -j1` 全绿；`-- --auto-exit` 冒烟 0 错误；doctest 122/122（`tests=yes`）。
- [ ] 0.3 记录当前 demo 图集（`Dragon`）与散图（`Sheep`）的渲染基线截图，供改造后逐像素回归对比。

**验证**：基线数据落盘到 `.agent_tmp/`（不入库）。

## 阶段 1：`DragonBonesData` 资源（新文件）

- [ ] 1.1 新建 `src/db_data.h` / `src/db_data.cpp`（遵守 `directory-structure.md`：33 行 MIT 头、`#pragma once`、`<godot_dragon_bones.h>` 前置、`namespace godot`）。
- [ ] 1.2 定义 `GDDragonBonesData` / `DBDragonBonesData` 别名（位置与写法见 `design.md` §2）。
- [ ] 1.3 声明类：`GDCLASS(DragonBonesData, Resource)`、`enum TextureSourceType`（公开）+ `VARIANT_ENUM_CAST`（放在 namespace 闭合外）。
- [ ] 1.4 字段与 setter/getter：`ske_file`、`texture_source_type`、`texture_source`、`is_imported`；`texture_source_type` 的 setter 里 `#ifdef TOOLS_ENABLED notify_property_list_changed();`。
- [ ] 1.4b 两种形态：`is_imported == true` 时字段只读（`_validate_property`），且解析走「资源同目录 + 名字」；`false` 时字段可编辑，解析直接用完整路径。实现 `resolve_ske_path()` / `resolve_texture_source_path()`（design §3.3）。
- [ ] 1.5 `_validate_property`：按 `texture_source_type` 切换 `texture_source` 的 hint（`PROPERTY_HINT_FILE` + `"*.json"` / `PROPERTY_HINT_DIR`）。
- [ ] 1.6 `_bind_methods`：`bind_method` + `ADD_PROPERTY`（用显式 `PropertyInfo`，见 `class-binding.md`）+ `BIND_ENUM_CONSTANT`。
- [ ] 1.7 注册：`src/dragon_bones_registration.cpp` 的 SCENE 层加 `GDREGISTER_CLASS(DragonBonesData)`。
- [ ] 1.8 `doc_classes/DragonBonesData.xml` 新建。

**验证**：编译通过；`.gdextension` 加图标（可选）；编辑器里新建资源、两种模式各选一次文件/目录，确认对话框模式正确。

## 阶段 2：单例工厂

- [ ] 2.1 把 `DragonBonesFactory` 从 `Resource, private dragonBones::BaseFactory` 改为内部单例（不暴露、不注册）。保留 `private dragonBones::BaseFactory` 继承（`_build*` 虚函数仍要覆盖）。
- [ ] 2.2 加 `static DragonBonesFactory *get_singleton()`；在 `dragon_bones_registration.cpp` 的 SCENE init 创建、uninit 销毁。
- [ ] 2.3 **销毁顺序**：工厂先于 `BaseObject::clearPool()`（`src/dragon_bones_registration.cpp:105`），见 `design.md` §7。
- [ ] 2.4 删除三个扁平列表属性及其 setter（`set_dragon_bones_ske_file_list` / `set_texture_atlas_json_file_list` / `set_scattered_texture_dir_list`，`src/factory.cpp:320-328,434-443`、`_bind_methods` 对应行 L530-546）。
- [ ] 2.5 实现 `ensure_data_loaded(Ref<DragonBonesData>)` / `release_data(cache_name)`（`design.md` §4.4）；**不做引用计数**，由 `~DragonBonesData()` 调 `release_data`。
- [ ] 2.6 缓存键改为 `design.md` §4.2 决定的方案；**三处注册键统一**（骨架 / 图集 / 散图图集，见 §4.3）。
- [ ] 2.7 `create_armature` 改为按 `cache_name` 取数据，去掉「回退第一套」的兜底（`src/factory.cpp:505-509` 等）。
- [ ] 2.8 保留 `get_imported_file_name` / `convert_to_imported_path`（归属按 §8.6 结论）。
- [ ] 2.9 卸载该套时 `unref()` 其持有的 `Ref<Texture2D>`（`src/texture_atlas_data.h`）。

**验证**：编译；工厂单例创建/销毁无泄漏（运行期无 `ERROR`/`WARNING`）。

## 阶段 3：View 改造

- [ ] 3.1 `src/armature_view.h/.cpp`：`Ref<DragonBonesFactory> factory` → 新属性（按 §8.5 命名），`ADD_PROPERTY` 的 `PROPERTY_HINT_RESOURCE_TYPE` 改为新类名。
- [ ] 3.2 `rebuild_armature()`：先 `ensure_data_loaded`，再构建；`data_name` 参数换成 `cache_name`。
- [ ] 3.3 `_validate_property`：armature/skin 候选列表改为从单例按 `cache_name` 查。
- [ ] 3.4 析构里 `release_data`。
- [ ] 3.5 **移除** `instantiate_dragon_bones_data_name`，连带 `doc_classes/DragonBonesArmatureView.xml`、demo 场景/脚本。
- [ ] 3.6 `set_armature_settings` 里对 `factory->is_imported()` 的判断（`src/armature_view.cpp:578`）改为新资源。
- [ ] 3.7 实现 `~DragonBonesData()` → `factory->release_data(cache_name)`；**不加**防御代码应对工厂先析构的情形（design §9 已记录）。

**验证**：demo 场景（`Dragon`，图集模式）能加载并播放。

## 阶段 4：编辑器层

- [ ] 4.1 `try_import` 改为产出 `DragonBonesData`（`src/editor/dragon_bones_editor_plugin.cpp:156-204`）。
- [ ] 4.2 `_get_save_extension` / `_get_resource_type` 改 `dbdata` / 新类名。
- [ ] 4.3 **删除** `get_all_imported_factories`（`src/factory.h:133-136`）及编辑器「文件移动跟随」逻辑（`_reimport_moved_factory_files`、`_on_file_system_dock_files_moved`）——新方案下不需要（design §8 #2）。
- [ ] 4.4 项目设置 `auto_generate_dbfactory` **改名**（破坏性变更，PR 中显式说明）；`_reimport_dbfactory_recursively` 相应改名与改写。
- [ ] 4.5 **导出插件** `_export_file` 改按 `.dbdata` 判类型并打进 ske + 图集 json / 散图目录下全部 PNG（`design.md` §6.2）——漏改只在导出后暴露。
- [ ] 4.6 删除 `.dbfactory` 的 FileProcessor / Saver / Loader 与注册行（`design.md` §6.3）。
- [ ] 4.7 新增 `ResourceFormatLoaderDragonBonesData` / `ResourceFormatSaverDragonBonesData`（cfg 格式，字段精简），替换注册行（design §6.3）。
- [ ] 4.8 **不做**压缩；打包位置维持 `.godot|godot/imported/<md5>.dbimport`（design §6.4、§8 #9/#10）。

**验证**：开启 `auto_generate_dbdata` 后，改动/新增龙骨文件能在同目录生成 `.dbdata`；移动资源后重新导入正确。

## 阶段 5：删除与清理

- [ ] 5.1 删除 `SAVED_EXT` 等 `.dbfactory` 常量；全仓库 `grep -rn "dbfactory"` 确认无残留（含 `SConstruct`、`CMakeLists.txt`、`.github/`、`generate_xcframework.sh`）。
- [ ] 5.2 `doc_classes/DragonBonesFactory.xml` 删除或改写（工厂不再是 Resource、不暴露）。
- [ ] 5.3 两个 build 文件同步（`SConstruct` 与 `CMakeLists.txt`——若新增了源文件，两处都要加）。
- [ ] 5.4 README.md / README.zh.md 成对更新（`.dbfactory` → `.dbdata`、散图说明、项目设置名）。

**验证**：`grep -rn "dbfactory\|DragonBonesFactoryFileProcessor"` 只剩历史文档说明。

## 阶段 6：全量验证（回归门）

- [ ] 6.1 `scons -j1 target=template_debug` 与 `target=template_release`（后者 `TOOLS_ENABLED` 关闭，验证编辑器代码无泄漏引用）。
- [ ] 6.2 Web 构建（linkage 敏感，唯一能暴露重复符号的平台）。
- [ ] 6.3 **同名冲突实测**：`Dragon/`（图集）与 `Dragon_scattered/`（散图）同时存在并被引用，两者渲染均正确、无 `ERROR`、无静默丢数据。
- [ ] 6.4 demo `-- --auto-exit` 冒烟 0 错误；与阶段 0.3 基线截图逐像素对比。
- [ ] 6.5 doctest 全绿。
- [ ] 6.6 导出 PCK 后运行（验证 4.5 的隐藏路径打包链路）。

**回滚点**：每个阶段结束是一个 commit 边界；若阶段 2 的缓存键方案验证失败，回滚到阶段 1 后重选 §4.2 的另一种方案。

## 风险清单

| 风险 | 触发条件 | 缓解 |
|---|---|---|
| 缓存键漏改一处 → 纹理空白 | 骨架与图集注册键不一致 | 阶段 2.6 强制三处统一；6.3 专项实测 |
| 单例销毁顺序错误 → 二次释放 | 工厂晚于 `clearPool()` 析构 | 阶段 2.3；6.1 release 构建验证 |
| 导出链路漏改 → 只在导出后暴露 | 只跑编辑器内测试 | 阶段 4.5 + 6.6 |
| `.dbdata` 析构晚于单例 → use-after-free | 退出时销毁顺序不确定 | **已知风险，当前不处理**（design §9），实现后留意退出报错 |
| 项目设置改名破坏旧工程 | 直接改名 | §8.3 决策；若改名需写迁移 |
| 4.3 兼容性 | 用了 4.5+ 的 API | `PROPERTY_HINT_FILE_PATH` 明确不用；`PROPERTY_HINT_FILE/DIR` 全版本可用 |
