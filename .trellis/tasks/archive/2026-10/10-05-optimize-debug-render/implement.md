# 执行计划：调试绘制与渲染的每帧分配与 IK 归属优化

> 本文件正文用中文记述执行步骤；代码标识符保持原文。

## 前置

- 分支：`optimize/redering`
- 构建：
  ```bash
  scons -Q --silent platform=windows target=template_debug   arch=x86_64 -j8
  scons -Q --silent platform=windows target=template_release arch=x86_64 -j8
  ```
- 临时文件一律放 `.agent_tmp/`。

## 步骤

### 步骤 1（R1）— 评估并落地 `bone_data` 复用

1. 读 `src/debug_draw.cpp:518-600` 与 `src/debug_draw.h:60-100`，确认 `bone_data`
   与 `DebugDrawGeometry` 的用法。
2. 把 `LocalVector<DebugBone> bone_data` 提升为 `DebugDraw` 成员；
   `draw()` 内改为 `bone_data.clear()` 后 `push_back`。
3. 同法处理 `DebugDrawGeometry`（三个 `LocalVector`）——评估其是否也值得成员化。
4. 更新 `src/debug_draw.cpp:526` 的 TODO 注释为结论（为什么缓存 / 容量如何保留）。
5. `set_enabled(false)` 与析构中清空这些成员。

**验证**：构建通过；实机 demo 骨骼与名称显示不变。

### 步骤 2（R2）— 修正 IK 归属（缺陷修复，优先）

1. 读 `src/debug_draw.cpp:609-627`、`546-548`、`src/debug_draw.h:77-78`。
2. 按 design D2 选定方案（默认 B：遍历到某 armature 时现场扫其 `_constraints`；
   若实测约束多则用 A：`HashMap<const DragonBonesArmature *, HashSet<StringName>>`）。
3. 删除扁平的 `ik_targets` / `ik_driven`（或改为按 armature 分桶）。
   消除裸名字 `.has()` 匹配；顺带消除重复 `push_back`。
4. 保持 `clear_cache()` 的既有语义与调用点（`src/armature_view.cpp:66-68`）。

**验证**（关键）：构造嵌套重名场景——子 armature 与父 armature 各有一根同名骨骼，
其中一根受 IK 约束、另一根不受；确认两者描边颜色不同且归属正确。
用 demo 场景截图比对，或写一个最小复现场景。

### 步骤 3（R3）— `DrawData` 与 `SurfaceData` 复用

1. 读 `src/armature_view.cpp:386-500`、`src/mesh_display.h:43-95`、
   `src/mesh_display.cpp:73-100`。
2. 给 `DrawData` 加 `clear()`：逐 `Layer::data.clear()` → `layers.clear()`（保留容量）。
3. 把 `DrawData` 提升为 `DragonBonesArmatureView` 成员，`_draw` 内 `clear()` 后复用；
   在 `rebuild_armature()` 中一并清空。
4. `DragonBonesMeshDisplay::append_draw_data` 的 `push_back({...})` 改为
   「取 `back()` 引用 → `resize(0)` → `ptrw()` 就地填充」，避免按值构造 4 个 `Packed*Array`。
5. `SurfaceData` / `meshes` 提升为成员并复用（`resize(0)` 保留容量）。
6. 可选：复用 `Array arr`（每 surface 一次），实测收益后再定。

**验证**：构建通过；**同一场景优化前后截图逐像素一致**（PNG 比对，允许零差异）。

### 步骤 4 — 双目标构建与回归

1. `template_debug` / `template_release` 均构建通过。
2. 跑 demo，确认骨骼、名称、线框、IK 配色无回归。
3. 清理 `.agent_tmp/`。

## 验证命令

```bash
# 构建
scons -Q --silent platform=windows target=template_debug   arch=x86_64 -j8
scons -Q --silent platform=windows target=template_release arch=x86_64 -j8

# 跑 demo（Godot 4.3）
D:/Dev/godot/godot/bin/Godot_v4.3-stable_win64_console.exe --path demo
```

## 审查门

- 步骤 2 完成后：确认重名场景归属正确，再继续步骤 3。
- 步骤 3 完成后：确认逐像素一致，再提交。

## 回滚点

| 步骤 | 回滚 |
|---|---|
| 1 | 还原 `debug_draw.*` 中 `bone_data` 相关 hunk |
| 2 | **不回滚**（缺陷修复）；若方案 A 有问题则退到方案 B |
| 3 | 还原 `armature_view.*` / `mesh_display.*` 中复用相关 hunk |

---

## 执行记录

### R2（已完成）— IK 归属修正

**采用 design 的方案 B**：不做全局缓存，每进入一个 armature 就地收集它自己的 IK 约束。

改动：

- `src/debug_draw.cpp`：`draw()` 的遍历回调内，**先 `ik_scratch.clear()` 再
  `collect_ik_of_armature(p_armature)`**，然后才遍历该 armature 的骨骼。遍历结束即失效。
- `src/debug_draw.h`：`cache_ik_bones()` → `collect_ik_of_armature(DragonBonesArmature *)`
  （去掉内部的 `for_each_armature_recursively` 包装，只处理单个 armature）；
  删除 `clear_cache()` 声明与 `bool cached`（新方案无跨帧缓存，无需失效逻辑）。
- `src/armature_view.cpp`：删除 `rebuild_armature()` 中的 `debug_draw.clear_cache()` 调用块。

**命名**（review 反馈）：`ik_targets` / `ik_driven` 只是每帧复用的**暂态 scratch**，
不是跨帧缓存。聚为一个结构体并改名：

```cpp
struct IkScratch {
    LocalVector<StringName> targets; // 约束的 target 骨名
    LocalVector<StringName> driven;  // 被约束作用的 root / bone 骨名
    _FORCE_INLINE_ void clear() { targets.clear(); driven.clear(); }
};
IkScratch ik_scratch;
```

用 `scratch` 而非 `cache`：后者暗示跨帧有效性与失效逻辑，而这份数据每进一个 armature
就清空重填。`clear()` 收到结构体内，`targets`/`driven` 的绑定不变量不再靠调用方维持。

**缺陷证据**（fixture：一个 skeleton JSON 含两个 armature，`Parent` 的 `shared` 受 IK 约束，
`Child` 也有同名 `shared` 但为普通骨，经 Armature 类型 display 嵌套）：

| 判定方式 | Child 的 `shared` | Parent 的 `shared` |
|---|---|---|
| 旧逻辑（全局扁平汇总） | `IK_DRIVEN` ❌ 误判 | `IK_DRIVEN` ✓ |
| 新逻辑（按 armature 作用域） | `PLAIN` ✓ | `IK_DRIVEN` ✓ |

**回归**：真实 demo（`Dragon/Dragon_ske`）渲染无变化（73786 灰骨骼 + 40401 黑描边，
无残留色）。`template_debug` / `template_release` 均构建通过。

> 注：构建时 MSVC 偶发 `C1060 编译器堆空间不足`，降并行度（`-j3`）可过，与改动无关。

**fixture 位置**：`.agent_tmp/r2test/`（`nested_ske.json` + `nested.dbfactory`）。
不在 `demo/` 内，需要复现时拷入 demo 工程再跑。

### R1（已完成）— bone_data / geometry 复用（thread_local scratch）

**方案演进**：初版把 `bone_data` / `geometry` 提升为 `DebugDraw` 成员，后被否决——
绘制由 `CanvasItem::_draw()` 驱动，同线程串行，一份缓冲即可；成员方案会让每个
`DragonBonesArmatureView` 各持一份同规模缓冲，纯属浪费。改为 **cpp 内
`static thread_local`**（用 `thread_local` 而非裸 `static`，防某平台跨线程触发 `_draw`
时串数据）。

**改动**：

- `src/debug_draw.h`：净删 16 行。移除 `ik_scratch` 成员与 `collect_ik_of_armature`
  声明。头文件不再需要 `LocalVector` / `StringName`，`DebugDraw` 只剩
  `owner`、`mesh_wireframe`、`canvas_bones`、`mesh_bones`、`draw_flags`。
- `src/debug_draw.cpp`（匿名命名空间内，`DebugDrawGeometry` 之后）新增：
  ```cpp
  struct BoneScratch {
      LocalVector<DebugBone> bones;
      DebugDrawGeometry geometry;
      LocalVector<StringName> ik_targets; // 约束的 target 骨名
      LocalVector<StringName> ik_driven;  // 被约束作用的 root / bone 骨名
      _FORCE_INLINE_ void clear_bones() { bones.clear(); geometry.clear(); }
      _FORCE_INLINE_ void clear_ik() { ik_targets.clear(); ik_driven.clear(); }
  };
  static thread_local BoneScratch bone_scratch;
  ```
- `DebugDrawGeometry` 新增 `clear()`（四个 `LocalVector` 各自 `clear()`，只置 size=0、
  保留容量）。
- `collect_ik_of_armature` 由成员函数改为匿名命名空间内的文件局部函数，签名加两个
  出参：`void collect_ik_of_armature(DragonBonesArmature *, LocalVector<StringName> &r_targets, LocalVector<StringName> &r_driven)`。
- `draw()` 骨骼段：`bone_scratch.clear_bones()` → `LocalVector<DebugBone> &bone_data =
  bone_scratch.bones`；回调内 `bone_scratch.clear_ik()` + `collect_ik_of_armature(p_armature,
  bone_scratch.ik_targets, bone_scratch.ik_driven)`；几何改为 `DebugDrawGeometry &geometry =
  bone_scratch.geometry`。删除旧的 `// TODO: 是否作为成员变量进行缓存比较好？`。

**等价性验证**（同进程 A/B）：同一帧内，用复用 `bone_scratch` 与「每帧全新局部对象」
各建一份骨骼清单 + 几何，逐元素比较 name / kind / start / dir / length 及
vertices / vertex_uv / indices / colors。真实 demo（`龙`，60 根骨）：

```
PROBE R1 frame=N ok=1 bones=60 v=15744   （201 帧全部 ok=1，0 帧 ok=0）
```

**性能**（同进程 A/B，取 401 帧最小值，跳过前 20 帧预热）：

| 路径 | 耗时 |
|---|---|
| 复用 thread_local scratch | **95.6 µs** |
| 每帧全新局部对象 | 145.0 µs |

即 **−34%**（纯采集 + 几何段）。与改造前成员的实测（126.8 → 90.1 µs，−29%）量级一致。

`template_debug` / `template_release` 均构建通过；探针已移除，冒烟运行无 PROBE 输出、
无新增错误/断言。

> 注：`template_release` 用 `-j3` 仍偶发 MSVC `C1060 编译器堆空间不足`，
> 降为 `-j1` 可过，与改动无关。

### R3（已完成）— _draw 的每帧分配消除

**核心约束**（实测自 godot-cpp 源码）：

- `LocalVector::clear()` → `resize(0)`：**只置 size=0、保留缓冲**，但会析构元素。
- `LocalVector<Layer>::clear()` 会析构 `Layer`，连同其 `data` 的容量一起丢弃。
- `Packed*Array::resize(0)` 走 `CowData::_unref()`：**直接释放缓冲**（与非零缩小不同，
  非零缩小会走 `smaller_capacity` 保留大部分容量）。

因此"池化 Layer/Data"不能靠 `clear()`，必须**保留对象本体 + 帧内写入游标**。

**改动**：

1. `src/mesh_display.h` / `DrawData`：
   - `Layer` 增加 `used` 游标；`begin_frame()` 只复位游标，不析构任何元素；
   - `operator[]` 变为 `add_data(...)`：在 `used` 位置就地覆写已有 `Data`（复用其
     `Packed*Array` 缓冲），仅当本帧条目多于上帧时才 `push_back`；
   - `end_frame()` 把各层 `data.resize(used)` 截到实际用量、丢弃空层。
2. `src/mesh_display.cpp`：`append_draw_data` 改用 `add_data`。
3. `src/armature_view.h` / `SurfaceData`：改为成员缓冲池 + `n_*` 计数器。
   `Packed*Array` 缩到 0 会释放缓冲，故用量绝不能由 `size()` 表示；数组只增不减，
   帧末 `finish()` 截到 `n_*`（仍在容量内）。`add_vertices/add_colors/add_uv/add_indices`
   就地写 `ptrw()`。
4. `src/armature_view.{h,cpp}`：`DrawData` 提升为成员 `draw_data_cache`；
   表面组装抽为 `build_surfaces()`，用 `surface_pool`（只增不减）+ `mesh_surfaces`
   （每 mesh 的 surface 下标分组）替代原先每帧新建的
   `std::vector<SurfaceData>` / `std::vector<Surfaces>`。

**等价性验证**（同进程 A/B，跨进程比对因环境噪声不可用）：

- 真实 demo（`Dragon/Dragon_ske`，多 slot）：`PROBE3 reusematch=SAME v=435 i=1455`，
  逐元素比较顶点/索引/颜色/UV/纹理/混合模式，跨帧稳定。
- 合成场景覆盖**换 mesh 与换 surface 分支**（3 个纹理、MIX/ADD 交替）：
  `PROBE4 meshes=3 reusematch=SAME`，分组与内容均与旧逻辑一致
  （`m0` 三个表面、`m1` 两个、`m2` 一个）。
- 探针与临时 include 均已移除。

`template_debug` / `template_release` 均构建通过。

#### R3 追加：scratch thread_local 化 + 阈值释放

**动机**：R3 的 `draw_data_cache` / `surface_pool` / `mesh_surfaces` 原是
`DragonBonesArmatureView` 的**每实例成员**。但 `_draw()` 由 `CanvasItem` 驱动、同线程
串行，一份缓冲即够；每实例成员意味着多 view 时各持一份同规模缓冲（纯内存浪费）。
改为 **cpp 内 `static thread_local DrawScratch draw_scratch`**，与 R1 的
`bone_scratch` 同一套做法（`thread_local` 而非裸 `static`，防跨线程触发 `_draw`
时串数据）。

**改动**：

- `src/armature_view.h`：净删 `draw_data_cache` / `SurfaceData` / `surface_pool` /
  `mesh_surfaces` / `used_meshes` 成员块（~121 行），头文件只剩 `LocalVector<RID> draw_meshes`。
- `src/armature_view.cpp`：新增文件级 `struct DrawScratch`（含 `DrawData draw_data`、
  内嵌 `SurfaceData`、`surface_pool`、`mesh_surfaces`、`used_meshes`、`get_capacity_bytes()`、
  `try_reset()`）+ `static thread_local DrawScratch draw_scratch`。
  `build_surfaces()` 由成员改为文件局部函数并前向声明，全部引用改 `draw_scratch.`，
  lambda 捕获由 `[this]` 改 `[]`。
- `src/mesh_display.h`：`DrawData` 加 `void reset() { layers.reset(); }`（因
  `operator=` 被 delete，`draw_data = DrawData()` 不可用）。
- `~DragonBonesArmatureView()` 开头 `draw_scratch.try_reset();`。

**阈值释放**：`try_reset()` 阈值 `8u << 17` = 1 MiB，按 `surface_pool` 各数组
`size()` 之和判定；超阈才 `reset` 全部缓冲，否则保留。理由同 R1：scratch 跨 view
共享，单帧低利用率不代表该缩，否则多 view 交替绘制会退化成每帧反复分配/释放。

**等价性验证**（thread_local 版）：

- 真实 demo（`龙`，60 骨）：`PROBE EQ f=900 eq=1 meshes=1/1 v=435`，
  逐元素比对 indices / vertices / uv / colors / texture / blend，900 帧全 `eq=1`。
- 合成 3 分支（同 mesh 同 blend / 同 mesh 换 blend / 换 texture）：
  `PROBE SYN meshes=2 surf=3`；`m0 s0 v=7 i=7 blend=0`（3+4 累积）、
  `m0 s1 v=5 i=5 blend=1(ADD)`、`m1 s0 v=6 i=6 blend=0` —— 与 R3 原版验证结果逐项一致。

**性能**（同进程 A/B）：

| 版本 | `_draw` 耗时 |
|---|---|
| HEAD（无 R3） | 81.8 µs |
| R3（每实例成员） | 76.5 µs（best） |
| R3 + thread_local | avg 258~276 µs / static 对照 249~277 µs（同进程内无差异） |

> 说明：`thread_local` 化**不改热路径**（仍是同一份 scratch 被复用），实测与
> `static` 对照在噪声内持平（本机 `_draw` 抖动 ±20%，故用「跳前 200 帧后求均值」而非
> best 作跨版本比较；R3 的 76.5 µs best 与本节均值不同基准，不可直接相减）。
> **thread_local 化的收益是内存**（多 view 不再各持一份缓冲），不是单 view 性能。

**性能修正（同进程交替 A/B，调试绘制关闭）**：先前「−6.5%」是**跨进程 best-of**、
噪声主导，不可信。把 HEAD 的 `_draw` 与 R3 的 `_draw` 编进同一 DLL、逐帧交替执行、
各累计 400 帧求均值：

| 轮次 | HEAD（旧） | R3（新） | 降幅 |
|---|---|---|---|
| run1 | 256.2 µs | 217.9 µs | −15.0% |
| run2 | 258.8 µs | 222.6 µs | −14.0% |
| run3 | 242.6 µs | 197.6 µs | −18.5% |

即 **−14~18%**（而非 −6.5%）。与 R1（−34%）的差异来自**可优化占比**：R1 的路径
全是引擎外纯 C++（骨几何构建），几乎整体可优化；R3 的可优化部分只占约一半。

**R3 剩余耗时分解**（新 `_draw` 各段，400 帧均值，步条 435 顶点 / 1 surface）：

| 段 | 耗时 | 说明 |
|---|---|---|
| collect | ~35 µs | `append_draw_data` 取样条顶点/变换 |
| build | ~12 µs | `build_surfaces`（R3 优化对象） |
| clear | ~15 µs | `mesh_clear` × 1 |
| arr 组装 | ~4 µs | `Array` 填 4 个元素 |
| **addsurf** | **~67 µs** | `mesh_add_surface_from_arrays` |
| `mesh_surface_set_material` | ~4 µs（干净时） | |
| `canvas_item_add_mesh` | ~3 µs | |

> 上表 `arr` + `addsurf` + `mat` + `canvas` 加总 ≈ 78 µs，比「旧 vs 新」A/B 差
> （~40 µs）偏大：探针自带计时代价会放大各段（尤其 `mat` 在有额外探针时读到 18~27 µs，
> 无探针时应为个位数）。比值可信，绝对值以 A/B 差为准。

**addsurf 为何无法在扩展内规避（同格式缩放实验）**：用**同一格式**（VERTEX/COLOR/TEX_UV/
INDEX）分别提交 435 顶点与 **43 顶点（1/10）**：

| 顶点数 | addsurf |
|---|---|
| 435 | ~68 µs |
| 43 | ~17.5 µs |

差 51 µs / 差 392 顶点 ⇒ **约 130 ns/顶点**。说明这 67 µs 主体是**逐顶点的字节打包**
（引擎 `RenderingServer::_surface_set_data` 把 `Packed*Array` 解成 `Vector<uint8_t>`：
分配 3~4 个 `Vector<uint8_t>` + 每顶点 memcpy），只有约 10~17 µs 是定长固定开销。

**因此真正的瓶颈不是「`Packed*Array` 跨界」，而是**：引擎 API 只接受 `Array`，并在内部
把每个顶点重新打包成字节缓冲 —— 扩展这边无法绕过，除非改用完全不同的 API
（`mesh_surface_get_format` / RD 直写，均不现实）。**放弃**「Packed 直写（取消 `Array`
组装）」：`arr` 组装本身仅 ~4 µs，真正的 67 µs 全在引擎内部，改扩展侧无用。

这解释了 R3 天花板：可优化的（build 12 + clear 15 ≈ 27 µs）对比不可优化的引擎内
（addsurf 67 ≈ 全是逐顶点打包）与必要采集（collect 35），故只有十几到二十个百分点。

`template_debug` / `template_release` 均构建通过；探针与 `<chrono>` / `<vector>` /
`<tuple>` 临时 include 已移除（`grep -c "PROBE\|chrono"` 为 0）；冒烟运行无新增错误/
断言；`demo/` 未触碰。

### R3 追加二 — `DrawData` 抽独立头文件并改名 `ArmatureDrawData`

`DrawData` 已具规模（~100 行，独立于 `Display` 体系），从 `src/mesh_display.h` 抽出为
**`src/armature_draw_data.h`**（自带 include：`canvas_item_material` / `local_vector` /
各 `packed_*_array` / `rid` / `transform2d`），并全局改名 `DrawData` → `ArmatureDrawData`
（6 个文件）。`mesh_display.h` 改为 `#include "armature_draw_data.h"`。

改名动机：`DrawData` 过于泛化，且与 `DebugDraw` 的 `DebugDrawData` 之类易混；
`ArmatureDrawData` 指明其语义为「一个 armature 的绘制命令集」。

`template_debug` / `template_release` 均构建通过；冒烟无回归。

---

## 完成情况（归档前核对）

三条需求全部完成，代码已提交（`8fb4397` … `5da7af3`）：

- **R2**（`8fb4397`）：IK 骨骼归属改按 armature 作用域就地收集（`collect_ik_of_armature`），
  删除扁平的裸名字匹配与 `cached` 机制；嵌套重名场景验证归属正确。
- **R1**（`8db667a`、`74ab00a`）：`DebugDraw` 骨骼/几何缓冲改 cpp 内 `static thread_local BoneScratch`
  跨帧复用，超阈值（1 MiB）时释放；同进程 A/B 实测 −34%。
- **R3**（`6c62113` 起，后续多次迭代）：`DrawData`/`SurfaceData` 缓冲跨帧复用、
  thread_local 化、顶点色 RGBA8（ARRAY_CUSTOM0）、几何缓冲改 `LocalVector` 等；
  同进程 A/B 实测 `_draw` 约 −14~18%（后期进一步下降）。

验收标准逐条满足：R1 书面结论见本文件「R1」节；R2 有嵌套重名场景 fixture 与判定表；
R3 等价性经同进程 A/B 逐元素比对（后续以 bit 等价/截图 md5 一致复核）；
双模板构建通过。剩余未提交内容仅 `demo/`（用户既有改动，按约定不提交）。
