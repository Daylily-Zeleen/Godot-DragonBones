# 设计：编辑器选中范围贴合实际显示内容

## 边界与机制结论

- **不可达**：引擎内置选中框/手柄/吸附/点击命中基于 `CanvasItem::_edit_*`（4.3-stable~4.8.dev 均为普通 C++ 虚函数，无 GDVIRTUAL、不在 extension API dump；编辑器侧亦无脚本回落）。**不改引擎则无法替换内置选框**。证据：`research/edit-rect-hooks.md`。
- **采用 M2：EditorPlugin 叠加层**。载体 = 现有 `DragonBonesEditorPlugin`（`src/editor/dragon_bones_editor_plugin.h:83`），覆盖 `_forward_canvas_draw_over_viewport(Control*)`（godot-cpp 已绑定：`gen/include/godot_cpp/classes/editor_plugin.hpp:119`，`:146-151` 自动 `BIND_VIRTUAL_METHOD`），刷新用 `update_overlays()`（同文件 `:105`）。
  - 不选 M1（节点自绘）：无法可靠获知“选中态”，且绘制语义混入场景内容。
  - 弱点（接受并为 Out of Scope）：引擎默认位置图标仍在；框选/点击命中语义不变。
- **不改引擎内置行为**（M3 需上游改动，本仓库范围不可行）。

## 数据与坐标契约

- **数据源 = 与渲染同源的「当前姿态绘制数据」`ArmatureDrawData`**：其顶点经 `data.transform` 后已位于 View 本地空间（`src/armature_view.cpp:722` identity 提交网格；`research/selection-bounds.md` 3.2/3.4 证明与 `_edit_get_rect()` 期望空间一致）。
- **取数入口**：`DragonBonesArmatureView` 新增 `#ifdef TOOLS_ENABLED` **内部方法** `Rect2 get_editor_selection_rect() const`（不绑定脚本 → 无需 `doc_classes/*.xml`）。复用 `draw_scratch.draw_data`（与 `_draw()` 同一收集路径）；空数据/无效 armature 返回空 `Rect2`，调用方以 `size.is_zero_approx()` 判定跳过。
- **选中范围** = 全部绘制数据顶点经 `data.transform` 变换后的 AABB（节点本地空间）。
- **节点局部 → 叠加层空间**：实现期以前置实验确定（参照引擎 ciep 4.3:3494 `transform * get_global_transform_with_canvas()` 的组合思路）；验收采用「与 Sprite2D 引擎选框对齐」目视校验。
- **多选**：遍历 `EditorInterface::get_selection()->get_selected_nodes()` 过滤本类；各自独立绘制。

## 刷新模型（R3/R6：无选中零开销）

- 事件驱动 `update_overlays()`：
  - `EditorSelection::selection_changed` → 重建监听集合 + 刷新；
  - 监听集合内节点 `CanvasItem::draw` 信号（姿态推进/数据重建会触发重绘）→ 刷新；以 ObjectID 校验维护、随选择变化与节点释放断开。
- **不使用** `_forward_canvas_force_draw_over_viewport`（避免无条件逐帧画布重绘）。回退预案：若实测 `draw` 信号在编辑器不触发，则仅在「存在选中的本类节点」时启用 force 变体（有选中才产生逐帧成本），并在 PR 中记录。
- 绘制层级：叠加层投递于引擎 `_draw_selection()` 之后（ciep 4.3 `:3918` vs `:3925`）→ 预期覆盖在内置选框之上；列入实测校验。

## 兼容与门控

- 全部新代码 `#ifdef TOOLS_ENABLED`（声明、定义、`#endif // TOOLS_ENABLED`）；`template_release` 不含任何新符号（`SConstruct:94-96`）。
- 4.3~current 全区间可编译：所用 API 4.3 即有，无新增版本守卫。
- 不新增/不改绑定 API；不动既有 `get_rect()/get_global_rect()`（inverse 语义为既有技术债，不在本任务扩大范围）。
- 改动不涉链接/源码 glob 变化 → 常规构建矩阵即可（仍跑宿主 debug/release 两档）。

## 风险与缓解

| 风险 | 缓解 |
|---|---|
| 编辑器内姿态推进时机未知（`research/selection-bounds.md` Q3.6 [未验证]）→ 范围框可能不随预览更新 | draw 信号驱动 + demo 实测（已验证随姿态更新）；不行则文档化限制或启用 force 回退预案 |
| xform 组合错误 → 叠加层错位 | 与 Sprite2D 选框对齐校验；zoom/旋转/负缩放下实测 |
| 与引擎手柄/图标视觉冲突 | 视觉校验；选中色 + 1px 线宽 |
| `update_overlays()` 链路在编辑器未生效 | 实现第 1 步做最小 spike（画固定框）先行验证 |

## 回滚

单特性提交，回滚 = revert；运行时行为零变化（全 TOOLS 门控）。
