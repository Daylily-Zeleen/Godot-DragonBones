# 支持散图（Images 模式）导出的龙骨资源

## Goal

让插件支持 DragonBones 以 **Images（散图）模式**导出的资源：每个部件是独立 PNG 文件、
**没有** `*_tex.json` 图集描述。使此类资源能像图集模式一样被导入（生成 dbfactory）、
加载并正确渲染，且每帧各部件使用各自独立的纹理。

关联 issue：#22（散图不生成 dbfactory）、#39（散图错位，duplicate of #22）、
#40（多纹理动画部分部件消失，渲染侧相关）。

## Background（已确认事实）

### 散图导出的实际形态（以 `demo/dragonbones_demo/assets/Sheep` 为准）

```
Sheep/
├── Sheep_ske.json            # 骨架数据，顶层只有 frameRate/name/version/compatibleVersion/armature
└── Sheep_texture/            # 16 张独立 PNG，文件名 = 部件 display 名
    ├── Sheep_01_Body.png     # （部分为 mesh 槽位）
    ├── ear_lt.png
    └── ...
```

- **没有 `Sheep_tex.json`** —— 图集模式下该文件是纹理区域的唯一描述来源。
- 部件的 `display.name` / `path` 与 PNG 文件名（不含扩展名）一一对应。
- 含普通图片槽与 mesh 槽两种。
- 骨架数据含 `defaultActions: gotoAndPlay idle` 类似的自动播放动作。

### 现有插件的三处缺口

1. **导入器不识别**（issue #22）：`DragonBonesImportPlugin::try_import()`（`src/editor/dragon_bones_editor_plugin.cpp:160-193`）
   在 `<base>_tex.json` 不存在时直接返回 `{}`，散图项目永远不会生成 `.dbfactory`。
2. **工厂无散图加载路径**：`DragonBonesFactory` 只接受 `texture_atlas_json_file_list`，
   走 `parseTextureAtlasData()` 解析图集 JSON；没有"目录里的散 PNG"的入口。
3. **纹理模型假设单一图集**：`DragonBonesTextureAtlasData` 持有**一张** `display_texture`
   （整张图集），`Slot_GD::get_texture()` 返回它；UV 用 `parent`（所属图集）的
   width/height 归一化。散图需要"每个部件一张纹理"。

### 可复用的既有能力（研究结论，详见 design.md）

- 上游 `BaseFactory::_getTextureData()` 在按图集名查不到时，若图集 `autoSearch == true`
  会**遍历所有图集按纹理名查找** —— 天然支持"N 个图集 × 各 1 张纹理"的散图模型。
- 绘制数据按**每个槽位的纹理 RID** 分组批处理（`ArmatureDrawData::add_data` →
  `rebuild()` 按 texture_rid 合并 surface），渲染层不假设"全骨架共用一张图"。
- UV 计算用的是 `parent` 图集的 width/height —— 若为每张散图合成一个
  width/height = 该图尺寸的图集，现有 UV 代码无需改动。

## Requirements

- R1 散图项目能被导入器识别并生成 `.dbfactory`（issue #22）。
- R2 运行时能加载散图：每个部件使用其对应的独立 PNG 纹理，位置/尺寸正确（issue #39）。
- R3 支持 mesh 槽位（Sheep 案例中 body/head 等是 mesh）。
- R4 支持一个工厂加载多个龙骨数据、多个纹理来源（图集 + 散图混用不崩溃；
  关联 issue #40 的多纹理场景，需回归验证）。
- R5 图集模式完全不受影响（全量回归：现有 demo 资源 + CI 矩阵）。
- R6 散图模式下的显示切换（issue #62 场景）与 None 状态行为与图集模式一致。

## Acceptance Criteria

- AC1 `demo/dragonbones_demo/assets/Sheep/Sheep_ske.json` 能生成 `.dbfactory`，
  在场景中实例化 `DragonBonesArmatureView` 后正确渲染（与 DragonBones 编辑器截图一致）。
- AC2 Sheep 的 5 个动画（goat_*_anim）均可播放，无渲染报错
  （对照 issue #40 的 `array_len == 0` 报错）。
- AC3 mesh 槽位（Sheep_01_Body / Sheep_01_Hair / ear_* / leg_* 等带 FFD 的槽位）变形正确。
- AC4 图集模式回归：`demo/dragonbones_demo/assets/Dragon` 与 `demo/guy` 渲染不变，
  doctest 全绿，CI 24 job 全绿。
- AC5 用户可在检查器中配置散图目录（或导入器自动发现），无需手写任何 JSON。

## Out of Scope

- 不支持散图 + 图集**混在同一骨架**的奇异导出（DragonBones 编辑器不会产生）。
- 不修改第三方龙骨运行时（thirdparty/dragonBones）—— 已确认与上游 master 一致，
  散图所需能力（autoSearch 跨图集查找）上游已具备。
- 不处理 issue #39 中作者自行定位的 "Strip whitespace" 导出选项问题（图集模式问题）。
