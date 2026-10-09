# 技术设计

## 1. 边界与职责划分

```
*_ske.json / *_ske.dbbin ──┐
*_tex.json  (图集模式)     ├──►  DragonBonesData (Resource, *.dbdata)
*_texture/  (散图模式)     ┘         │  只描述数据源 + 桥接
                                     │
                        DragonBonesArmatureView (Node2D)
                                     │  持有 Ref<DragonBonesData>
                                     ▼
                      DragonBonesFactory (全局单例, 不暴露给脚本)
                                     │  解析 / 构建 / 缓存运行时数据
                                     ▼
                        dragonBones::BaseFactory 内部 maps
                                     │
                        DragonBonesArmature ← create_armature()
```

关键点：`.dbdata` 是**纯数据源描述 + 桥接器**，它自己不解析、不构建、不缓存；单例工厂才是唯一持有运行时数据的地方。

## 2. 类名与命名冲突

用户已决定：资源类名为 `DragonBonesData`（对外命名即此）。它与 `dragonBones::DragonBonesData` 同名。

用户指定的处理方式：在同时引入两个 namespace 的翻译单元里定义两个别名区分。建议在**一个共享头**中集中定义（例如放进新的 `src/db_data.h`，或已有的 `src/godot_dragon_bones.h` ），避免每个 `.cpp` 各写一份：

```cpp
namespace godot {
using GDDragonBonesData = godot::DragonBonesData;        // 资源（*.dbdata）
} //namespace godot
namespace dragonBones {
using DBDragonBonesData = dragonBones::DragonBonesData;  // 运行时数据
} //namespace dragonBones
```

约束：别名必须定义在**两个 namespace 都未 `using`** 的头里，否则 alias 自身也会歧义。受影响的现有文件（都用 `using namespace godot; using namespace dragonBones;`）：`src/factory.cpp:55-56`、`src/armature.cpp` 等。逐个替换为限定名或别名，不做全局 `using`。

`src/dragon_bones.cpp`/`.h` 里的内部单例类 `godot::DragonBones` 与新类名 `DragonBonesData` 不冲突（不同标识符）。

## 3. `.dbdata` 资源设计

### 3.1 字段

| 字段 | 类型 | 说明 |
|---|---|---|
| `ske_file` | `String` | 单个骨架文件（`*_ske.json` 或 `*_ske.dbbin`）。hint：`PROPERTY_HINT_FILE`，过滤 `"*.json,*.dbbin"` |
| `texture_source_type` | `enum`（`TextureSourceType`） | `TEXTURE_SOURCE_ATLAS` / `TEXTURE_SOURCE_SCATTERED` |
| `texture_source` | `String` | 图集模式 = `*_tex.json`；散图模式 = `*_texture/` 目录 |

### 3.2 检查器 hint 动态切换（用户已确认「单字段 + 模式枚举」）

引擎侧核实结论：

- `PROPERTY_HINT_DIR` 走 `EditorPropertyPath(folder=true)` → `EditorFileDialog::FILE_MODE_OPEN_DIR`（`editor/inspector/editor_properties.cpp:4155`、`_path_pressed`）。
- `PROPERTY_HINT_FILE` 走 `folder=false` → `FILE_MODE_OPEN_FILE` + 按 `hint_string` 加过滤器。
- **不存在**「文件或目录二选一」的单一 hint。`PROPERTY_HINT_FILE_PATH` 仅在 `extension_api-4-5.json` 及以后存在（4.3/4.4 无），且在 4.5+ 仍只走 `FILE_MODE_OPEN_FILE`。

因此实现为：

```cpp
void DragonBonesData::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == SNAME("texture_source")) {
		if (texture_source_type == TEXTURE_SOURCE_SCATTERED) {
			p_property.hint = PROPERTY_HINT_DIR;
			p_property.hint_string = "";
		} else {
			p_property.hint = PROPERTY_HINT_FILE;
			p_property.hint_string = "*.json";
		}
	}
}
```

`texture_source_type` 的 setter 必须 `notify_property_list_changed()`（`#ifdef TOOLS_ENABLED`），让检查器立即换编辑器——仓库已有该模式的先例（`src/armature_view.cpp:140-142`）。

`PROPERTY_HINT_DIR` 与 `PROPERTY_HINT_FILE` 在 4.3→4.8 全范围存在（均早于 4.3），无需版本守卫。

### 3.3 两种管理形态（用户已确认）

`is_imported` **保留**，并承担「区分两种管理方式」的职责：

| 形态 | `is_imported` | 谁创建 | `ske_file` / `texture_source` 存什么 | 可编辑 |
|---|---|---|---|---|
| 自动导入 | `true` | 导入器 | **仅文件名 / 文件夹名**（如 `Dragon_ske.json`、`Dragon_texture`） | 只读（`_validate_property` 锁死） |
| 用户手动创建 | `false` | 用户 | 用户自己选的**完整路径** | 自由编辑 |

自动导入形态的关键收益：不记录绝对路径 → 资源与数据文件同目录 → **整体移动目录即安全**，不再需要 `get_all_imported_factories` 之类的编辑器跟随逻辑。

**路径解析规则（由 `is_imported` 显式分流，不靠猜）**：

```cpp
String DragonBonesData::resolve_ske_path() const {
    if (is_imported) {
        return get_path().get_base_dir().path_join(ske_file);   // 同目录 + 名字
    }
    return ske_file;                                             // 用户填的完整路径
}
```

> 不用「有 `/` 就当路径、否则当名字」这种启发式：名字里可能出现子目录片段，启发式会在边界情形上猜错，而 `is_imported` 是明确的、已知的事实。

纹理侧同理（`texture_source` 是文件还是目录由 `texture_source_type` 决定）。

### 3.4 桥接接口

`.dbdata` 需要向 View/工厂传递：解析后的 ske 路径、解析后的图集路径或散图目录、以及**唯一缓存名**。不要把三件事拆成三个公开 getter 让调用方自己拼；提供明确的桥接调用面：

```cpp
String get_effective_cache_name() const;   // 见 §4
String resolve_ske_path() const;           // 见 §3.3
String resolve_texture_source_path() const;
```

### 3.5 资源序列化格式（用户已确认：沿用现有 cfg 格式）

**引擎侧核实结论**：不注册自定义 saver/loader，就**无法**写出 `.dbdata` 扩展名。

- 引擎内置两个 saver 的 `recognize()` 都恒为 `true`（`scene/resources/resource_format_text.cpp`、`core/io/resource_format_binary.cpp`），靠 `get_recognized_extensions` 区分：text → `"tres"/"tscn"`；binary → `get_base_extension()` + `"res"`。
- `ResourceSaver::recognize_path` 默认即「路径扩展名 ∈ saver 的扩展名列表」（`core/io/resource_saver.cpp`）。所以 `ResourceSaver::save(res, "x.dbdata")` 两个内置 saver 都不认，返回 `ERR_FILE_UNRECOGNIZED`。
- 改 base extension 需要 `RES_BASE_EXTENSION(m_ext)` 宏（`core/io/resource.h:40-49`），**godot-cpp 未暴露**该宏或其虚函数。

因此**保留自定义 loader/saver**，格式沿用现有 ConfigFile 文本（`VERSION` / `UID` / `[properties]` / `[other]`），仅把字段从三个路径列表换成 `ske_file` / `texture_source_type` / `texture_source` / `imported`。

> 用户提到「压缩后再打包」：经实测**决定不做**（详见 §6.3）。打包位置维持现状——`convert_to_imported_path()` 的 `.godot|godot/imported/<md5>.dbimport` 隐藏路径。

## 4. 缓存命名：消除同名冲突（R3，本设计最关键的一节）

### 4.1 问题

全局单例后所有 `.dbdata` 共用一份 `_dragonBonesDataMap`。现状 `load_dragon_bones_ske_file_list` 调 `loadDragonBonesData(raw)` **不传 name**（`load_dragon_bones_ske_file_list` 内 `loadDragonBonesData((const char *)raw_data.ptr())`，`src/factory.cpp:265`），于是 `parseDragonBonesData` 用 JSON 内嵌 `name` 当键（`BaseFactory::addDragonBonesData`：`mapName = name.empty() ? data->name : name`）。`Dragon/` 与 `Dragon_scattered/` 内嵌名都是 `Dragon` → 第二套被静默丢弃。

### 4.2 方案：资源自增 ID 作键（用户决定）

缓存键**不取** JSON 内嵌名，也用**不**资源路径 —— 而是 `.dbdata` 内部一个**静态自增 ID**：

```cpp
static std::atomic<uint64_t> next_uid;
const uint64_t uid{ next_uid.fetch_add(1, std::memory_order_relaxed) };
String get_effective_cache_name() const { return vformat("dbdata:%d", uid); }
```

**为什么不能用资源路径**（最初方案，已废弃）：`Resource` 可以在代码里 `DragonBonesData.new()`，此时 `get_path()` 返回空串、`get_name()` 也无内容 —— 用户手动创建的资源**根本拿不到键**。这不是边界情况，而是「用户自行创建资源」这一受支持的用法。

**为什么不用运行时内嵌 name**：不同目录下的两套骨架可能同名（实测 `Dragon/` 与 `Dragon_scattered/` 内嵌 name 都是 `Dragon`），单例 map 下会互相覆盖并静默丢数据。

线程安全：`std::atomic<uint64_t>::fetch_add`，`memory_order_relaxed` 足够（只需「不重复」，无需跨线程顺序）。C++17 已由构建系统启用（`CMakeLists.txt` `CMAKE_CXX_STANDARD 17`）。

> **事实澄清**（设计初稿的错误论述，已纠正）：曾以「省内存」为由推荐单例。核实后**该理由不成立** ——
> - 纹理走 `ResourceLoader::load()`，默认 `CACHE_MODE_REUSE`，**同名纹理天然共享**，与工厂个数无关；
> - 单例与否影响的是 **JSON 解析出的模型对象**，但**相同素材若来自不同路径的两个 `.dbdata`，单例同样会解析两遍**（键不同）——本设计不做内容级去重。
>
> 因此单例的真实代价是：必须维护 `_loaded_keys` 记账表 + 资源析构要回调单例（潜在 use-after-free，见 §9），而收益仅是「同一个 `.dbdata` 被多处引用时只解析一次」。**用户决定：先按单例做**（缓存键用自增 ID），并保留「以后改成 `.dbdata` 持有 `BaseFactory` 成员」的可能 —— 那时键与记账表都可删掉。

### 4.3 连带影响（必须在同一改动内处理，否则功能静默错乱）

`_getTextureData(textureAtlasName, textureName)` 用 **dataName** 在 `_textureAtlasDataMap` 里找图集（`thirdparty/dragonBones/factory/BaseFactory.cpp:225/229/242/246` 传的 `dataName` 即 `dataPackage->dataName`，而 `dataPackage->dataName = mapName`，见 `_fillBuildArmaturePackage`）。

因此**图集必须以同一个 `cache_name` 注册**，否则改了 key 之后纹理查不到、槽位变空白。三处必须用同一个键：

1. 注册骨架：`addDragonBonesData(data, cache_name)`
2. 注册图集（图集模式）：`addTextureAtlasData(atlas, cache_name)`——注意现状走 `BaseFactory::parseTextureAtlasData(rawData, textureAtlas, name)` 的 `name` 参数（`src/factory.cpp:92-94` 转发），把 `cache_name` 传进去即可。
3. 注册散图图集（散图模式）：现状 `addTextureAtlasData(atlas, key)` 已在用 data_name（`src/factory.cpp:427`），改为 `cache_name`。

`autoSearch` 在**本仓库从未被置为 `true`**（`BaseFactory.h:87` 构造为 `false`，`DragonBonesData.cpp:25`、`TextureAtlasData.cpp:12` 亦为 `false`，全仓库无赋值处）。因此 `_getTextureData` 的 autoSearch 兜底分支（`BaseFactory.cpp:23-40`）实际永不生效，后果是**确定的**：按名查不到图集就返回 `nullptr`，该 slot 的纹理变空白（不是拿到别人的纹理，但同样静默——不报错、不崩溃，只是画面缺件）。

设计上要求：三处注册键显式一致，不得依赖 autoSearch。

### 4.4 多套共存时的加载/卸载语义

单例工厂需要一套「按 cache_name 增删」的接口，替代现状的三个扁平列表 setter（`set_dragon_bones_ske_file_list` / `set_texture_atlas_json_file_list` / `set_scattered_texture_dir_list`，`src/factory.cpp:320-328,434-443`）——那是「一次设置全量替换」的语义，与全局单例不匹配。

建议接口（`.dbdata` 作为唯一调用者）：

```cpp
// 幂等：已加载则直接返回；未加载则解析 ske + 图集/散图并注册
Error ensure_data_loaded(const Ref<DragonBonesData> &p_data);
// 引用计数归零时卸载该套
void release_data(const String &p_cache_name);
```

**不需要额外的引用计数**（用户已确认）。理由（已核实）：

- `ResourceCache::resources` 是 `HashMap<String, Resource *>`——**裸指针，不持 Ref**（`core/io/resource.h:197-201`），并在 `Resource::~Resource()` 里摘除自己。
- 场景加载时 `DragonBonesArmatureView` 持有 `Ref<DragonBonesData>`，所以 `.dbdata` 的**存活完全由 Ref 计数决定**——`Resource` 自身的引用计数就是所需的那个计数器。

因此：**在 `DragonBonesData::~DragonBonesData()` 里调 `factory->release_data(cache_name)`**，效果与「引用计数归零即卸载」等价，无需自建计数表。

> **已知风险（暂不处理，用户决定）**：`DragonBonesData` 的析构可能晚于单例工厂（退出时销毁顺序不确定），届时 `~DragonBonesData` 访问已销毁的单例即 use-after-free。**当前不写防御代码**，待实测出现该问题再处理。

## 5. `DragonBonesArmatureView` 改造

| 现状 | 改为 |
|---|---|
| `Ref<DragonBonesFactory> factory`（`src/armature_view.h:60`） | `Ref<DragonBonesData> data`（属性名定为 `data`） |
| `add_property("factory", RESOURCE_TYPE, "DragonBonesFactory")`（`src/armature_view.cpp:859`） | `ADD_PROPERTY(PropertyInfo(Variant::OBJECT, ..., PROPERTY_HINT_RESOURCE_TYPE, DragonBonesData::get_class_static()), ...)` |
| `rebuild_armature()` 调 `factory->create_armature(this, data_name, armature_name, skin_name)`（`src/armature_view.cpp:315-330`） | 先 `DragonBonesFactory::get_singleton()->ensure_data_loaded(data)`，再请求构建；构建参数中的 `data_name` 换成 §4 的 `cache_name` |
| `instantiate_dragon_bones_data_name`（选哪套数据） | 单套资源下语义被资源本身取代。检查器里仍可暴露「armature 名 / skin 名」，但其候选列表来自该资源已加载的那一套；`instantiate_dragon_bones_data_name` 建议移除（待确认，见 §8） |
| `_validate_property` 用 factory 的 loaded 名称列表填 hint（`src/armature_view.cpp:648-666`） | 改为从单例工厂按 `cache_name` 查该套的 armature/skin 名列表 |

卸载：`~DragonBonesArmatureView`（`src/armature_view.cpp`）在 `armature->release()` 后调 `DragonBonesFactory::get_singleton()->release_data(cache_name)`（配合 §4.4 的引用计数）。

## 6. 编辑器层改造

### 6.1 自动导入（R4）

`DragonBonesImportPlugin::try_import`（`src/editor/dragon_bones_editor_plugin.cpp:156-204`）不再组装 `DragonBonesFactory`，改为组装一个 `DragonBonesData` 资源并填 `ske_file` / `texture_source_type` / `texture_source`：

- 有 `<base>_tex.json` → `TEXTURE_SOURCE_ATLAS` + 该 json 路径
- 有 `<base>_texture/` → `TEXTURE_SOURCE_SCATTERED` + 该目录路径

`_get_save_extension`（`:123-125`）→ `"dbdata"`；`_get_resource_type`（`:127-129`）→ 新类名。

`_reimport_dbfactory_recursively`（`src/editor/dragon_bones_editor_plugin.cpp:211-241`）里的 `SAVED_EXT` 拼接改为 `dbdata`；`_reimport_moved_factory_files`（`:280-318`）与 `_on_file_system_dock_files_moved`（`:280-284`）**整段删除**（见下）。

**`get_all_imported_factories` 删除**（用户决定）：该注册表（`src/factory.h:133-136`）只服务于「编辑器移动文件时跟随更新」——新方案下自动导入的资源字段只存名字、与数据同目录，整体移动目录即安全，**不需要跟随逻辑**；用户手动创建的资源由用户自管。因此连同 `get_all_imported_factories`、`_on_file_system_dock_files_moved`、`moved_factory_files`、`_reimport_moved_factory_files` 一并删除。

设置项名 `Godot_DragonBones/auto_generate_dbfactory`（`src/editor/dragon_bones_editor_plugin.cpp:209`）——spec 明确警告这是**项目设置迁移**而非改名（`editor-plugin.md` 的 Gotcha）。是否改名为 `auto_generate_dbdata` 需决策（见 §8）。

### 6.2 导出插件（容易漏，必须同改）

`DragonBonesExportPlugin::_export_file`（`src/editor/dragon_bones_editor_plugin.cpp:55-79`）目前按 `DragonBonesFactory::get_class_static()` 判类型，并把 `get_dragon_bones_ske_file_list()` + `get_texture_atlas_json_file_list()` 里的源文件以 `convert_to_imported_path` 的隐藏路径打入 pck。

改为：按 `.dbdata` 判类型，取它的 `ske_file` + `texture_source`（散图模式还要打进整个目录下的 PNG）。**若漏改**：编辑器内一切正常，导出后的游戏跑起来找不到源文件——典型「只在导出后暴露」的故障。

`get_imported_file_name` / `convert_to_imported_path`（`src/factory.h:74-78`）**用户已决定**：提为独立工具或工程静态函数。导出与运行期回退都继续用它，**打包位置不变**（`.godot|godot/imported/<md5>.dbimport`）。

**两种形态的打包差异**：

- 自动导入（`is_imported = true`）：字段是裸名字，导出插件按「资源同目录 + 名字」解析出源文件再打入 pck。
- 用户手动创建：字段是完整路径，直接取用。

### 6.3 格式读写（硬切）

删除 `.dbfactory` 专用的 **FileProcessor / Saver / Loader**，但 **`.dbdata` 仍需要自己的 loader/saver**（原因见 §3.5：不注册就无法使用自定义扩展名）。

| 删除 | 保留并改造 |
|---|---|
| `DragonBonesFactoryFileProcessor`（`src/factory.h:145-156`，`src/factory.cpp:580-671`） | — |
| `ResourceFormatSaverDragonBones`（`src/factory.h:158-168`，`src/factory.cpp:677-711`） | → `ResourceFormatSaverDragonBonesData`（扩展名 `dbdata`，cfg 格式写 `ske_file`/`texture_source_type`/`texture_source`/`imported`） |
| `ResourceFormatLoaderDragonBones`（`src/factory.h:170-186`，`src/factory.cpp:717-788`） | → `ResourceFormatLoaderDragonBonesData`（同格式读；UID 处理照搬现状：编辑器内补建并回写） |
| 注册行 `src/dragon_bones_registration.cpp:79-80` | 换成新类的注册（数量不变，仍是 2 行） |

`.dbdata` 的 cfg 依旧写 UID 到顶层（与现状一致，**不是问题**）；字段精简为：`[properties]` 存 `ske_file` / `texture_source_type` / `texture_source`，`[other]` 存 `imported`。

> 更正：设计初稿曾写「`.dbdata` 可直接走标准 Resource 序列化、无需自定义格式」——该结论**错误**（见 §3.5 的引擎核实）。

### 6.4 打包压缩：实测后决定不做（用户决定）

曾评估「对打包的数据文件做压缩」。**实测数据**（引擎侧 `PackedByteArray::compress(mode)`，4.3 / 4.6.1 / 4.8-dev 逐字节一致）：

| 文件 | 原始 | fastlz | **deflate** | zstd | gzip | brotli |
|---|---|---|---|---|---|---|
| `Dragon_ske.json` | 18165 | 25% | **15%** | 17% | 15% | ❌ 报错 |
| `Sheep_ske.json` | 58134 | 26% | **16%** | 17% | 16% | ❌ 报错 |
| `龙_ske.json` | 44056 | 46% | **29%** | 30% | 29% | ❌ 报错 |
| `Dragon_tex.png` | 159196 | 98% | **94%** | 95% | 94% | ❌ 报错 |

核实到的约束：
- **brotli 不能用于压缩**：`Only brotli decompression is supported`（`core/io/compression.cpp:48`）。
- `decompress_dynamic` 只支持 DEFLATE/GZIP；FASTLZ/ZSTD 报 `Condition "p_mode != MODE_DEFLATE && p_mode != MODE_GZIP"`。故**必须存原始大小**，用定长 `decompress(size, mode)`。
- **PCK 本身零压缩**：`PackedFile` 只有 `offset/size/md5/flags`，`pack_flags` 无压缩位（`editor/export/editor_export_platform.cpp:2151-2158`）；文件在 pck 里是原样字节。

**结论（用户决定）：不做压缩。** 打包位置维持现状（`convert_to_imported_path()` 的隐藏路径）。理由：收益集中在 ske JSON（-71~-84%），但图集 PNG 压不动（-2~-6%），而「以图集为主」的资源整体收益只有 3.5~9.2%；额外引入的复杂度（压缩标记、原始大小、双格式分支）不值得。

## 7. 生命周期与内存

按 `memory-and-lifetime.md`：

- 单例工厂在 module init（`MODULE_INITIALIZATION_LEVEL_SCENE`）创建、uninit 销毁，与现有 `godot::DragonBones` 单例同处（`src/dragon_bones_registration.cpp` 的 SCENE init，`dragon_bones = memnew(DragonBones);` 处）。销毁顺序：**必须先销毁工厂**（它会 `returnToPool` 运行时数据），再 `BaseObject::clearPool()`（`src/dragon_bones_registration.cpp:105`），否则池被清后工厂再析构会二次释放。
- 工厂持有的 `Ref<Texture2D>` 必须在卸载该套时 `unref()`（现状散图路径挂在 `DragonBonesTextureDataScatted` 上，`src/texture_atlas_data.h`）。
- 若工厂持有静态表（如 `all_imported`、调色板之类），按约定注册 `DragonBones::add_clean_static_callback`（`src/dragon_bones.h:66-71`）。

## 8. 决策记录（已全部确认）

| # | 问题 | 决定 |
|---|---|---|
| 1 | `is_imported` 是否保留 | **保留**，并用于区分「自动导入」与「用户手动创建」两种形态（§3.3） |
| 2 | `get_all_imported_factories` 归属 | **删除**。新方案下自动导入的资源与数据同目录、整体移动即安全，不再需要编辑器跟随逻辑；用户手动创建的资源由用户自管 |
| 3 | 项目设置名 `auto_generate_dbfactory` | **改名**（破坏性变更，需在 PR 中显式说明）；相关属性名等一并改名 |
| 4 | `.dbdata` 是否需要 loader/saver | **需要**。不注册就无法使用 `.dbdata` 扩展名（§3.5 引擎核实）；沿用 cfg 格式 |
| 5 | View 的属性名 | `factory` → 改为引用 `.dbdata` 的属性；**移除 `instantiate_dragon_bones_data_name`** |
| 6 | `get_imported_file_name` / `convert_to_imported_path` 归属 | 提为**独立工具 / 工程静态函数**；打包位置不变（§6.2、§6.4） |
| 7 | 缓存键选什么 | **资源内静态自增 ID**（§4.2）。路径不可用（手动 new 的资源无 path），内嵌 name 会撞名 |
| 8 | 是否引入引用计数 | **不引入**。`Resource` 自身的 Ref 计数已足够；在 `~DragonBonesData()` 里 `release_data` 即可（§4.4） |
| 9 | 打包压缩 | **不做**（§6.4 实测：收益集中在 JSON，图集 PNG 压不动，整体不划算） |
| 10 | 打包位置 | **维持现状**（`.godot|godot/imported/<md5>.dbimport`） |

## 9. 暂不处理的已知风险

| 风险 | 说明 | 触发条件 |
|---|---|---|
| `.dbdata` 析构晚于单例工厂 | `~DragonBonesData` 里访问已销毁的单例 = use-after-free（§4.4） | 退出时销毁顺序不确定 |
| 缓存键漏改一处 → 纹理静默空白 | 骨架与图集注册键须一致（§4.3） | 改动不完整 |

> 上述两项均**用户明确决定当前不写防御代码**，待实测出现再处理。第一项需在实现后留意退出时的报错；第二项由 §4.3 的强制一致 + 验收项覆盖。
