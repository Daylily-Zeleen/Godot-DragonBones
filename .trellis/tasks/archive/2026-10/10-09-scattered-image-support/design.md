# Design — 散图（Images 模式）支持

## 1. 现状架构（研究结论）

### 纹理模型

```
DragonBonesFactory
 └─ _textureAtlasDataMap[dataName] = vector<TextureAtlasData*>     （上游字段）
     └─ DragonBonesTextureAtlasData（src/texture_atlas_data.h）
         ├─ display_texture : Ref<Texture2D>     ← 整张图集 PNG（唯一纹理）
         ├─ imagePath / width / height / autoSearch
         └─ textures[name] → DragonBonesTextureData（SubTexture：region/rotated/frame）
```

- 加载：`load_texture_atlas_json_file_list()` → `parseTextureAtlasData()`（上游，
  解析 `*_tex.json`）→ `_buildTextureAtlasData(atlas, &file_path)`（`src/factory.cpp:114`）
  按 `imagePath` 相对 tex json 目录加载整张图集并 `atlas_data->init(texture)`。
- 槽位取纹理：`Slot_GD::get_texture()`（`src/slot.cpp:44`）= `_textureData->getParent()`
  → `atlas->get_display_texture()` —— **图集级**纹理。
- UV：`__get_uv_pt()` 用 `region` 与**父图集**的 width/height 归一化（`src/slot.cpp:118-131`）。
- 绘制收集：`DragonBonesArmature::append_draw_data()` → 每槽位一条
  `add_data(zOrder, transform, verts, indices, colors, uvs, texture_rid, blend_mode)`。
- 渲染合批：`ArmatureDrawData::rebuild()`（`src/armature_view.cpp:250-275`）按
  `texture_rid` 分组 surface —— **渲染层本就支持一骨架多纹理**。

### 纹理查找（实测修正，实现依据）

`BaseFactory::_getTextureData(textureAtlasName, textureName)`（`BaseFactory.cpp:8-30`）：

1. 先按 `textureAtlasName` 精确查找 —— `_textureAtlasDataMap[name]` 是 **vector**，
   同名可挂多个图集，命中后**逐图集按纹理名查找**；
2. `autoSearch` 兜底：`TextureAtlasData::autoSearch` 默认 false 且解析器从不设置
   （实测 `grep autoSearch thirdparty/dragonBones/parser/` 无结果），**该路径不会触发**。

`DisplayData.texture` 在构建显示列表时解析并缓存（`BaseFactory.cpp:225/229/242/246`，
`_getSlotDisplay`），随后 `Slot_GD::_updateFrame()` 读 `_textureData`。

> 实测推翻了最初"依赖 autoSearch 兜底"的设想。实际方案：把散图图集注册到
> **已加载的龙骨数据名**下（`_textureAtlasDataMap[dataName]` 的 vector 追加），
> 精确查找即按纹理名逐图集命中。散图不改变 "None"（displayIndex=-1）的语义，
> issue #62 的修复仍成立。

### 散图导出的形态（`demo/dragonbones_demo/assets/Sheep`）

- 无 `*_tex.json`；`<base>_texture/` 目录下 N 张 PNG，文件名 = display 名。
- 每张 PNG 即完整纹理（无 trim/旋转；`Strip whitespace` 不影响散图）。
- 骨架数据的 display 只有 `name`/`path`（= PNG 文件名），无纹理区域信息 →
  区域必须由插件合成：**region = 整张图 (0,0,W,H)**。

## 2. 方案

### 2.1 运行时：遍历 DisplayData + 模型层直接组装（最终实现）

`DragonBonesFactory` 新增散图加载入口：

```cpp
Error load_scattered_texture_dir_list(PackedStringArray p_dirs);   // 绑定检查器属性
```

实现（第一版曾"递归扫描目录 + 合成单图集 JSON + 逐图调 loadTextureAtlasData"，
按评审意见废弃 —— JSON 拼装与目录遍历是不必要的复杂度）：

1. **遍历已加载的 DragonBonesData → ArmatureData → skins → SlotDisplays**，
   收集全部 `DisplayData::path`（Image/Mesh；子骨架 Armature 类型跳过）。
   散图的纹理名就是它 —— 不扫描目录、不猜文件，直接按骨架数据里引用到的取。
2. 每个散图目录建一个 `DragonBonesTextureAtlasData` 容器
   （`_buildTextureAtlasData(nullptr, nullptr)`，**不加载整图**，`display_texture` 为空）。
3. 对每个 path：加载 `<dir>/<path>.png`，`atlas->createTexture()` 创建
   `DragonBonesTextureData`：`region = (0,0,W,H)`（全图），**纹理与尺寸直接挂在
   `DragonBonesTextureData` 上**（`texture` / `texture_size` 新字段），
   `atlas->addTexture(texture_data)` 入集。
4. `addTextureAtlasData(atlas, dataName)` 注册到全部已加载的数据名下 ——
   `_getTextureData(dataName, path)` 精确命中同名 vector 逐图集按名查找。

配套改动：

- `DragonBonesTextureData` 新增 `Ref<Texture2D> texture` 与 `Vector2 texture_size`
  （图集模式为空，行为不变）。
- `Slot_GD::get_texture()`：优先返回 `DragonBonesTextureData::texture`，
  为空回落图集 `display_texture`（图集模式不变）。
- `Slot_GD::_updateFrame()`：UV 归一化尺寸优先用 TextureData 自带尺寸
  （散图），否则用图集宽高（图集模式不变，避免除 0）。

效果：每部件一张独立纹理；渲染层按每槽位纹理 RID 分组合批，天然多纹理；
`"None"`（displayIndex = -1）语义不变（issue #62 修复不回归）。

**备选（否）**：把 N 张散图合成一张运行时图集（`Image::blit_rect` 拼大图）——
省图集对象但引入打包算法与内存峰值，且失去"用户可单独替换部件纹理"的散图核心价值
（issue #22 提到的诉求）。

### 2.1b 修正方案：散图按数据名（套）管理（评审后修订，待实现）

评审发现的两个缺陷：

- **P1 隐藏耦合**：`load_scattered_texture_dir_list()` 内部调用
  `load_texture_atlas_json_file_list(member)` 做全量重载 —— "设置散图目录"会
  隐式清掉并重载与散图无关的图集注册。
- **P2 工厂级聚合**：`display_path_set` 从**所有** DragonBonesData 收集、图集
  注册到**所有**数据名下。多套资源并存时：A 套目录必然缺 B 套的部件 PNG →
  `ERR_CONTINUE_MSG` 毒化 `err` → 整个导入失败；且同名部件会按注册顺序命中
  错误纹理（静默错误）。

**根因**：散图纹理的归属是"每套 DragonBonesData"，实现却做成了工厂级聚合。
按套管理是正确方向；在大重构（ResourceSet 成组管理）落地前，本节先把散图
的归属收敛到数据名粒度 —— key 即未来的"套"标识，重构时不返工。

修改点（只动 `src/factory.h/.cpp` 与 `.dbfactory` 字段，导入器微调）：

1. **API 按套**：
   - 废弃 `load_scattered_texture_dir_list(PackedStringArray)`；
   - 新增 `Error set_scattered_texture_dirs(const String &p_data_name, PackedStringArray p_dirs)`
     —— `p_dirs` 为空表示清除该套散图；内部**只遍历 `_dragonBonesDataMap[p_data_name]`**
     的 armatures/skins/displays 收集显示路径（→ P2 的路径收集修复）；
   - 成员 `std::map<std::string, PackedStringArray> _scattered_texture_dirs;`
     按套记忆（序列化/重载用）。

2. **选择性清理**（→ P1 解决）：
   - 成员 `std::map<std::string, std::vector<DragonBonesTextureAtlasData *>> _scattered_atlases;`
     记录每套注册的图集指针；
   - 重设某套时：用 `make_dragon_bones_data_unref_texture_atlas_data()` 解除
     DragonBonesData 对这些图集的引用，把它们从 `_textureAtlasDataMap[data_name]`
     向量中移除并 `returnToPool()`；**向量中非散图条目（图集模式）不动**，
     也不再调用 `load_texture_atlas_json_file_list`。

3. **注册范围**：`addTextureAtlasData(atlas, data_name)` 只挂到该套名下
   （→ P2 的静默错纹理解决）。

4. **缺失纹理的错误处理**：
   - 缺失 = 该套散图目录缺文件（美术漏导出）：收集全部缺失项后**一次性**
     `ERR_PRINT` 列明 `data_name + path + 目录`，不再逐条 `ERR_CONTINUE` 毒化；
   - 函数返回 `ERR_PARSE_ERROR`（该套资源加载失败，导入器对该资源报红），
     按套隔离后不影响其他套。

5. **序列化与导入器**：
   - `.dbfactory`：`scattered_texture_dirs` 从扁平数组改为 **Dictionary**
     （`data_name → dirs`）。本分支未发布，格式可直接改，无兼容负担；
   - `try_import()`：ske 加载后用 `get_loaded_dragon_bones_data_name_list()`
     取本资源的数据名（单个），以它为 key 调新 API。

6. **与大重构的关系**：`_scattered_atlases[data_name]` /
   `scattered_texture_dirs[data_name]` 的 key 就是未来"套"（ResourceSet）的
   粒度；重构时把 key 从 String 换成 set 引用即可，不返工。

验证计划（对应修复）：

- Sheep 单套：5 动画渲染完整（回归）
- **双套共存**：Dragon_scattered + Sheep 两套散图 + Dragon 图集在同一工厂加载，
  互不污染、无报错（P2 的直接对应测试）
- 缺文件场景：移除一张 png → 报错信息指明 `data_name + path`，其余部件与
  其他套正常
- 图集模式回归（Dragon/guy/龙）+ doctest + CI

### 2.2 导入器：识别散图导出（issue #22）

`DragonBonesImportPlugin::try_import()`（`src/editor/dragon_bones_editor_plugin.cpp:160`）
在 `_tex.json` 不存在时不再直接返回 `{}`，而是做散图探测：

- 探测规则（按优先级）：
  1. `<base>_texture/` 目录存在且含 `*.png`（DragonBones 编辑器 Images 模式的
     目录命名惯例，Sheep 即 `Sheep_texture/`）；
  2. 兜底：`<base>_ske.json` 同目录下存在与任一 display/path 同名的 `*.png`
     （防御非标准目录名；需要解析 ske json 的 skin display 名集合）。
- 命中后：dbfactory 增加 `scattered_texture_dir_list`（新属性）记录目录，
  运行时由 2.1 的入口加载。
- 未命中：维持原行为（不是龙骨资源）。

### 2.3 dbfactory / 检查器

- `DragonBonesFactory` 新增 `scattered_texture_dir_list` 属性（带 dir hint），
  序列化进 `.dbfactory`（复用现有 ResourceSaver 路径，`dragon_bones_editor_plugin.cpp:150-153`）。
- 图集 json 列表与散图目录可共存（R4：混用多资源）。

### 2.4 渲染（预期零改动，需验证）

`rebuild()` 已按 texture_rid 分 surface；多纹理即多 surface，理论天然支持。
issue #40 报告的 `array_len == 0` 与部件消失出自旧版 `_draw()` 批处理实现
（issue 评论定位在 329 行附近，该区域已重构为 ArmatureDrawData/SurfaceData）。
用多图集场景回归验证（AC2），若复现再单独定位。

## 3. 风险与决策点

| 风险/决策 | 处理 |
|---|---|
| 同名图集顺序影响同名纹理的命中优先级 | 散图与图集的纹理名来自不同导出，实践中不冲突；如遇冲突先注册者优先 |
| 散图目录名非 `<base>_texture` | 探测规则第 2 条兜底（按 display 名匹配同目录 png） |
| mesh 槽 FFD 依赖 `display == _meshDisplay` 的指针比较（`_updateFrame` 中 `currentVerticesData`） | 本项目 raw/mesh 同对象；合成图集不改变该关系。散图 mesh 的 `displayData->texture` 同样走 `_getTextureData` |
| 贴图缺失（png 被删/未导入） | 沿用图集模式的 `ERR_FAIL_COND_V` 行为：该图集加载失败打错误，不崩溃；对应部件不显示 |
| 旧版 dbfactory 兼容 | 新属性有默认空值，旧资源反序列化不受影响 |
| `Image::load()` 读 png 需要 import 后的 .ctex 还是原文件 | 用 `ResourceLoader::load` 得到 `Texture2D` 再 `get_size()`，与 `_buildTextureAtlasData` 同路径；目录探测在导入器侧只 `FileAccess::exists` |

## 4. 测试设计

- 单元（doctest，`tests=yes`）：合成图集 JSON 生成的纯逻辑（名字、region、
  imagePath 拼接）——不依赖资源的部分。
- 集成（手测/场景）：Sheep 案例渲染对照 DragonBones 编辑器截图；5 个动画轮播。
- 回归：Dragon / guy（图集模式）渲染不变；issue #62 场景（None 切换）不回归；
  doctest + CI 全绿。
