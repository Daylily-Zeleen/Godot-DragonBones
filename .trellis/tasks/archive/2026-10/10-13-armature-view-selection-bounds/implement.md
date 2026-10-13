# 实现计划

## 顺序清单

1. **机制 spike（先行验证）**：在 `DragonBonesEditorPlugin` 加 `_forward_canvas_draw_over_viewport` 最小实现（固定矩形）+ `_enter_tree` 触发 `update_overlays()`；编辑器实测：能画出、层序在引擎选框之上、`update_overlays()` 生效。不通过则按 design「回退预案」调整后再继续。
2. **View 侧取数**：`src/armature_view.h/.cpp` 新增 `TOOLS_ENABLED` 内部方法（未绑定）：当前显示内容包围盒 + 边界边折线组；复用 `append_draw_data` 收集路径；核对嵌套骨架覆盖，必要时递归合并。
3. **插件侧**：
   - 选择监听：`EditorSelection::selection_changed` 维护「选中 ArmatureView 集合」；
   - 节点 `draw` 信号连接/断开（ObjectID 安全）；
   - `_forward_canvas_draw_over_viewport`：逐选中节点绘制 R1 框 + R2 边界折线（编辑器选中色，1px）；
   - 所有事件 → `update_overlays()`；全部 `#ifdef TOOLS_ENABLED` 门控。
4. **边界情形**：无数据/加载失败跳过；节点释放断开；插件退出清理连接。
5. **验证**（仓库无自动化测试，规范定义 = 编译 + 手动 demo）：
   - `scons platform=windows target=template_debug debug_symbols=yes`
   - `scons platform=windows target=template_release`（确认 TOOLS 符号不泄漏）
   - 手测清单（下）
6. **文档**：若最终新增任何绑定成员 → 同步 `doc_classes/*.xml`（设计上应避免）；PR 说明变更与已知限制。

## 手测清单（编辑器）

- [x] 选中 Dragon / Dragon_scattered / Sheep / 龙 四组 demo 资源节点：框贴合显示内容（自动化 + 人工）。
- [x] 播放/scrub/切换动画：框随姿态更新。
- [x] 移动/旋转/缩放（含负缩放）、zoom in/out：始终贴合、无错位。
- [x] 与 Sprite2D 引擎选框对齐校验（相同变换下）。
- [x] 多选：两个 ArmatureView + 一个 Sprite2D → 各画各的。
- [x] 空数据/加载失败节点选中：无绘制、无报错。
- [x] 取消选中 / 禁用插件：叠加层消失、无残留。
- [ ] 4.3-stable 与本地引擎仓（4.8.dev）各实测一遍（4.8.dev 已测；4.3 待人工）。
- [x] 点击显示区域选中（远离原点）/空白反选/一步拖动（自动化注入输入流验证）。
- [ ] 未选中时无逐帧开销（帧耗对比）。

## 危险文件 / 回滚点

- `src/armature_view.h/.cpp`：渲染主路径文件，新增内容严格限定在 TOOLS 块内；
- `src/editor/dragon_bones_editor_plugin.h/.cpp`：与既有 filesystem 监听逻辑共存，不改既有行为；
- 回滚：单提交 revert；无格式/状态迁移。

## start 前复核

- [ ] `implement.jsonl` / `check.jsonl` 已策展（spec + research 条目）；
- [ ] prd/design/implement 一致；用户已批准最终规划摘要。
