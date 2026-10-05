# 调试模式下绘制锥状骨骼与骨骼名称

> 本文件正文用中文记述需求；代码标识符保持原文。

## 背景

`DragonBonesArmatureView::debug` 开启后，当前仅把各 slot 的三角网格转成线框绘制
（`src/armature_view.cpp:427-489`），**不包含骨骼信息**。调试骨架动画时无法直观看到
骨骼层级、朝向和长度，只能凭 slot 线框推断。

## 需求

在调试模式下额外绘制骨骼：

1. **锥状（楔形）骨骼**：每根骨骼画成一个锥形——大端位于骨骼起点，小端位于骨骼终点。
   即沿骨骼方向由粗到细收窄。
2. **骨骼名称**：在每根骨骼旁边显示其名称。

## 验收标准

- `debug = true` 时，`_draw()` 除现有 slot 线框外，额外绘制全部骨骼的图形与名称。
- `debug = false` 时行为与现状完全一致，无额外绘制开销。
- 每根骨骼绘制为「枢轴圆环 + 圆心连线」，长骨（`length > 2.5 * radius`）额外绘制筝形：
  大端在骨骼起点、小端在骨骼终点，方向与骨骼朝向一致。
- 图形与名称均带描边；受 IK 约束的骨骼描边用另一色区分。
- 骨骼名称文字位置贴近对应骨骼，不与其他骨骼的名称严重重叠到不可读。
- 骨骼层级/位置随动画实时更新。
- 仅在 `DEBUG_ENABLED` 下编译，发布构建（`template_release`）不含此代码。
- 全平台 CI 构建通过。

## 现状调研（已确认）

| 项 | 位置 |
|---|---|
| 调试绘制入口 | `src/armature_view.cpp:427` `#ifdef DEBUG_ENABLED / if (debug)` |
| 调试 mesh 句柄 | `src/armature_view.h:71-73`，`RID debug_mesh`（仅 DEBUG_ENABLED） |
| 骨骼集合 | `DragonBonesArmature::bones`（`src/armature.h:71`，`std::map<std::string, Ref<DragonBonesBone>>`），经 `get_bones()` 暴露 |
| 骨骼全局变换 | `DragonBonesBone::get_global_transform()`（`src/bone.h:90`，已绑定 ClassDB） |
| 骨骼名 | `DragonBonesBone::get_name()`（`src/bone.h:64`） |
| **骨骼长度** | 上游 `dragonBones::BoneData::length` 存在（`thirdparty/dragonBones/model/ArmatureData.h:362`，由 `JSONDataParser.cpp:348` 从 `"length"` 解析并乘 armature scale），但 **`DragonBonesBone` 未暴露** |

## 已确认（调研结论）

- **嵌套 armature 会递归**。`Slot::getDisplay()` 在 slot 展示 armature 时返回子 armature
  的 display（`thirdparty/dragonBones/armature/Slot.cpp:299-300`），而
  `DragonBonesArmature` 继承 `Display` 并 override `append_draw_data`，
  故 `src/armature.cpp:191-192` 会进入子 armature。**骨骼绘制必须同样递归**。
  （此前一度误判为不递归，已纠正。）
- **绘制方式**：复用现有 `debug_mesh`，用 `mesh_add_surface_from_arrays` 追加 surface，
  保持与现有 slot 线框一致的 `RenderingServer` 风格。不新建 RID，也不用
  `draw_colored_polygon`（会引入第二条不一致的绘制链路）。
- **字体**：`ThemeDB::get_singleton()->get_fallback_font()`，不硬编码。
- **文字缩放**：最终采用固定像素字号（`DEBUG_BONE_NAME_FONT_SIZE`，基准 14px），
  不做缩放补偿——补偿逻辑一度实装，已在 `a4ed9b2` 删除。
- **短骨骼**：骨长不足 `PREFER_KITE_LENGTH_RATIO * radius`（2.5）时不画筝形，
  只保留枢轴圆环 + 圆心连线；没有单独的红色空心锥体路径。
- **锥体视觉**：描边 + 明暗梯度解决纯色平坦问题；颜色先沿用 `debug_color`。
