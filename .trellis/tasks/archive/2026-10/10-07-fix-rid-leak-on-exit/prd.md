# 修复退出时 Shader/Material RID 泄露，并让 CI 能检出资源泄露与异常崩溃

## Goal

消除进程退出时 `RenderingServer` 已销毁后仍析构 GDExtension 持有的静态材质所产生的 RID 泄露与引擎错误输出；并让 CI 能在出现资源泄露或异常崩溃时**失败**，而不是静默通过。

## Background（已确认事实，均经本机实测）

### 泄露现象（Windows x64，Godot 4.3-stable）

```
$ Godot_v4.3-stable_win64_console.exe --headless --editor --path demo --quit-after 60
ERROR: 1 RID allocations of type 'N13RendererDummy15MaterialStorage11DummyShaderE' were leaked at exit.
ERROR: Parameter "RenderingServer::get_singleton()" is null.
   at: ~Shader (scene/resources/shader.cpp:264)
ERROR: Parameter "RenderingServer::get_singleton()" is null.
   at: ~Material (scene/resources/material.cpp:173)
```

触发条件（实测）：

| 命令 | `leaked at exit` 命中数 |
|---|---|
| `--headless --path demo --quit-after 30` | 0 |
| `--headless --path demo res://dragonbones_demo/demo.tscn --quit-after 30` | 0 |
| `--headless --editor --path demo --quit-after 60` | 1 |
| `--headless --editor --path demo --quit-after 60`（清空编辑器恢复场景后） | 1 |
| `--headless --editor --path demo --quit-after 60`（临时禁用扩展） | 0（仅扩展缺失的解析错误） |

即：**仅编辑器模式且扩展已加载时复现**；用 Dummy 渲染驱动（`--headless`）即可，**不需要真实 GPU**，CI 可跑。

### 根因（已实测确认，完整证据链见 design.md）

`src/debug_draw.cpp:98-99` 的函数级静态 `Ref<ShaderMaterial>`：

```cpp
static Ref<ShaderMaterial> &get_bone_material() {
    static Ref<ShaderMaterial> material = [] { ... }();   // 仅此一处持有
    return material;
}
```

该静态量只在 **DLL 卸载 / CRT 静态析构**时才析构，此时 `RenderingServer` 已销毁 → 析构报 `get_singleton() is null`，其内部 RID 未释放 → `RID_Owner` 析构报 `leaked at exit`。

该静态材质**没有任何清理注册**（对比：`blend_materials` 已通过 `clear_static` 清理）。

**触发时机**：编辑器启动时检查器枚举 `debug_draw_*` 属性 → `get_color_ik_bone_outline()`（`debug_draw.cpp:545`）→ 惰性创建该材质。故 `--editor` 必现且**无需打开场景**；非编辑器 headless 不创建，故无泄露。

**定位过程（插桩反证）**：
1. 在既有 `clear_static`（SCENE 级 uninit，RS 仍存活）插桩，确认 `blend_materials` 已被正常清空 → 泄露依旧 ⇒ **排除** `blend_materials`。
2. 在同一时机额外释放该静态材质 → **所有 ERROR 完全消失** ⇒ 元凶锁定 `debug_draw.cpp:99`。

**关键时序**（`main/main.cpp`）：`uninitialize_modules(MODULE_INITIALIZATION_LEVEL_SCENE)`（`:5280`，`clean_static` 执行处，RS 仍存活）**早于** `finalize_display()`（`:5325`，RS 销毁）。故既有清理机制是可靠时机，**修复无需改动释放顺序或引入新钩子**。

> 注：本机 `D:/Dev/godot/godot` 引擎源码为 4.8-dev，与运行时 4.3-stable 二进制版本不同，引擎侧行号不可对照；结论以本机实测输出为准。

### CI 现状

`.github/workflows/build.yml` 的 `tests` job 已编译 `tests=yes` 扩展、下载 Godot、导入资源，并跑 doctest（122 断言，用 `grep -q "\[doctest\] Status: SUCCESS!"` 防假绿）。

**当前完全没有**：资源泄露检查、异常崩溃检查 —— 退出期的 `ERROR: ...leaked at exit` 或崩溃 backtrace 不会让 CI 失败。

## Requirements

- **R1** 消除退出时的 RID 泄露与 `RenderingServer::get_singleton() is null` 错误输出。
- **R2** CI 在**出现资源泄露**时失败。
- **R3** CI 在**出现异常崩溃**时失败（`CrashHandlerException` / `Program crashed` / signal 类输出）。
- **R4** CI 检查失败时**中止后续流程**，不继续跑后续 step/job 造成误导。
- **R5** R2–R4 的检查须先能在**未修复**的代码上真实变红（避免写出永不为红的检查），再修 R1 使其转绿。
- **R6** R1 修复不得引入新的泄露/崩溃；现有 122 个 doctest 断言与既有 `build` matrix 全部保持通过。
- **R7** 检查方式：**全量扫描 `ERROR:`**（用户决定，暂不设白名单；导入期噪声已在既有 `--import` step 处理，与测试无关）。

## Acceptance Criteria

- **AC1**（R1）`--headless --editor --path demo --quit-after N` **不出现**任何 `ERROR:`（含 `leaked at exit` 与 `get_singleton() is null`）。
- **AC2**（R1）`--headless --path demo` 与 `--headless --path demo res://dragonbones_demo/demo.tscn` 仍无 `ERROR:`。
- **AC3**（R2/R3）CI 新增的检查在人为注入泄露/崩溃时以非零退出。
- **AC4**（R4）该检查失败时后续步骤不被继续执行。
- **AC5**（R5）本 PR 先提交「CI 检查（未带修复）」，在 CI 上观察到真实失败；随后的修复提交使其转绿。
- **AC6**（R6）doctest 122 断言仍全绿；`build` matrix 不受影响。
- **AC7**（R7）检查为全量 `ERROR:` 扫描，无白名单。

## Out of Scope

- 不重构 `RenderingServer` / 引擎侧 `Material`/`Shader` 析构行为（属引擎侧设计）。
- 不处理与本泄露无关的既有 verbose 噪声（`Pages in use exist at exit` 等）。
- 不引入真实 GPU 相关 CI。
- 不改变 demo 的场景内容与资源。
