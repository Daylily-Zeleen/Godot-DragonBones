# 技术设计：调试绘制与渲染的每帧分配与 IK 归属优化

> 本文件正文用中文记述设计；代码标识符保持原文。

## 现状（已核实，含行号）

### R1 — `bone_data` 每帧重建

`DebugDraw::draw` 每帧（每次 `_draw`）构造
`LocalVector<DebugBone> bone_data`（`src/debug_draw.cpp:526`），遍历全部嵌套 armature
的骨骼填入，随后用于生成几何与名称。`DebugBone` 含 `Vector2 / float / StringName / Kind`，
`StringName` 是引用计数，填入时需构造（字符串到 `StringName` 的转换在入表时完成）。

同函数内还有 `DebugDrawGeometry geometry`（三个 `LocalVector`）每帧新建。

### R2 — IK 归属错误（真实缺陷）

`cache_ik_bones()`（`src/debug_draw.cpp:609-627`）用
`for_each_armature_recursively` 遍历全部嵌套 armature，把每个约束的
`target` 名字推进 `ik_targets`、`root`/`bone` 名字推进 `ik_driven`——**完全是扁平的
`LocalVector<StringName>`**（`src/debug_draw.h:77-78`），不记录来自哪个 armature。

绘制时（`src/debug_draw.cpp:546-548`）只拿 `bone_name` 与这两份表做 `.has()` 匹配：

```cpp
if (ik_targets.has(bone_name)) { kind = KIND_IK_TARGET; }
else if (ik_driven.has(bone_name)) { kind = KIND_IK_DRIVEN; }
```

**缺陷**：`DragonBonesArmature::get_bones()` 返回 `std::map<StringName, Ref<DragonBonesBone>>`，
键是**每个 armature 内部的骨骼名**。嵌套时，子 armature 的骨骼名与父 armature 的完全
可以相同（各自独立的骨骼表）。若 A 的 `hand` 是 IK 约束骨、B 的 `hand` 是普通骨，
则两份表里都有 `hand`，遍历 B 时会命中 A 写入的条目 → B 的 `hand` 被错误标成 IK。

另外 `ik_driven` 会把同一个名字重复 `push_back`（约束的 `root` 与 `bone`、跨多个约束都
可能重名），无去重。

### R3 — `_draw` 的每帧分配

`src/armature_view.cpp:392`：

```cpp
DrawData draw_data; // TODO: 避免每帧重建
armature->append_draw_data(draw_data);
```

`DrawData`（`src/mesh_display.h:43-95`）持 `LocalVector<Layer> layers`；
`Layer` 持 `LocalVector<Data> data`；`Data` 含 4 个 `Packed*Array`
（`vertices/indices/colors/vertices_uv`）+ `Transform2D` + `RID` + `Color`。

每帧分配点：

1. `DrawData` 的 `layers` 容器本身（首次 `operator[]` 时 `insert` 会分配）。
2. 每个 `Layer::data` 容器（`push_back` 增长；`LocalVector` 不释放，析构才释放）。
3. 每个 `Data` 里 4 个 `Packed*Array` 的缓冲（`append_draw_data` 时按值拷贝填入，
   `DragonBonesMeshDisplay::append_draw_data` 见 `src/mesh_display.cpp:88-100`）。
4. `_draw` 内的 `std::vector<SurfaceData>` / `std::vector<Surfaces> meshes`
   （`src/armature_view.cpp:400-440`），以及每个 `SurfaceData` 的 4 个 `Packed*Array`。
5. 最后每 surface 一个 `Array arr`（`arr.resize(ARRAY_MAX)`）—— `Array` 是 `Variant`
   数组，会分配。

`DragonBonesMeshDisplay` 本身的 `vertices/indices/colors/vertices_uv` 已经是池化对象的
成员（`src/mesh_display.h:132-135`，对象来自 `from_pool()`），**这部分已具备复用条件**；
真正每帧新增的是 `DrawData` 里的**拷贝**。

## 设计决策

### D1（R1）— `bone_data` 提升为成员并复用

**决定：提升为 `DebugDraw` 成员，每帧 `clear()` 后复用容量。**

理由：骨骼数量与名称是稳定的（由骨架数据决定，不随动画变化），
`LocalVector::clear()` 只将 size 置 0、**保留已分配容量**，因此稳态下零分配。
`StringName` 元素被覆盖时引用计数就地更新，无额外分配。

与 R3 的 `DrawData` 复用同一思路，且 `DebugBone` 只在 `DEBUG_ENABLED` 下存在，
release 构建零代价。

风险：`bone_data` 的生命周期跨帧后，若某帧骨骼数变少，旧元素仍留在容量里——
但 `clear()` 后 `size` 正确，遍历用 `range-for` 只走有效元素，安全。
名称绘制在 `draw()` 末尾同步消费，不跨帧持有。

### D2（R2）— 归属判定改为 (armature, bone_name) 对

**决定：把 `ik_targets` / `ik_driven` 改为键为 `DragonBonesArmature *` 的映射，
或等价地在遍历时按当前 armature 就地收集。**

候选方案：

| 方案 | 做法 | 取舍 |
|---|---|---|
| A. `HashMap<DragonBonesArmature *, HashSet<StringName>>` | 缓存时按 armature 分桶 | 需要 armature 指针作键；`cached` 失效时机需覆盖 armature 重建（已有 `clear_cache()` 钩子） |
| B. 每帧遍历时按 armature 即时查询约束 | 不做全局缓存，遍历到某 armature 时现场扫它的 `_constraints` | 无缓存失效问题，实现最简；代价是每帧扫约束（数量极少，通常个位数） |
| C. 把归属写进 `DebugBone` 时用 armature 作用域局部表 | 遍历进入某 armature 前先算出该 armature 的局部名字集合 | 同 B，但更显式 |

**倾向 B/C**：`_constraints` 每个 armature 通常只有 0~2 个（IK 约束本就稀疏），
每帧现场扫一遍的成本远低于维护一份带作用域的缓存及其失效逻辑。
且 B 天然解决「去重」——现场判定用 `HashSet` 或直接按约束条目比对，不再累积重复项。

若实测约束数量可观（例如某骨架几十个），退回 A：`ik_targets` 改为
`HashMap<const DragonBonesArmature *, HashSet<StringName>>`，键用 armature 指针，
在 `clear_cache()` 里整体清空（该钩子已在 `rebuild_armature()` 中调用，
`src/armature_view.cpp:66-68`）。

**必须消掉的语义**：裸名字 `.has()` 匹配。代之以「该骨骼所属 armature 的约束集合」。

### D3（R3）— `DrawData` 成员化 + 就地清空复用

**决定：把 `DrawData` 提升为 `DragonBonesArmatureView` 成员（`DrawData draw_data_cache`），
每帧 `clear()` 后复用；`SurfaceData`/`meshes` 同样提升为成员或改为按内容就地填充。**

`DrawData` 需要一个 `clear()`：把 `layers` 里的每个 `Layer::data.clear()`，
`layers.clear()`（保留容量）。注意 `LocalVector::clear()` 保留容量，正合需要；
但 `Layer` 是值类型、`layers.clear()` 不释放内层 `data` 的容量，所以要先逐层 `data.clear()`
再 `layers.clear()`，否则容量随层消失。

`Data` 内 4 个 `Packed*Array`：复用同一 `Data` 元素时用 `resize(0)` 而非重新构造，
并用 `ptrw()` 就地写入（现有代码是 `push_back({...})` 按值构造，改为取引用后填充）。

`SurfaceData` / `meshes`：这两个是 `_draw` 局部的临时聚合结构。可提升为成员
（`LocalVector<LocalVector<SurfaceData>>`）并每帧清空复用；`surface_data->indices` 等
同样 `resize(0)` 复用。

`Array arr`：每个 surface 构造一次。可用成员 `Array` 复用（`arr[RenderingServer::ARRAY_INDEX] = ...`
逐项赋值即可覆盖），但 `Array` 存 `Variant`，赋值 `Packed*Array` 时仍会包一层。
需实测收益后决定是否值得——**列为可选**，不阻塞主线。

**验证一致性的关键**：R3 是纯内存复用，渲染结果必须逐像素不变。
用同一场景同一帧位置截图，优化前后比对 PNG。

## 边界与契约

- `DrawData` 成员必须在 `rebuild_armature()` 时失效（armature 换新，旧层结构可能不适用），
  复用 `clear_cache()` 钩子或在 `rebuild_armature()` 里显式清空。
- `DebugDraw` 的成员 `bone_data` 在 `set_enabled(false)` / 析构时清空，避免持有一份
  指向已释放 armature 名字的 `StringName`（`StringName` 本身是全局驻留字符串，安全，
  但仍应清空以释放容量）。
- 不得改变 `DrawData::operator[]` 的语义（有序插入、同 z-order 归并到同层）。
- 不引入裸 `new`/`delete`；`RID` 释放且仅释放一次（见 memory spec）。

## 回滚

三块互相独立，可各自单独回滚（R1 / R2 / R3 分别落在 `debug_draw.*` 与 `armature_view.*`）。
R2 是缺陷修复，优先做且必须保留；R1、R3 是纯优化，若引入回归可直接还原对应 hunk。
