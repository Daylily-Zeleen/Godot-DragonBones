# Implement — 散图（Images 模式）支持

前置：`prd.md`（需求/验收）、`design.md`（方案已定，含风险决策）。
分支约定：从 master 拉特性分支 `feature/scattered-image-support`，PR 目标 master。
构建：Windows `scons -j1`（MSVC 偶发 C1060）；clang-cl 的 CMake 构建用于 clangd。
临时文件一律放 `.agent_tmp/`。

## 步骤 0：基线（已完成的研究）

- [x] 确认散图导出形态（`demo/dragonbones_demo/assets/Sheep`：无 `_tex.json`，
      `Sheep_texture/` 下 16 张 PNG，文件名 = display 名，含 mesh 槽）
- [x] 确认纹理模型与查找路径（见 design.md §1）：
      `autoSearch` 跨图集查找为上游既有能力；渲染按 texture_rid 分 surface
- [x] 确认导入器缺口：`try_import()` 无 `_tex.json` 即放弃（issue #22 根因）

## 步骤 1：运行时 — 散图加载（design.md §2.1）

- [x] 1.1 `DragonBonesFactory` 新增：（实现方式按评审意见改为"遍历 DisplayData +
      模型层直接组装"，见 design.md §2.1 修订版；不再合成 JSON / 调 loadTextureAtlasData）
  - `PackedStringArray scattered_texture_dir_list` 属性（`set/get`，绑定检查器，
    dir hint；序列化进 `.dbfactory` 由现有 ResourceSaver 承担）
  - `Error load_scattered_texture_dir_list(PackedStringArray p_dirs)`
- [x] 1.2 实现"每 PNG 合成单图集 JSON → `loadTextureAtlasData`"管线：
  - 图集名 = `<dataName>/<png_stem>` 形态（防与 dataName 冲突）或按调用上下文
    无 dataName 时用 `scattered/<dir_name>/<png_stem>`（实现时定，保持可复现）
  - `imagePath` = PNG 绝对路径（`_buildTextureAtlasData` 用 `ResourceLoader` 加载）
  - `width/height` 用加载后 `Texture2D->get_size()`
  - region = 全图；`name` = png stem（与 display path 对应）
- [x] 1.3 去重/重载语义与 `load_texture_atlas_json_file_list` 对齐
  （先解除 DragonBonesData 对旧图集的占用再 `removeTextureAtlasData`）
- [x] 1.4 验证：内存中 `getAllTextureAtlasData()` 含全部合成图集；
  `_getTextureData("不存在的dataName", "ear_lt")` 能经 autoSearch 命中

## 步骤 1b：评审修正 — 散图按数据名（套）管理（design.md §2.1b）

> 评审结论：初版实现存在 P1（散图加载器内嵌图集全量重载的隐藏耦合）与
> P2（显示路径/图集注册工厂级聚合，多套资源必然互相污染 + 必然报错）。
> 按套管理的大重构另立任务；本步先把归属收敛到数据名粒度。

- [ ] 1b.1 废弃 `load_scattered_texture_dir_list(PackedStringArray)`，
      新增 `set_scattered_texture_dirs(data_name, dirs)`（按套收集显示路径）
- [ ] 1b.2 选择性清理：`_scattered_atlases[data_name]` 记录每套图集指针，
      重设时解除引用 + 从同名向量移除 + returnToPool；图集模式条目不动
- [ ] 1b.3 图集只注册到该 `data_name` 名下
- [ ] 1b.4 缺失纹理：汇总一次性 ERR_PRINT（data_name + path + 目录），返回
      ERR_PARSE_ERROR；删除逐条 ERR_CONTINUE 毒化 err 的写法
- [ ] 1b.5 `.dbfactory` 的 `scattered_texture_dirs` 改为 Dictionary
      （data_name → dirs）；`try_import` 在 ske 加载后取数据名再调用
- [ ] 1b.6 验证：Sheep 单套回归 + Dragon_scattered/Sheep 双套共存互不污染 +
      缺文件场景报错定位 + 图集模式回归 + doctest

## 步骤 2：导入器 — 识别散图（design.md §2.2，issue #22）

- [x] 2.1 `try_import()`：`_tex.json` 不存在时做散图探测
  （`<base>_texture/` 目录含 png；兜底按 ske display 名匹配同目录 png）
- [x] 2.2 命中后设置 dbfactory 的 `scattered_texture_dir_list` 并加载 ske
- [x] 2.3 `_reimport_dbfactory_recursively` 兼容（`auto_generate_dbfactory` 路径）

## 步骤 3：集成验证（PRD AC1-AC3）

- [x] 3.1 `demo/dragonbones_demo/assets/Sheep/Sheep_ske.json` 生成 `.dbfactory`
- [x] 3.2 场景实例化渲染正确（对照 DragonBones 编辑器截图）；5 个 `goat_*_anim` 轮播
- [x] 3.3 mesh 槽位（Sheep_01_Body 等）FFD 变形正确
- [x] 3.4 无 `array_len == 0` 类渲染报错（对照 issue #40）

## 步骤 4：回归（PRD AC4-AC5 / R5-R6）

- [x] 4.1 图集模式回归：Dragon / guy 渲染不变；issue #62 None 切换不回归
- [x] 4.2 doctest 全绿（`tests=yes`）；若有纯逻辑可测（合成 JSON 的名字/region）补单测
- [ ] 4.3 CI 24 job 全绿（PR 上验证）
- [x] 4.4 clang-format 触碰过的文件（`.trellis/spec/gdextension/quality-guidelines.md`）

## 步骤 5：收尾

- [ ] 5.1 README.md / README.zh.md 增补"散图模式"支持说明（两份保持成对）
- [ ] 5.2 `.trellis/spec/build/build-systems.md` / `editor/editor-plugin.md`
      补充散图导入路径的约定（若引入新模式）
- [ ] 5.3 PR 描述（中文）+ 关联 issue：`Closes #22`，`Fixes #39`（#40 若验证已修复则一并 Close）

## 风险与回滚

- 全部新逻辑为**新增入口**，图集模式路径不动 → 回滚 = 还原 commit。
- 若 autoSearch 兜底与精确查找冲突（图集名设计缺陷），在 1.2 调整图集名规则即可，
  不触及渲染。

## 提交约定

- commit/PR 中文（conventional 前缀保留）；每完成一步停下等用户确认，
  除非用户明确要求一次性做完（AGENTS.md 授权规则）。
