# 执行计划：InitializedBuffer 抽取、修复与 doctest 接入

> 每个步骤完成后停下汇报；构建与验证命令见文末。

## 前置

- 分支：`optimize/redering`（沿用当前分支；如需新分支由用户指定）。
- 参考实现：`D:\Dev\godot\GodotJS-Ext`（`thirdparty/doctest`、`src/*/tests/`、SConstruct `tests` 选项、`register_types` 的 `try_run()`）。
- 临时文件一律放 `.agent_tmp/`。
- **不得触碰 `demo/` 下任何既有文件**（除运行时不产生持久改动）。
- 构建必须 `-j1`（MSVC `C1060` 风险）。

## 步骤

### 步骤 1 — 抽取 `src/initialized_buffer.h`

1. 从 `src/armature_draw_data.h` 原样搬出 `namespace internal { template<...> class InitializedBuffer {...} }`。
2. 补头文件包含：`error_macros.hpp`、`memory.hpp`（`memrealloc`/`memfree`/`memnew_arr_placement`）、`type_traits`、以及 `MAX` 的来源（确认 `godot_cpp/core/math_defs.hpp` 或 `defs.hpp`）。
3. `armature_draw_data.h` 改为 `#include "initialized_buffer.h"`，删除内联定义。
4. 头文件顶部写清 5 条不变量（见 design §2）。
5. 双模板构建通过；demo 冒烟无新错误。

**验证**：`scons ... target=template_debug|template_release` 均通过；`--path demo` 冒烟。

### 步骤 2 — 修复 D1–D5

按 design §3 逐条修：
- D1 `resize()` 返回类型改 `void`。
- D2 `ArmatureDrawData::get_capacity_bytes()` 去掉多余的 `sizeof(Data) *`。
- D3 `insert()` 加 `CRASH_BAD_INDEX(p_at, count + 1)`（允许尾插）；**保留** `buff` 往返（实测正确）。
- D4 `reset()` 末尾补 `count = 0`。
- D5 删除 `ArmatureDrawData::end_frame()` 的过时注释（函数体保留为空）。

**不改**：`clear()` / `resize()` 的不析构行为 —— 这是设计语义，测试需断言「缩容后缓冲槽仍为有效可读对象」。

**验证**：双模板构建通过；demo 冒烟无新错误（D2 影响 `try_reset`，观察日志无异常）。

### 步骤 3 — 搭 doctest 测试基础设施（构建系统）

1. 新建 `tests/`：`test_main.cpp`（唯一 `DOCTEST_CONFIG_IMPLEMENT`）、`test_runner.h`（含 `try_run()`）。
2. SCons：加 `opts.Add(BoolVariable("tests", ...))`、`tests` 分支（宏 `GDDB_TESTS_ENABLED` + `Glob("tests/*.cpp")` + 必要 `CPPPATH`）。
3. CMake：加 `option(GODOT_DRAGONBONES_TESTS ...)` 与条件 `target_sources`/`target_compile_definitions`。
4. `register_types.cpp`：`#ifdef GDDB_TESTS_ENABLED` 下在 SCENE 初始化末尾调 `try_run()`。
5. `scons -n` 核对源列表：含 `tests/**`（`tests=yes` 时），**不含** `thirdparty/doctest/**`、仍不含 `thirdparty/godot-cpp/**`。

**验证**：
```bash
scons -Q --silent platform=windows target=template_debug arch=x86_64 -j1 tests=yes
scons -Q --silent platform=windows target=template_debug arch=x86_64 -j1   # 默认无测试
```
CMake（若本机 clang-cl 可用）：
```bash
cmake -S . -B build_tests -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DGODOT_DRAGONBONES_TESTS=ON
cmake --build build_tests
```

### 步骤 4 — 写 `InitializedBuffer` 单元测试

覆盖（对应 AC6）：
- 默认构造 / `is_empty` / `size` / `get_capacity` / `get_capacity_bytes`。
- `reserve`：增长、满足后不再增长（`tight=false` 的 1.5x 与 `tight=true` 的精确值）、**既有元素内容保持**。
- `resize`：扩容（新槽经 `memnew_arr_placement` 构造）、缩容（`count` 变小、**缓冲槽仍为有效可读对象**）、`resize(0)` 保留容量与指针。
- `clear`：`count=0`，`capacity`/`data` 指针不变，**不析构**（用非平凡析构计数器断言析构次数为 0）。
- `reset`：`data==nullptr && capacity==0 && count==0`；非平凡析构类型全量析构。
- `push_back`：`const&` / `&&` / initializer 三种重载；超过初容触发增长且既有内容不乱。
- `insert`：空表、首、中、尾（`p_at == count`）；**越界 `p_at > count` 触发断言**（在 `DEBUG_ENABLED` 的单测内以子进程/崩溃标记方式处理，或标注为不可在进程内测）。
- `operator[]`：读写、越界 `CRASH_BAD_INDEX`（同上的崩溃路径策略）。
- `ArmatureDrawData::get_capacity_bytes()`：构造已知层/命令后断言**精确字节数**（D2 回归：修复前断言失败）。

**验证**：`--gddb-run-tests` 全绿；`implement.md` 记录 D1–D5 各自「修复前会失败」的证据（可用一步 `git stash` 或临时回退验证）。

### 步骤 5 — R8 评估 R1/R3

1. 走查 `src/debug_draw.cpp` 的 `DebugDrawGeometry` 与 `BoneScratch`、`src/armature_view.cpp` 的 `SurfaceData`/`surfaces`。
2. 对照 design §5 的判据，给出每处的「可/不可 + 理由」。
3. 结论写入任务目录（如 `research/reuse-evaluation.md`）；若判定应改且代价可控，与用户确认后再纳入。

**验证**：书面结论存在，含代码证据（file:line）。

### 步骤 6 — 全局收尾

1. 双模板构建（`tests=no` 默认）通过；demo 冒烟无新错误。
2. `tests=yes` 构建 + `--gddb-run-tests` 通过。
3. 更新 `.trellis/spec/build/build-systems.md`（记录 `tests` 开关、运行命令、双系统一致性要求）。
4. `README.md` / `README.zh.md` 若列出构建方式则同步。

## 验证命令

```bash
# 构建（默认，不含测试）
scons -Q --silent platform=windows target=template_debug   arch=x86_64 -j1
scons -Q --silent platform=windows target=template_release arch=x86_64 -j1

# 构建（含测试）
scons -Q --silent platform=windows target=template_debug arch=x86_64 -j1 tests=yes

# 跑到 demo 里执行测试
cp bin/libgddragonbones.windows.template_debug.x86_64.dll demo/addons/godot_dragon_bones.daylily-zeleen/bin/
"D:/Dev/godot/godot/bin/Godot_v4.3-stable_win64_console.exe" --path demo --gddb-run-tests

# 冒烟
"D:/Dev/godot/godot/bin/Godot_v4.3-stable_win64_console.exe" --path demo res://.agent_tmp/smoke.tscn --quit-after 300

# 源列表核对
scons -n platform=windows arch=x86_64 target=template_debug tests=yes | grep -c doctest   # 期望 0
```

## 审查门

- 步骤 1 后：确认抽取无行为变化（构建 + 冒烟）。
- 步骤 2 后：确认 D1–D5 修复，且 D2 有失败→通过的证据。
- 步骤 4 后：测试全绿并覆盖每个 API/边界。
- 步骤 5 后：R8 结论评审。

## 回滚点

| 步骤 | 回滚 |
|---|---|
| 1 | 还原 `armature_draw_data.h`、删除 `initialized_buffer.h` |
| 2 | 还原 D1–D5 各 hunk（保留测试以暴露问题） |
| 3 | 删除 `tests/` 与 SCons/CMake/register_types 的条件分支 |
| 4 | 删除测试文件 |
| 5 | 仅文档，无代码回滚 |

---

## 执行记录（已完成）

### 步骤 1 — 抽取 `src/initialized_buffer.h`

`InitializeBuffer` 原样搬到 `src/initialized_buffer.h`（自带 `defs.hpp`/`error_macros.hpp`/`memory.hpp`
+ 标准库 `<cstddef>/<cstdint>/<cstring>/<type_traits>/<utility>`），命名空间 `godot::internal` 不变。
`armature_draw_data.h` 改为 `#include "initialized_buffer.h"`（净删 ~129 行）。
双模板构建通过，demo 冒烟无变化。

### 步骤 2 — 修复 D1–D5

- **D1** `resize()` 返回类型 `U` → `void`（原实现无 return）。
- **D2** `ArmatureDrawData::get_capacity_bytes()` 去掉多余的 `sizeof(Data) *`。
- **D3** `insert()` 补 `CRASH_BAD_INDEX(p_at, count + 1)`（允许 `p_at == count` 尾插）；
  `buff` 往返经实测正确，保留。
- **D4** `reset()` 末尾补 `count = 0`。
- **D5** 删除 `end_frame()` 的过时注释（函数体保留为空）。
- **不改**：`clear()`/`resize()` 不析构 —— 设计语义，测试断言「缩容后缓冲槽仍可读」。

### 步骤 3 — 构建系统接入

- `SConstruct`：`tests` 参数（默认 no）→ `CPPDEFINES=["GDDB_TESTS_ENABLED"]` + `CPPPATH=["tests/"]` + `Glob("tests/*.cpp")`。
- `CMakeLists.txt`：`GODOT_DRAGONBONES_TESTS`（默认 OFF）→ 同一宏 + `target_sources(tests/*.cpp)` + include。
- `src/dragon_bones_registration.cpp`：SCENE 初始化末尾 `#ifdef GDDB_TESTS_ENABLED` 调 `gddb::tests::try_run()`。
- `tests/test_runner.h`：扫 `--gddb-run-tests`，自定义 reporter（经 Godot print 落地），`std::exit(exit_code)`。
- `tests/test_main.cpp`：唯一 `DOCTEST_CONFIG_IMPLEMENT` TU。

**验证**：`scons -n ... tests=no | grep -c tests/` → 0；`tests=yes` → 编译；CMake OFF → build.ninja 无 test_main；ON → 有且构建通过。

### 步骤 4 — 单元测试

`tests/initialized_buffer_test.h`（11 用例）+ `tests/armature_draw_data_test.h`（D2 回归）。
共 **119 assertions**，`--gddb-run-tests` 全绿、exit 0。

**回归证据**（逐条回退修复、确认测试失败，再恢复）：

| 缺陷 | 回退后失败断言 |
|---|---|
| D2 | `cap < sizeof(Data)*sizeof(Data)` → `16240 < 7744` 失败 |
| D3 | `has_diag`（子进程 `out of bounds` 诊断）失败 |
| D4 | `b.size() == 0` → `3 == 0` 失败 |

D1 为编译期契约（无 return 已消除），由类型改为 `void` 保证；D5 为注释删除，无运行期断言。

**关键实现细节**：
- `OS::execute` 的输出 `Array` 只含**一个** String（stdout 整体，`p_read_stderr=true` 时含 stderr），
  不是逐行 —— `core/core_bind.cpp:411-424`。
- 子进程需传真实工程路径（`globalize_path("res://")`），`--path res://` 非法。
- `SceneTree` 不在 `build_profile.json`，故 `try_run` 用 `std::exit` 而非 `tree->quit()`。

### 步骤 5 — R8 评估

结论见 `research/reuse-evaluation.md`：

| 对象 | 可改？ | 决定 |
|---|---|---|
| R1 `DebugDrawGeometry` | 技术可以 | **不改**（收益边际；触碰逐 bit 验证过的路径，风险 > 收益） |
| R1 `BoneScratch` | 可以但无意义 | 不改 |
| R3 `SurfaceData` | **不可以** | 不改（终点是 `RenderingServer` 的 `Packed*Array`；换过去要多一次整块拷贝，且 `Packed*Array` 非平凡搬移与 `insert` 的 memmove 冲突） |
| R3 `surfaces`/`mesh_surfaces` | 不可以 | 不改（元素非平凡搬移） |

### 步骤 6 — 文档

- `.trellis/spec/build/build-systems.md`：新增「C++ Unit Tests (`tests`)」小节（双系统开关、运行命令、两个 gotcha）。
- `README.md` / `README.zh.md`：新增「C++ unit tests」小节。
- `thirdparty/doctest/`（`doctest.h` + `LICENSE.txt` + `patches/`）纳入版本控制。

### 步骤 7 — 代码审查（trellis-check）与整改

审查结论：**0 CRITICAL / 6 WARNING / 6 NOTE**。已整改：

| 项 | 处置 |
|---|---|
| W2 `tests/test_runner.h` 缺 `#include <initialized_buffer.h>`（仅靠传递包含） | **已补** |
| W3 `operator[]` 越界路径无测试（PRD R3 / design 步骤 4 要求） | **已补**子进程测试 `--gddb-test-index-oob`，并验证回退该守卫后测试失败 |
| W4 D1 无回归断言 | **已补** `static_assert(is_same_v<decltype(resize(0)), void>)`；实测把返回类型改回 `U` 会触发 MSVC `C4716 必须返回一个值` |
| W1 `scons ... tests=yes` 触发 godot-cpp 的 "Unknown SCons variables" 警告 | **保留 + 文档化**：godot-cpp 自建 `Variables` 且无扩展点；选项实际生效，消除该警告需重构为「自建 Environment + opts 再传给 godot-cpp」，代价不成比例。已在 SConstruct 与 spec 中注明 |
| W6 `.gitignore:20 build/` 把 `.trellis/spec/build/` 整层静默忽略（该层 spec 从未入库），且本文档行号漂移 | **已修**：加 `!.trellis/spec/build/` 反转规则（`build/` 仍被忽略）；修正文档行号引用 |
| N1 `test_runner.h` 引入未使用的 `engine.hpp`/`main_loop.hpp` | **已删** |
| N4 SConstruct 手写 `ARGUMENTS.get` 解析 | 与 W1 同源，保留并注释 |

未采纳（附理由）：
- N2 D2 断言用「量级」而非「精确字节数」：精确值依赖 `sizeof(Data)` 与 `layers` 的 1.5x 增长策略，
  不同平台会不同；量级断言已能可靠捕获 D2（实测 `16240 < 7744` 失败）。
- N3 `insert(p_at, T&&)` / `insert(p_at, const T&)` 无调用点：属公开 API 面，保留以备将来使用。
- N5 `tests/*.obj` 构建产物：已被 `*.obj` 忽略。

### 步骤 8 — 最终验证

- 默认构建（`tests` 关闭）：`template_debug` / `template_release` 均通过；demo 冒烟无新错误。
- `tests=yes` 构建 + `--gddb-run-tests`：**122 assertions 全过**，exit 0。
- CMake：`GODOT_DRAGONBONES_TESTS=OFF` 不编译 tests；`ON` 编译且构建通过。
- 回归证据（逐条回退修复 → 测试失败 → 恢复）：
  D2 `16240 < 7744`；D3 子进程 `has_diag=false`；D4 `3 == 0`；
  `operator[]` 越界守卫 `rc != 0 → 0 != 0`；D1 返回类型 → MSVC `C4716`。

### 步骤 9 — 重构：把子进程死亡用例从通用入口解耦（审查后整改）

**问题**：初版把两个具体用例的实现细节（`--gddb-test-insert-oob`、`--gddb-test-index-oob`
及各自的越界代码）直接写进了 `try_run()`。`try_run()` 是 `register_types` 的唯一钩子、
是**通用**测试入口；每新增一个崩溃用例都要改它，且把 `initialized_buffer.h` 也牵了进来。

**方案**：新增 `tests/test_death.h`，提供通用子进程死亡测试设施：

- 用例在命名空间作用域用 `GDDB_DEATH_CASE("<name>") { ... }` 注册执行体；
- 通用入口只按 `--gddb-death=<name>` 派发，**零知识**；
- `try_run()` 收敛为固定两分支：派发死亡用例 / 跑 doctest。

**过程中踩到并修复的三个坑**（均已写入 spec）：

1. **静态初始化期禁用 Godot 容器**：初版注册器在命名空间作用域构造
   `LocalVector<DeathCase>` → 该构造发生在库加载（`DllMain`）阶段，此时 Godot 内存系统未就绪 →
   抛异常 → Windows 报 `Error 1114: DLL 初始化例程失败`（DLL 本身有效，仅加载失败）。
   改为**侵入式 POD 单链表**（节点在各 TU 静态存储、链表头零初始化），无构造、无分配。
2. **trap 的退出码是负数**：Windows 为 `0x80000003`（`STATUS_BREAKPOINT`，即 `-2147483645`），
   POSIX 是信号。初版用 `rc < 0` 判「无法拉起子进程」从而误吞了用例；只有 `OS::execute`
   自带的 `-1` 才表示启动失败。
3. **`std::exit` 跳过引擎退出**：因 `SceneTree` 不在 build profile 中，测试入口用 `std::exit(exit_code)`；
   随之而来的退出期噪声（`Pages in use exist at exit` / `BUG: Unreferenced static string`）是预期的。

**验证**：
- `--gddb-run-tests`：**122 assertions 全过**，exit 0，进程正常退出，无 1114。
- 回归检测仍有效：移除 `operator[]` 守卫 → 死亡用例失败（`rc != 0 → 0 != 0`），exit 1。
- **可扩展性实测**：临时新增一个 `GDDB_DEATH_CASE("demo-probe-case")`（空指针写），
  只加一个头文件 + 一行 include，`try_run()` 零改动即被派发执行（子进程 exit 5）。
- 默认双模板构建通过；demo 冒烟无新错误；CMake `GODOT_DRAGONBONES_TESTS=ON` 构建通过。
