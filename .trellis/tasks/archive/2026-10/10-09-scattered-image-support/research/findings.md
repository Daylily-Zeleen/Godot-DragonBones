# Research — 散图支持

调研时间：2026-10-09（与 issue #62 修复同一代码基线）

## 1. 散图导出的真实形态

样本：`demo/dragonbones_demo/assets/Sheep`（用户提供的 DragonBones 编辑器
Images 模式导出）

```
Sheep_ske.json            顶层键：frameRate/name/version/compatibleVersion/armature
Sheep_texture/            16 张独立 PNG
  ├─ Sheep_01_Body.png    （mesh 槽）
  ├─ Sheep_01_Hair.png
  ├─ ear_lt.png / ear_rt.png / eye_*.png / leg_*.png / foot_*.png / copyrights.png
```

- **没有 `*_tex.json`** —— 图集模式下纹理区域的唯一描述来源不存在。
- slot/display 的 `name`（`path` 未写，缺省 = name）与 PNG 文件名一一对应。
- 17 个槽位，其中 7 个为 `mesh`（body/head/ear×2/leg×4，带 FFD）。
- 动画：goat_sleep_idle_anim / goat_idle_anim / goat_walk_anim / goat_eat_anim /
  goat_trot_anim（5 个）。
- 帧率 30。

推论：散图模式下纹理区域无任何元数据，**区域 = 整张 PNG**，由插件合成。

## 2. 现有纹理管线（代码事实）

### 加载链（图集模式）

```
*_tex.json
 → DragonBonesFactory::load_texture_atlas_json_file_list()   src/factory.cpp:272
   → BaseFactory::parseTextureAtlasData()                    上游
     → _buildTextureAtlasData(atlas, &file_path)             src/factory.cpp:114
        imagePath = tex json 目录 / atlas->imagePath
        ERR_FAIL_COND_V(!ResourceLoader::exists(image_path))
        atlas_data->init(ResourceLoader::load(image_path))   ← 整张图集作为唯一纹理
```

- `DragonBonesTextureAtlasData`（src/texture_atlas_data.h:84）：
  `display_texture` = 整张图集；`width/height/imagePath/autoSearch/textures`。
- `DragonBonesTextureData`（同文件 :41）：`region/rotated/frame/name`。

### 槽位取纹理

`Slot_GD::get_texture()`（src/slot.cpp:44-52）：
`_textureData->getParent()` → `atlas->get_display_texture()`（图集级纹理）。

### UV

`Slot_GD::__get_uv_pt()`（src/slot.cpp:118-131）：
`uv = region 相关量 / parent->width|height` —— **依赖父图集的宽高**。
→ 散图若让每张 PNG 自成一个图集（width/height = 该图尺寸），
region = 全图时 UV 恰为 (0,0)-(1,1)，现有代码零改动。

### 跨图集查找（散图可行性的关键）

`BaseFactory::_getTextureData(textureAtlasName, textureName)`
（BaseFactory.cpp:8-30）：

1. 精确：`_textureAtlasDataMap[textureAtlasName]` 内逐图集按名找；
2. 兜底：`autoSearch == true` 的图集全遍历按名找。

`DisplayData.texture` 在构建显示列表时解析缓存
（BaseFactory.cpp:225/229/242/246 —— `_getSlotDisplay`）。
`BuildArmaturePackage::textureAtlasName` = dataName（如 "Sheep_ske"）。

→ 合成图集名只要不等于 dataName，就会走 autoSearch 按纹理名命中。

### 渲染合批（issue #40 关联）

`DragonBonesArmature::append_draw_data()`（src/armature.cpp:183-196）：
每槽位一条 `add_data(..., texture_rid, ...)` —— **纹理 RID 是每槽位的**。
`ArmatureDrawData::rebuild()`（src/armature_view.cpp:250-275）按 texture_rid
分组 surface，`indices->is_empty()` 跳过。

→ 渲染层不假设"一骨架一纹理"。issue #40 定位在旧版 `_draw()` 批处理
（issue 评论指向 329 行附近），该区域已重构为 ArmatureDrawData/SurfaceData；
多纹理需回归验证（不预设已修复）。

## 3. 导入器（issue #22 根因）

`DragonBonesImportPlugin::try_import(ske_file)`
（src/editor/dragon_bones_editor_plugin.cpp:160-193）：

- `base_path` 必须以 `_ske` 结尾，剥出 `base_path`；
- **`tex_atlas_file = base_path + "_tex.json"`，不存在 → `return {}`**
  （散图项目在此被拒绝，永远不生成 `.dbfactory`）；
- 命中则：`load_texture_atlas_json_file_list([tex_json])` +
  `load_dragon_bones_ske_file_list([ske])` → 存为 `.dbfactory`
  （`DragonBonesFactory::SAVED_EXT`）。

另有 `_reimport_dbfactory_recursively()` + 设置项
`Godot_DragonBones/auto_generate_dbfactory`：扫描全项目 json，
对不存在 `.dbfactory` 的尝试 `try_import`。

`DragonBonesFactory` 的纹理入口只有一个：
`texture_atlas_json_file_list` 属性（src/factory.cpp:421-425，
ADD_PROPERTY 带 `*.json` file hint）。

## 4. 方案比选

| 方案 | 说明 | 结论 |
|---|---|---|
| A. 内存合成"每 PNG 一个图集" | 每张 PNG 合成单图集 JSON（width/height=图尺寸，region=全图）走现有解析管线；autoSearch 兜底查找 | **采用**。零 thirdparty 改动、零模型改动、UV 零改动 |
| B. 运行时拼大图 | 把 N 张散图 blit 成一张运行时图集 | 否：引入打包算法与内存峰值；丢失"单独替换部件纹理"的散图核心价值（issue #22 的诉求） |
| C. 导入器生成合成 `_tex.json` 落盘 | 与现有格式完全兼容 | 否：tex json 是"一个图集 + N SubTexture"，表达不了 N 个独立纹理文件；生成 N 个 json 文件污染用户目录 |

## 5. 开放点（实现时定）

- 合成图集的命名规则（需保证不与 dataName 冲突，且对用户可读）。
  倾向：`scattered/<目录名>/<png_stem>`。
- 散图目录探测的兜底规则是否需要（非 `<base>_texture` 目录名）。
  倾向：先只实现目录惯例 + 兜底" ske 同目录存在与 display 同名的 png"。
- `Image::load()` 与 `ResourceLoader::load` 的选择：后者与
  `_buildTextureAtlasData` 同路径（吃 import 缓存）。
