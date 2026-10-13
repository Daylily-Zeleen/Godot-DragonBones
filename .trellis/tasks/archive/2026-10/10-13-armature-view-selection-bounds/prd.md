# ArmatureView 编辑器选中范围贴合实际显示内容

## Goal

编辑器中选中 `DragonBonesArmatureView` 时，提供贴合「当前实际显示内容」的范围提示（随姿态更新的包围盒）；同时把编辑操作体感对齐普通 Sprite2D：点击显示区域即选中、按住显示内容即可拖动移动。

用户动机：当前选中该节点时画布上没有任何范围提示（引擎只画原点位置图标），无法判断节点的实际可视范围；且期望提示与「实际显示内容」一致。

## Background（已确认事实）

引擎现状（4.3 / 4.4 / 4.5 / 4.6 / master 逐一核对源码）：

- `CanvasItem::_edit_use_rect()` 默认 `false`、`_edit_get_rect()` 默认空矩形 → 引擎不会为该节点绘制任何范围框。
- 选中 `use_rect()==false` 的节点时，`CanvasItemEditor::_draw_selection()` 只绘制 `EditorPosition` 位置图标（4.3 与 master 行为一致）。
- 该组编辑器钩子（`_edit_get_rect` / `_edit_use_rect` / `_edit_is_selected_on_click` 等）在 4.3~master 均为**纯 C++ virtual，未 GDVIRTUAL 化**，godot-cpp 绑定中亦不存在（全仓 grep 无匹配）→ **GDExtension 无法覆盖，节点自身无法影响引擎绘制/命中选择**。上游亦有相关提案（godotengine/godot-proposals#5289），实际绑定未见暴露。
- 点击命中选择同样走 `_edit_is_selected_on_click`：`use_rect()==false` 时判定为「距原点 < 容差」，即当前只有点击原点附近才能选中（`scene/main/canvas_item.cpp:59-63`；4.3 `editor/plugins/canvas_item_editor_plugin.cpp` `_find_canvas_items_at_pos`）。

可达机制（本仓库 + godot-cpp 绑定核对）：

- 引擎 `EditorPlugin` 提供 `GDVIRTUAL1(_forward_canvas_draw_over_viewport, Control *)`（master `editor_plugin.h:123`；4.3 `canvas_item_editor_plugin.cpp:3925` 在 2D 画布合成阶段调用），godot-cpp 已生成对应可覆盖虚函数（固定版 `gen/include/godot_cpp/classes/editor_plugin.hpp:119`，`register_virtuals` 自动 `BIND_VIRTUAL_METHOD`）→ **可通过现有 `DragonBonesEditorPlugin`（`src/editor/dragon_bones_editor_plugin.h`）叠加绘制范围提示**。
- 节点渲染：`_draw()` 把龙骨网格以 identity transform 提交到自身 canvas item（`src/armature_view.cpp:676-736`）→ 网格顶点即节点局部空间坐标；节点局部空间的内容范围可由 `ArmatureData::aabb`（骨架 JSON 的静态 aabb × armature scale，`src/armature.cpp:227-236`；demo 全部骨架均含该字段）或当前姿态的运行时绘制数据（`ArmatureDrawData` 顶点/索引）计算。
- 仓库已有 debug 绘制基础设施（`src/debug_draw.*`：网格线框/骨骼），取数方式可参考（其本身为 DEBUG_ENABLED 运行时特性，编辑器侧需新的取数入口）。

## Requirements

- **R0（必须，核心验收）**：在编辑器 2D 画布中，于 `DragonBonesArmatureView` 的实际显示区域内点击（含远离原点的位置）即选中该节点；**按住显示内容拖动即整体移动节点（像 Sprite2D 一样），释放时整段提交撤销/重做**；点击画布空白处不误选；带修饰键（Shift/Ctrl/Alt）点击保留引擎原生语义。多选时按住任一选中实例拖动可整体移动。
- **R1（必须）**：选中一个或多个 `DragonBonesArmatureView` 时，在 2D 画布上绘制贴合**当前实际显示内容**的范围包围盒；内容为空/未加载/无数据时不绘制且不报错。
- **R2（已否决）**：曾考虑以各部件「实际轮廓」替代整体矩形；经评审与实测，逐部件轮廓视觉噪杂（龙骨由数十个重叠部件构成），明确**不实现**，选中范围统一为整体包围盒。
- **R3（编辑器期专属）**：不改变运行时行为、导出内容与公开 API 语义；绘制仅在编辑器启用；无选中时不产生绘制与逐帧轮询开销。
- **R4（一致性）**：支持范围内（4.3 起）编辑器行为一致；视觉贴合编辑器主题（选中色），与引擎默认位置图标共存、不干扰其他节点。

## Acceptance Criteria

1. demo 工程中选中挂有数据的 ArmatureView：画布出现贴合显示内容的范围（R1）；播放/切换动画、手动改姿态后范围随之更新；移动/旋转/缩放节点后仍贴合。
2. 未挂数据、数据加载失败的节点：选中后不出现错误绘制与报错。
3. 多选多个 ArmatureView / 与其他节点混合多选：各 ArmatureView 独立正确绘制。
4. 4.3 与当前支持的最新版本编辑器行为一致（同一场景目视核对）。
5. 取消选中/禁用插件后叠加层立即消失，无残留。
6. 编辑器无选中时无额外每帧开销（应以事件驱动 `update_overlays()` 等刷新，不做无条件逐帧轮询）。

## Out of Scope

- 修改/隐藏引擎默认位置图标、接管 F 聚焦等引擎内部行为（GDExtension 不可达）。
- 缩放/旋转手柄与网格吸附：引擎仅对声明 rect 的节点提供手柄（不可达）；自定义手柄+吸附属后续增强，本期不做。缩放/旋转仍可在检查器修改。
- 运行时（游戏内）选中/范围相关任何行为。

## Open Questions

（无阻塞项；R2 轮廓形态与实现取舍见 design.md。）
