# R8 评估：R1 / R3 能否改用 `InitializedBuffer`

> 结论先行：**R1 的 `DebugDrawGeometry` 可以改，但收益边际、风险不小，建议不改**；
> **R3 的 `SurfaceData` 不能改**（设计上被 GDExtension 边界类型约束）。

## 判据

`InitializedBuffer` 的语义（见 `src/initialized_buffer.h` 顶部注释）：

- `[0, capacity)` 全部是已构造有效对象，`count` 仅逻辑游标；
- `clear()`/`resize()` 不析构、不释放；只有 `reset()` 归还内存；
- 写入走 `push_back(Func&&)`（就地初始化）或 `push_back(T&&/const T&)`；
- `insert()` 用 `memmove` 搬移 → 要求 `T` **可平凡搬移**。

## R1：`DebugDrawGeometry`（`src/debug_draw.cpp:171-210`）

现状：4 个 `LocalVector`，元素类型 `Vector2` / `uint32_t` / `int32_t`，全部平凡构造/析构/可搬移。
使用方式（`body_tri`，`debug_draw.cpp:215-232`）：只做 `push_back` 追加 + `clear()` 复位。

| 维度 | 评估 |
|---|---|
| 语义可替换性 | **可以**。`LocalVector::clear()` 对平凡析构类型就是 `count = 0`（保留容量），与 `InitializedBuffer::clear()` 一致；`reset()` 亦对应 |
| 可平凡搬移约束 | 满足（`Vector2`/整型） |
| 收益 | **边际**。`InitializedBuffer::push_back(Func&&)` 可省掉 `LocalVector::push_back(T&&)` 的临时对象构造，但对 8/4 字节平凡类型几乎无差别。真正的开销在 `_draw` 的 `mesh_add_surface_from_arrays`（见 R3 的测量），不在这里 |
| 风险 | **高**。该路径的顶点值此前做过**逐 bit 等价**验证（三角优化提交 `62756de`）；改容器会触碰这段高度敏感、已优化过的代码路径，一个搬移/清空语义差异就可能让输出位模式变化 |
| 建议 | **不改**。收益不足以覆盖回归风险。若将来 R1 需要新增「就地构造」能力（避免临时对象确有实测收益），再单独评估 |

补充：`BoneScratch` 的 `LocalVector<DebugBone>` / `LocalVector<StringName>` **不应改**：
`StringName` 非平凡析构，`InitializedBuffer` 的「缓冲区全有效」模型要求 `memnew_arr_placement`
对**每个** `reserve` 出来的槽真正构造 `StringName`（`LocalVector` 对非平凡类型也一样要构造），
两者成本相同，但 `InitializedBuffer` 还多了「整块缓冲区语义」的维护负担，换过去只是徒增复杂度。

## R3：`SurfaceData`（`src/armature_view.cpp:70-205`）

现状：4 个 `Packed*Array`（`PackedInt32Array`/`PackedVector2Array`/`PackedColorArray`/`PackedVector2Array`）
+ `n_*` 游标，写入走 `ptrw()` 就地覆写，提交时直接放进 `Array` 交给 `RenderingServer`。

| 维度 | 评估 |
|---|---|
| 语义可替换性 | **不能**。`SurfaceData` 的终点是 `mesh_add_surface_from_arrays(mesh, PRIMITIVE_TRIANGLES, arr)`，其中 `arr[ARRAY_*]` 必须是 `Variant` 能承载的 `Packed*Array` |
| 若强行替换 | 就得在提交前把 `InitializedBuffer` 转成 `Packed*Array` → **多一次整块拷贝**（当前是零拷贝：`Packed*Array` 的 `ptrw()` 直接写在最终缓冲上，放进 `Array` 只是一次引用计数） |
| 可平凡搬移约束 | `Packed*Array` 含 `CowData`，**非平凡搬移**；`InitializedBuffer::insert` 的 `memmove` 对它不安全 |
| 建议 | **不改**。这是设计约束，不是优化机会 |

同理 `DrawScratch::surfaces`（`LocalVector<SurfaceData>`）与 `mesh_surfaces` 也不改 —— 元素含
`Packed*Array`（非平凡析构 + 非平凡搬移）。

## 结论

| 对象 | 可改？ | 决定 |
|---|---|---|
| R1 `DebugDrawGeometry` | 技术上可以 | **不改**（收益边际、触碰逐 bit 验证过的路径，风险 > 收益） |
| R1 `BoneScratch`（bones / ik_*） | 技术上可以但无意义 | **不改** |
| R3 `SurfaceData` | **不可以** | 不改（GDExtension 边界类型约束） |
| R3 `surfaces` / `mesh_surfaces` | 不可以（元素非平凡搬移） | 不改 |

即：`InitializedBuffer` 的适用面是「**纯扩展侧、元素平凡可搬移、只做追加/清空**」的场景。
当前代码库里这类缓冲已经由 `ArmatureDrawData` 的两个 `InitializedBuffer` 实例覆盖；
R1/R3 的其余缓冲要么受益边际、要么语义不兼容。**本任务不为 R1/R3 引入改动。**
