# 调试绘制与渲染的每帧分配与 IK 归属优化

> 本文件正文用中文记述需求；代码标识符保持原文。

## 背景

`feat/debug-bone-draw`（已重命名为 `optimize/redering`）落地了骨骼调试绘制，
review 时留下三处待评估的问题：

1. `src/debug_draw.cpp:526` 的 `TODO` 注释（`bone_data` 是否应作为成员变量缓存）。
2. `DebugDraw::ik_targets` / `ik_driven` 用裸骨骼名匹配，嵌套 armature 下可能归属错误。
3. `DragonBonesArmatureView::_draw` 里 `DrawData draw_data;` 每帧重建，存在大量分配。

## 需求

### R1 — 评估 `debug_draw.cpp:526` 的 TODO

该 TODO 问「`bone_data` 是否作为成员变量进行缓存比较好」。评估并给出结论：
缓存与否、依据是什么、若有收益如何落地。

### R2 — 修正嵌套 armature 下 IK 骨骼的归属

`cache_ik_bones()` 把所有嵌套 armature 的 IK 约束汇总成两份**扁平的名字表**
（`ik_targets` / `ik_driven`），绘制时只用**裸骨骼名**匹配（`debug_draw.cpp:546-548`）。
armature 会嵌套，不同 armature 的骨骼可以重名，因此存在误判：同名骨骼在 A 里是
IK 约束骨、在 B 里是普通骨。必须让归属判定基于「骨骼所属 armature + 骨骼名」而非裸名字。

### R3 — 消除 `_draw` 的每帧分配

`_draw` 每帧新建 `DrawData`（内含 `LocalVector<Layer>`，每个 `Layer` 内含
`LocalVector<Data>`，每个 `Data` 内含 4 个 `Packed*Array`），以及临时的
`std::vector<Surfaces>` / `SurfaceData`。评估能否：
- 把 `DrawData` 提升为 `DragonBonesArmatureView` 成员并在每帧复用；
- 对 `DrawData::Layer` 或 `LocalVector<Data>` 做池化；
- 消除 `SurfaceData`/`meshes` 的每帧分配。

## 验收标准

- R1：给出书面结论（含数据或代码依据），若判定应缓存则实现，否则在 TODO 处更新注释说明理由。
- R2：重名骨骼分属不同 armature 时，每根骨骼的 IK 归属按其所属 armature 判定；
  构造一个嵌套重名场景验证（子 armature 的骨骼名与父 armature 相同但角色不同）。
- R3：连续两帧绘制不再重新分配 `DrawData` 的 `Layer`/`Data` 容器与其中的
  `Packed*Array`；`_draw` 不引入每帧新的堆分配（`RenderingServer` 内部调用除外）。
  行为与优化前逐像素一致（同一帧渲染结果不变）。
- 优化前后 `template_debug` / `template_release` 均构建通过。

## 约束

- **不改变渲染结果**：R3 是纯粹的内存复用，像素输出必须一致（可用帧截图逐像素比对）。
- 遵守 `.trellis/spec/gdextension/memory-and-lifetime.md`：不引入裸 `new`/`delete`；
  `RID` 释放且仅释放一次；`Ref<>` 成员在 teardown 清理。
- 不引入第二条绘制链路（现有 `RenderingServer` + `mesh_add_surface_from_arrays` 风格）。
- 调试绘制代码保持 `#ifdef DEBUG_ENABLED` 隔离，release 构建不为其付出代价。
