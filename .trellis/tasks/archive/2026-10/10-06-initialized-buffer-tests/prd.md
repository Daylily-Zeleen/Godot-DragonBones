# 抽取 InitializedBuffer 单头文件并接入 doctest 单元测试

## Goal

1. 把 `internal::InitializedBuffer` 从 `src/armature_draw_data.h` 抽为独立单头文件。
2. 审查其实现，修掉已发现的缺陷，并用**入库的 doctest 单元测试**固化行为。
3. 建立项目的 C++ 单测基础设施：`tests/` 目录 + `tests` 构建开关（SCons/CMake 双构建系统）+ 运行时 `--gddb-run-tests` 入口。
4. 评估 R1（`debug_draw` 几何缓冲）与 R3（`armature_view` 的 `SurfaceData`）能否改用 `InitializedBuffer`。

价值：`InitializedBuffer` 是手写容器（自管 `memrealloc` + 原地构造），没有测试即无安全网；R1/R3 的缓冲复用若统一到它，可减少三套并行实现。

## Background

- `thirdparty/doctest/` 已由用户加入（`doctest.h` + `LICENSE.txt` + `patches/`），当前**未被 git 跟踪**。
- 参考实现：`D:\Dev\godot\GodotJS-Ext`（`thirdparty/doctest`、`src/*/tests/`、SConstruct `tests` 选项、`register_types` 里的 `try_run()`、`--jsb-run-tests`）。本任务采用同样的分层，命令名改为 `--gddb-run-tests`。
- 项目当前**没有任何测试基础设施**：SConstruct 无 test target，`src/` 无测试，CMake 无测试分支。
- 项目的 C++ 标准为 C++17（`CMakeLists.txt:132`）。
- 两个构建系统必须同步（`.trellis/spec/build/build-systems.md` 的硬性约定）。
- `InitializedBuffer` 现被 `ArmatureDrawData::Layer` 与 `ArmatureDrawData::layers` 使用。

## 设计语义（用户明确，测试的判定基准）

`InitializedBuffer` 的存在理由与 `LocalVector` 的本质区别：

- **`[0, capacity)` 整个缓冲区恒为已构造的有效对象**；`count` 只是逻辑用量游标。
- 因此 `clear()` / `resize()` **有意不析构**被移出逻辑范围的元素 —— 它们仍是有效对象，可被后续写入复用。这正是它取代 `LocalVector` 的原因（`LocalVector::clear/resize` 会析构或释放）。
- 只有 `reset()` 释放整块缓冲时，才析构 `[0, capacity)`。

任何「`clear()`/`resize()` 应当析构」的判定都与该语义冲突，属误判。

## 已确认缺陷（`g++ -std=c++17` 实测，非推理）

复刻实现并用独立探针（`.agent_tmp/ib/`）逐项验证：

| ID | 缺陷 | 位置 | 实测证据 |
|----|------|------|----------|
| **D1** | `resize()` 声明返回 `U`，函数体**无 return** | `armature_draw_data.h:82-87` | 调用 `resize(3)` 后 `count==3` 正常，但返回值是未定义值；当前无调用点使用返回值故未暴露 |
| **D2** | `ArmatureDrawData::get_capacity_bytes()` 把 `layer.get_capacity_bytes()`（量纲已是字节）**再乘一次 `sizeof(Data)`** | `armature_draw_data.h:248-254` | 读码确认：单 layer 真实占用 = `cap*sizeof(Data)+sizeof(int)`，现被放大 `sizeof(Data)` 倍（约 80×） |
| **D3** | `insert(p_at, ...)` **对 `p_at` 无越界检查**；`bytes = (size() - p_at) * sizeof(T)` 在 `p_at > count` 时无符号下溢 | `armature_draw_data.h:117-137` | `insert(5, ...)` 于 `count==3` → **进程崩溃 rc=5** |
| **D4** | `reset()` 释放缓冲、置 `capacity=0`，但**不归零 `count`** | `armature_draw_data.h:106-118` | `reset()` 后 `size()` 仍返回 3；后续 `push_back` 会以陈旧 `count` 驱动 `reserve(count+1)`，可致逻辑错乱 |
| **D5** | `ArmatureDrawData::end_frame()` 为空，其注释描述的行为已不存在 | `armature_draw_data.h:283-285` | 注释与实现不符 → **删除该注释**（用户指定） |

> 已证伪的两条早期误判，记录以免复述：
> - `insert()` 的 `buff` 取出-拷回往返**不是缺陷**：实测 `insert(1)` 得 `[0,99,1,2,3]`，结果正确。
> - `clear()`/`resize()` 不析构**不是缺陷**：是设计语义本身（见上）。
> 唯一与「缓冲区全有效」模型相关的实质问题是不变量 2（`p_at` 越界）与不变量 4（`reset` 后状态）。

## Requirements

- R1 抽取 `InitializedBuffer` 到独立头文件（如 `src/initialized_buffer.h`），命名空间保持 `godot::internal`；`armature_draw_data.h` 改为包含它。
- R2 修复 D1–D5；`insert()` 增加 `p_at <= count` 的越界检查（`CRASH_BAD_INDEX(p_at, count + 1)`，允许尾插）。
- R3 新增 `tests/` 目录，用 doctest 编写 `InitializedBuffer` 的单元测试，覆盖：默认构造、`reserve`（增长、不缩、既有元素保持）、`resize`（扩/缩/**缩后缓冲槽仍有效**）、`clear`（保留容量、不析构）、`reset`（归还内存且 `count` 归零）、`push_back`（`const&`/`&&`/initializer 三种重载）、`insert`（首/中/尾/空表/**越界**）、`operator[]`（含越界崩溃路径）、`is_empty/size/get_capacity/get_capacity_bytes`、以及析构释放。
- R4 为 `ArmatureDrawData::get_capacity_bytes()` 补测试（含 D2 的回归断言：真实字节数而非被放大值）。
- R5 SCons 增加 `tests` 布尔选项（默认 `False`）；开启时定义宏、编译 `tests/*.cpp`、加入 doctest 头文件路径。
- R6 CMake 增加等价能力（一个 `option`/缓存变量 + 条件源文件与 include）。
- R7 `register_types.cpp` 在模块初始化时检测命令行 `--gddb-run-tests`，命中则运行 doctest 并以测试结果作为退出码。
- R8 评估 R1（`debug_draw.cpp` 的 `DebugDrawGeometry` / 骨骼 scratch）与 R3（`armature_view.cpp` 的 `SurfaceData`）能否改用 `InitializedBuffer`，给出**结论 + 依据**（不改代码，除结论为「应当改」且代价可控时才纳入本任务）。
- R9 更新文档：`.trellis/spec/build/build-systems.md` 记录 `tests` 开关与运行方式；`README.md` / `README.zh.md` 若涉及构建命令则同步。

## Acceptance Criteria

- [x] AC1 `src/initialized_buffer.h` 存在；`armature_draw_data.h` 不再内联定义该模板；两个 `template_debug` / `template_release` 构建均通过且产物行为不变（demo 冒烟无新错误）。
- [x] AC2 D1–D5 全部修复；测试中建有对应回归断言（D2 容量精确值、D3 越界、D1 返回值类型、D4 `reset` 后 `count`、D5 注释删除）。
- [x] AC3 `scons ... tests=yes` 可构建出带测试的库；`scons ... tests=no`（默认）不编译 `tests/`、不定义测试宏。
- [x] AC4 CMake 配置 + 构建在 `-DGODOT_DRAGONBONES_TESTS=ON` 下编译 `tests/`，在默认 OFF 下不编译。
- [x] AC5 `Godot_v4.3... --path demo --gddb-run-tests` 打印 doctest 汇总并以 0 退出；注入一个失败断言时以非 0 退出。
- [x] AC6 测试覆盖 R3 列出的每个 API 与边界；`--gddb-run-tests` 全绿。
- [x] AC7 R8 产出书面结论（含 R1/R3 各自的判断与理由），落在任务目录或 spec 中。
- [x] AC8 `thirdparty/doctest/` 被纳入版本控制（或明确记录为 git submodule/外部依赖的获取方式）。

## Out of Scope

- 不把 `DragonBonesMeshDisplay` / `Packed*Array` 相关路径重写为 `InitializedBuffer`（除非 R8 判定收益明确且用户批准）。
- 不引入 doctest 之外的测试框架、不引入 CI 改动。
- 不修改 `demo/` 下任何既有文件。
- 不做发布/打包流程改造。

## 决策记录

- D1–D5 的修复**纳入本任务**一并完成（用户已确认），先写测试、再修复，以「修复前断言失败」作为测试有效性的证据。
