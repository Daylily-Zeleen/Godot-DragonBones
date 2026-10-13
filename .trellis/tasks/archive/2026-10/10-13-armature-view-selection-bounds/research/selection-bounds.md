# Q2–Q4：`ArmatureData::aabb` 语义、仓库现状坐标链、编辑器规范约束

配套文件：Q1（`_edit_*` 虚函数的暴露与可覆盖性）见 [edit-rect-hooks.md](./edit-rect-hooks.md)。

标注约定：`[可证]` = 本地文件/命令直接得出；`[推断]` = 由代码机制推导（附依据）；`[未验证]` = 本地无法确认。

---

## Q2 `thirdparty/dragonBones` 里 `ArmatureData::aabb` 的来源与语义

### 事实

| 事项 | 结论 | 锚点 |
|---|---|---|
| 存储 | `dragonBones::Rectangle aabb;`（数据模型字段） | `thirdparty/dragonBones/model/ArmatureData.h:85`；`getAABB()` 直接返回其地址 `:302` |
| 键名常量 | `DataParser::AABB = "aabb"` | `thirdparty/dragonBones/parser/DataParser.cpp:61` |
| 写入（唯一解析点） | 解析 armature JSON 时读 `rawData["aabb"]` 的 x/y/width/height，**乘 `armature->scale`**；仅当成员存在时写入（缺省则保持 `clear()` 后的 0） | `thirdparty/dragonBones/parser/JSONDataParser.cpp:186-192` |
| 重置/清空 | `aabb.clear()`（ArmatureData 自身重置路径） | `thirdparty/dragonBones/model/ArmatureData.cpp:64`（`scale = 1.0f; name = "";` 之后，属初始化/清理段 `:55-66`） |
| 运行期是否更新 | **否**。整个 `thirdparty/dragonBones` 与 `src/` 内对 `aabb`/`getAABB` 的引用穷举只有：上面的解析写入、`ArmatureData.cpp:64` 的清空、以及唯一读取点 `src/armature.cpp:228-233`。没有随动画/姿态重算的代码 | `[可证]`（grep 全仓 `aabb|getAABB`） |
| 消费点 | `DragonBonesArmature::get_rect()` 直接把它当 Rect2 返回 | `src/armature.cpp:227-234` |
| 数据来源（样本） | demo 的 4 份 `*_ske.json` 第 1 行都带 `"aabb":{...}`，形如 Dragon：`{"x":-234.41,"y":-431.81,"width":693.28,"height":769.59}` | `demo/dragonbones_demo/assets/Dragon/Dragon_ske.json:1`、`.../Sheep/Sheep_ske.json:1`、`.../test/idle_ske.json:1`、`.../龙/龙_ske.json:1` |

### 语义判定

- `[可证]` 它是**导出文件中携带的静态数据**，运行期不变；不随当前动画帧/姿态更新。
- `[未验证]` 它是否等于“全部动画全部帧的并集”。该值由 DragonBones Pro（设计工具）写入；本地没有工具端实现，无法证明其生成算法。从样本看它明显大于骨骼原点附近的默认框（例：Dragon 的 aabb 从 `(-234.41, -431.81)` 起、尺寸 `693×769`），因此**至少**不是空的默认值。
- `[推断]` 它表达的是「骨架数据空间（armature data space）」中的矩形：解析时已乘 `armature->scale`（`JSONDataParser.cpp:188-191`），说明它是“该骨架自身的单位/缩放体系下的坐标”，与实例的节点变换、翻转无关（那两者是运行期状态）。
  - 佐证：`DragonBonesArmature::get_rect()` 才在 `src/armature_view.cpp:1004-1014` 里用节点变换做映射（说明 aabb 本身不是节点/全局空间坐标）。
- `[推断]` 不含：节点/父级变换、armature 实例翻转（`flip_x/flip_y`）、当前动画采样结果、slot 可见性。

---

## Q3 仓库现状：`get_rect()` 坐标空间链、绘制能力、编辑器钩子、动态边界数据可得性

### 3.1 现有 `get_rect()` 链（逐段锚点）

```
ArmatureData::aabb  (静态, 数据空间)
  └─ DragonBonesArmature::get_rect()            src/armature.cpp:227-234
       └─ DragonBonesArmatureView::get_rect()        src/armature_view.cpp:1004-1008
            return get_transform().inverse().xform(rect);          :1007
       └─ DragonBonesArmatureView::get_global_rect() src/armature_view.cpp:1010-1014
            return get_global_transform().inverse().xform(rect);   :1013
```

- `[可证]` 两者都对 aabb 施加**节点变换的逆**：`get_rect()` = 本地→父空间映射、`get_global_rect()` = 本地→全局空间映射（若 aabb 视为节点本地坐标）。
- `[推断]` 与 Node2D 常规约定（`get_rect()` 应返回本地空间）相比，`get_rect()` 的命名/空间并不自洽；规范也把 `get_global_rect` 的 inverse 记为已知技术债：`.trellis/spec/gdextension/quality-guidelines.md`（Known Tech Debt 行：`get_global_rect applies the inverse transform | src/armature_view.cpp:755`）——**该行号已过期**（实测为 `:1010-1014`）。
- `[推断]` 变换用的是 `Rect2::xform`（仅映射两个角），在带旋转/负缩放的父变换下会得到非轴对齐或负尺寸矩形，代码未做归一化。
- `[可证]` 公开文档已登记：`doc_classes/DragonBonesArmatureView.xml:56`（`get_global_rect`）、`:68`（`get_rect`，注明是 `DragonBonesArmature.get_rect` 的包装）；`doc_classes/DragonBonesArmature.xml:72`。

### 3.2 `_draw()` 与绘制数据链（空间结论）

- `[可证]` 提交路径：`src/armature_view.cpp:676`（`_draw()` 定义）→ `:682-683` 每帧 `begin_frame()` + `armature->append_draw_data(draw_scratch.draw_data)`（thread_local scratch）→ `:722` `RS->canvas_item_add_mesh(get_canvas_item(), mesh, identity, get_modulate(), texture)`（**identity** 网格变换）→ `:727` `debug_draw.draw(armature, draw_data)`（在 `#ifdef DEBUG_ENABLED` 块内）。
- `[可证]` 几何进入 node-local 空间的变换：`src/armature_view.cpp:299` `sd.append_vertices(data.transform, *data.vertices)`；`data.transform` 来自 slot 的 display 矩阵（`src/slot.cpp:355-368`，`display->transform = matrix` 在 `:367`，含 `_textureScale`），`data` 由 `src/armature.cpp:185-196` 生成：
  ```cpp
  const Transform2D global_transform = p_base_transfrom * transform;   // :190
  for (const Slot *raw_slot : armature_instance->getSlots()) {         // :191
      const Slot_GD *slot = static_cast<const Slot_GD *>(raw_slot);
      if (auto display = slot->get_display())                          // :193
          display->append_draw_data(r_data, global_transform, slot->color * p_modulate); // :194
  }
  ```
  并由 `src/mesh_display.cpp:91` `r_data.add_data(slot->_zOrder, p_base_transfrom * transform, …)` 写入 `ArmatureDrawData`。
  → **结论：`ArmatureDrawData` 的顶点经 `data.transform` 后即位于 View 的本地空间（= CanvasItem local），与引擎 `_edit_get_rect()` 期望的空间一致** `[可证]`（对比 3.4）。
- `[推断]` `Display::transform`（`src/mesh_display.h:50`）对 `DragonBonesArmature` 实例没有赋值点：`src/*.cpp` 全量赋值穷举只有 `src/mesh_display.cpp:42`（重置为 identity）与 `src/slot.cpp:367`（slot 的 display），`src/*.h` 无 `transform =`；`src/debug_draw.cpp:737` 只是读取 `p_armature->transform`。→ 根 armature 的该字段实践中恒为 identity（嵌套骨架的累积由 slot display 链完成）。

### 3.3 `src/debug_draw.*` 现有绘制能力（可作为“实际轮廓”先例）

- `[可证]` 结构：`src/debug_draw.h:43-44`（`Flag`：`DRAW_MESH` / `DRAW_BONE` / `DRAW_BONE_NAME`）、`:75`（`draw(DragonBonesArmature*, const ArmatureDrawData&)`）、`:83/:86/:89`（`RID canvas_bones` / `mesh_bones` / `mesh_wireframe`）、`:91`（默认 flags）；仅 `DEBUG_ENABLED` 编译。
- `[可证]` `src/debug_draw.cpp:645` 起 `DebugDraw::draw`：
  - `:649-718` 网格线框：遍历 `ArmatureDrawData` 的所有 layer/data，顶点按 `data.transform.xform(...)`（`:685`）落到 View 本地空间，三角拆线段（`line_indices.resize` `:698`、循环 `:699-716`），`:717` `mesh_add_surface_from_arrays(mesh_wireframe, PRIMITIVE_LINES, arr)`，`:718` 直接画到 **owner 的 canvas item**（不挂材质）；
  - `:724-800+` 骨骼：`p_root_armature->for_each_armature_recursively(...)`，`global_transform = global_transform * p_armature->transform`（`:737`），每根骨 `global_transform * bone->get_global_transform()`（`:744`）、长度取 `bone->get_length()`；`:784` 提交 `mesh_bones` 表面；`:788-794` 单独创建子 canvas item `canvas_bones` 并挂骨骼材质（描边/调色板 shader，shader 文本见 `:126-170`），`:797` 清空该层、`:802` 提交；`:804-812` 收尾（`DRAW_BONE_NAME` 分支）；
  - 文件末尾 `#endif // DEBUG_ENABLED`。
- `[可证]` 即：**“按当前姿态画真实轮廓/线框”在仓库里已有实现与材质方案**，但受 `DEBUG_ENABLED` 门控，且与 debug 开关/颜色等属性耦合。

### 3.4 引擎侧坐标空间对照（确认“同空间可直用”）

- `[可证]` 4.3-stable `editor/plugins/canvas_item_editor_plugin.cpp:3493-3508`：选中框 = `Transform2D xform = transform * ci->get_global_transform_with_canvas();`（紧随其后）后映射 `ci->_edit_get_rect()` 的四角；其中 `transform` 为 2D 编辑器画布的 canvas transform。
  → `_edit_get_rect()` 的语义是 **CanvasItem 本地空间**，与 3.2 的 draw-data 空间一致（View 的 mesh 以 identity 提交到自己的 canvas item，`:722`）。

### 3.5 编辑器插件现状：有无 viewport 叠加绘制钩子

- `[可证]` 现有编辑器类：`src/editor/dragon_bones_editor_plugin.h:43`（`DragonBonesExportPlugin : EditorExportPlugin`）、`:59`（`DragonBonesImportPlugin : EditorImportPlugin`）、`:83`（`DragonBonesEditorPlugin : EditorPlugin`）；注册于 `src/dragon_bones_registration.cpp:61-70`（`GDREGISTER_INTERNAL_CLASS` ×4，`:68` `EditorPlugins::add_by_type<DragonBonesEditorPlugin>()`；注意规范写的 `:61`/`:91` 已与当前树不一致）。
- `[可证]` `DragonBonesEditorPlugin` 只实现 `_enter_tree`（`src/editor/dragon_bones_editor_plugin.cpp:265-279`）与 `_exit_tree`（`:281-288`）：注册 import/export 插件、监听 `filesystem_changed`、初始化 `Godot_DragonBones/auto_generate_dbdata` 设置。
- `[可证]` **没有任何 viewport 叠加绘制钩子**：对 `src/`（`*.cpp`,`*.h`）grep `_forward_canvas|forward_canvas` 为空；无 `_forward_canvas_draw_over_viewport` / `update_overlays` 调用。

### 3.6 “按当前姿态动态计算显示内容边界”所需数据可否得到

| 需要的数据 | 可得性 | 锚点 |
|---|---|---|
| 当前姿态几何（顶点 + 到 View 本地的变换） | ✅ 已有：`ArmatureDrawData::Data{transform, vertices, indices, colors, vertices_uv, texture_rid, blend_mode, z_order}`，`Layer` 按 z-order 分组 | `src/armature_draw_data.h:52`（`struct Data`）、`:67`（`class Layer`）、`:135`（`add_data`） |
| 每帧收集点 | ✅ `_draw()` 内已在做（scratch `draw_data`） | `src/armature_view.cpp:682-683` |
| 可见 slot/display 过滤 | ✅ 由 runtime 决定：`armature_instance->getSlots()` + `slot->get_display()`；slot 可见性 API `getVisible()` | `src/armature.cpp:191-195`；`thirdparty/dragonBones/armature/Armature.h:354`；`thirdparty/dragonBones/armature/Slot.h:299`；`src/slot.h:57`（`Slot_GD::get_display`） |
| 嵌套（骨骼挂载）armature | ✅ 递归遍历已有 | `src/armature.h:146-167`（`for_each_armature_recursively`）、`:112`（`for_each_child_armature`） |
| 骨骼数据（长度/全局变换） | ✅ | `src/bone.h:64`（`get_length`）、`:77-78`、`:90-91`（`get_global_transform`）；`src/bone.cpp:180-186`（`to_gd_transform(boneData->globalTransformMatrix)`） |
| 与 `_edit_get_rect()` 同空间 | ✅ 见 3.2 / 3.4 | — |
| 编辑器（非运行）下姿态是否已推进 | ⚠️ `[未验证]`：`rebuild_armature()`（`:315-336`）内会 `armature->force_update()`，但后续是否持续推进取决于 `active`（`:364-368`）、`callback_mode_process`（`:535-542`）与 `_notification` 的 `NOTIFICATION_INTERNAL_PROCESS` / `NOTIFICATION_INTERNAL_PHYSICS_PROCESS`（`:658-674`）是否被触发（`advance` 内联于 `src/armature_view.h:119-123`）。编辑器内是否每帧推进需实测 | `src/armature_view.cpp:315-336`、`:535-542`、`:658-674`；`src/armature_view.h:119-123` |
| 实例翻转/缩放对边界的影响 | `[推断]` 会体现在 draw data 顶点（`flip_x/flip_y` 是 armature 实例状态、由 slot 的 `globalTransformMatrix` 体现）；而静态 `aabb` 不含 → 用静态 aabb 做选框在翻转/换姿态后必然不准 | `src/armature_view.cpp:1004-1014`（静态 aabb）+ `src/slot.cpp:355-368`（实例矩阵） |

---

## Q4 `.trellis/spec/` 中与“新增编辑器期行为”相关的强制约定

### `.trellis/spec/editor/index.md`

- `[可证]` 新增编辑器代码**必须**在声明与定义两侧用 `#ifdef TOOLS_ENABLED` 包裹，`#endif` 后写 `// TOOLS_ENABLED` 注释；代码若必须在导出游戏中运行则不属于该层。
- `[可证]` `template_release` 必须仍可构建：`SConstruct:94-96`（`env.debug_features` 为假时不追加 `TOOLS_ENABLED`、不编译 `src/editor/*.cpp`）。
- `[可证]` 注册/反注册必须对称（规范引用 `src/dragon_bones_registration.cpp:61` vs `:91`；当前树 `add_by_type` 在 `:68`，反注册在 `_exit_tree`（同文件 `:281-288` 的类实现内）—— 引用时以实际行为准）。
- `[可证]` 编辑器文件不得解引用 `try_import` 等可能失败的返回值（规范 Known Defects 有现存反例）。

### `.trellis/spec/editor/editor-plugin.md`

- `[可证]` 三类编辑器类与角色（导出/导入/`EditorPlugin` 装配）；注册为 internal：`GDREGISTER_INTERNAL_CLASS`（`src/dragon_bones_registration.cpp:61-70`）。
- `[可证]` `TOOLS_ENABLED` 门控范式与 `src/editor/` 编译门（同 index.md）。
- `[可证]` 导入管线契约（`DragonBonesImportPlugin::try_import`）与资源路径规则（`convert_to_imported_path`）；项目设置键名 `Godot_DragonBones/auto_generate_dbdata` 一旦改名即迁移。
- `[可证]` 已知缺陷清单（例如 `src/editor/dragon_bones_editor_plugin.cpp:275-291` 的 key 遮蔽、`:286` 的泄漏）**不得作为新代码范例**。

### `.trellis/spec/gdextension/index.md`

- `[可证]` 两层分离：Godot wrapper（`GDCLASS`）与 runtime 子类（`Slot_GD` 等，不得加 `GDCLASS`）。
- `[可证]` 注册等级现状：`DragonBonesArmatureView` 为 `GDREGISTER_CLASS`（可实例化）、`DragonBonesArmature/Bone/Slot/...` 为 `GDREGISTER_ABSTRACT_CLASS`（`src/dragon_bones_registration.cpp:74-84`；规范写 `:56-76`，已过期）。
- `[可证]` 每个新增公开方法要么在 `_bind_methods()` 绑定、要么在 PR 注明为内部；**每个新增绑定成员必须在 `doc_classes/<Class>.xml` 有对应条目**（setter 名要写实际绑定名）。
- `[可证]` 只允许三类 `#ifdef`：`TOOLS_ENABLED`、`DEBUG_ENABLED`、Godot 版本守卫；禁止手写平台守卫。
- `[可证]` 进入 runtime 指针前必须判空（参考 `src/armature_view.cpp:657-802` 的转发族）。

### `.trellis/spec/gdextension/quality-guidelines.md`

- `[可证]` 支持范围：`compatibility_minimum = 4.3`，声明于 `demo/addons/godot_dragon_bones.daylily-zeleen/godot_dragon_bones.gdextension:26`（同文件 `:3` `version = "v0.0.0"`，由发布流程替换；**没有** `compatibility_maximum`）；新 API 必须在 4.3→current 全区间可编译，若真需要更高版本则同 PR 提升 `compatibility_minimum`。
- `[可证]` 现存唯一版本条件代码：`TypedDictionary` 守卫（`src/armature.h:43-52`）。
- `[可证]` 仓库**无自动化测试**：验证 = 编译（`scons platform=<host> target=template_debug debug_symbols=yes`，必要时 Web）+ 手动跑 demo 并说明平台。
- `[可证]` 格式：`clang-format`（tab 缩进、`ColumnLimit: 0`），只格式化改动行；PR 标题/正文中文。
- `[可证]` Known Tech Debt 表可用于引用既有问题（含 `get_global_rect` inverse 一项，行号已过期）。

---

## 可选机制枚举（仅列事实与可行性，不构成设计）

| 机制 | 可用性（本地可证） | 能达到的效果 | 主要代价/风险 |
|---|---|---|---|
| **M1 View 自绘轮廓** | `_draw` 是 GDVIRTUAL、godot-cpp 已暴露（`gen/.../canvas_item.hpp:177/:184`）；仓库已有 DebugDraw 先例（`src/debug_draw.cpp:645+`） | 视觉上贴合实际显示内容（线框/轮廓/包围框） | 引擎的选中框、缩放手柄、吸附、点击命中仍按 `_edit_*` 默认值（`editor/plugins/canvas_item_editor_plugin.cpp:3493-3517` / `:638`），选中体验不变 |
| **M2 EditorPlugin 叠加层** | `_forward_canvas_draw_over_viewport(Control*)`/`_forward_canvas_force_draw_over_viewport` + `update_overlays()` 均已在 godot-cpp 绑定（`gen/.../editor_plugin.hpp:119-121,:146-151,:105`）；引擎投递链 `editor/plugins/editor_plugin_list.cpp:65-74` | 可在 2D 视口叠加任意轮廓/框（含多选、随选择切换） | 不改引擎选框语义；需自行处理 canvas transform、选择变更、刷新时机；绘制层级顺序 `[未验证]`；需 `TOOLS_ENABLED` 门控（规范强制） |
| **M3 让 `_edit_*` 变 GDVIRTUAL** | `[可证]` 当前（4.3–4.8.dev）全部是普通 C++ 虚函数，GDExtension 无钩子（见 Q1 文档第 3 节） | 唯一能真正改变引擎选中框/手柄/吸附的路径 | 需要改引擎（上游 PR / 自定义引擎构建），本仓库范围内不可行 |
| **M4 动态边界 API + 呈现（M1/M2）** | 所需数据已齐（见 3.6：draw data 每帧可得、空间一致） | “边界值”可复用给自绘/叠加/脚本/工具 | 公开 API 需同步 `doc_classes/*.xml`（规范强制）；与现有静态 `get_rect()/get_global_rect()` 的语义冲突需要兼容决策（含规范已记录 inverse 的既有问题）；需处理“编辑器内是否已推进姿态” `[未验证]` |

> 结论性事实（供规划取舍）：**在不改引擎的前提下，无法改变 2D 编辑器内置选框本身**；能做的只有“自己画出贴合内容的轮廓/框”（节点自绘或编辑器插件叠加），以及把“当前姿态边界”作为可复用数据暴露出来。

---

## 仍不确定 / 需人工确认

1. `[未验证]` DragonBones Pro 导出 `aabb` 的确切算法（是否全动画帧并集、是否含 canvas 尺寸），本地无工具端实现。
2. `[未验证]` 编辑器（非运行态）中 View 的姿态是否每帧推进、`data` 未设置时 armature 是否存在（影响“在编辑器里算当前边界”的可行性）。
3. `[未验证]` `_forward_canvas_draw_over_viewport` 叠加层与内置选中框/手柄的绘制顺序与遮挡关系（需实测）。
4. `[未验证]` 4.4+ 的 `DEBUG_ENABLED` 运行期 `_edit_get_rect` 消费者（4.7 `canvas_item.h:277-278` 注释所暗示的运行时节点选择）具体实现。
5. `[未验证]` `Rect2` 经带旋转/负缩放变换后的归一化需求（现有 `get_rect()/get_global_rect()` 未处理）。
6. `[未验证]` 叠加层绘制若改用 `CanvasItem::draw_*`（已绑定，`gen/.../canvas_item.hpp` 列表中含 `draw_line/draw_polyline/draw_rect` 等）在 `Control*` overlay 上的调用约束（是否必须在引擎的 canvas 绘制阶段内调用）——需实测。
