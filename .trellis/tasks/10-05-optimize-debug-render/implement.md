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
