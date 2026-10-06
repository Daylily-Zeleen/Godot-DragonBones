# 设计：InitializedBuffer 抽取、修复与测试接入

## 1. 边界与产物

| 产物 | 路径 | 说明 |
|------|------|------|
| 单头文件 | `src/initialized_buffer.h` | 模板 `godot::internal::InitializedBuffer`，可独立包含 |
| 修改 | `src/armature_draw_data.h` | 删掉内联定义，改为 `#include "initialized_buffer.h"`，并修 D2/D5 |
| 测试 | `tests/initialized_buffer_test.cpp`（或 `.h`） | doctest 用例 |
| 测试入口 TU | `tests/test_main.cpp` | 唯一 `DOCTEST_CONFIG_IMPLEMENT` 的 TU |
| 运行入口 | `tests/test_runner.h` | `try_run()`：扫 `--gddb-run-tests`，跑 doctest，设退出码 |
| 构建 | `SConstruct`、`CMakeLists.txt` | `tests` 开关 |
| 注册 | `register_types.cpp` | 调 `try_run()` |
| 文档 | `.trellis/spec/build/build-systems.md`、`README*.md` | 记录用法 |

## 2. 头文件设计

`src/initialized_buffer.h`：

```cpp
#pragma once
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/memory.hpp>          // memrealloc / memfree / memnew_arr_placement
#include <godot_cpp/templates/...>            // MAX / MIN 来源按抽取时实际依赖补齐
#include <type_traits>

namespace godot { namespace internal {

template <typename T, typename U = uint32_t, bool tight = false>
class InitializedBuffer { /* 与现状一致 + 缺陷修复 */ };

}} // namespace godot::internal
```

**不变量**（写入头文件注释，测试逐条覆盖）：

1. **`[0, capacity)` 全部是已构造的有效对象**；`count` 仅为逻辑用量游标。
2. `count <= capacity` 恒成立。
3. `data == nullptr` ⇔ `capacity == 0`；`resize(0)`/`clear()` 只改 `count`，**不析构、不释放**（与 `Packed*Array`/`LocalVector` 相反，这正是它存在的理由）。
4. **`reset()` 后回到初始态**：`data == nullptr && capacity == 0 && count == 0`（且析构了全部元素）。
5. 所有内存经 `memrealloc`/`memfree` 与 `memnew_arr_placement`，与 Godot 分配器一致（`memory-and-lifetime.md` 的桥接约定）。
6. `insert(p_at, …)` 要求 `p_at <= count`；`p_at == count` 为尾插。

## 3. 缺陷修复设计

### D1 `resize()` 无 return
改为 `_FORCE_INLINE_ void resize(U)`（调用点不使用返回值），与 `LocalVector::resize` 语义一致，避免留下无人使用的返回值契约。

### D2 `ArmatureDrawData::get_capacity_bytes()` 多乘一次
```cpp
// 现状（错）：layer.get_capacity_bytes() 量纲已是字节，外层再乘一次 sizeof(Data)
n += sizeof(Data) * layer.get_capacity_bytes();
// 修正：
n += layer.get_capacity_bytes();
```
`Layer::get_capacity_bytes()` 定义为 `data.get_capacity_bytes() + sizeof(z_order)`，量纲已是字节。

> 该缺陷未在当前 demo 触发崩溃，但会让 `try_reset()` 的 1 MiB 阈值约以 `sizeof(Data)`（≈80×）倍的速度被满足，从而**过早释放缓冲** —— 与「阈值释放」的设计意图相反。测试用固定构造断言精确字节数。

### D3 `insert()` 的 `p_at` 越界
`p_at > count` 时 `size() - p_at` 无符号下溢为极大值，`memmove` 越界 → **实测进程崩溃（rc=5）**。修复：

```cpp
_FORCE_INLINE_ void insert(U p_at, Func &&p_initializer) {
    CRASH_BAD_INDEX(p_at, count + 1);   // 允许 p_at == count（尾插）
    if (count >= capacity) reserve(count + 1);
    ...
}
```

> `buff` 取出-拷回往返**保留**：实测 `insert(1)` 得 `[0,99,1,2,3]`，结果正确；在「全缓冲区皆有效」模型下，它把 `data[count]`（有效对象）暂存、再放回 `data[p_at]` 位置，配合 `memmove` 完成整块搬移，语义自洽。

### D4 `reset()` 不归零 `count`
```cpp
void reset() {
    if constexpr (!std::is_trivially_destructible_v<T>) {
        for (U i = 0; i < capacity; i++) data[i].~T();
    }
    if (data) { memfree(data); data = nullptr; }
    capacity = 0;
    count = 0;   // ← 补上
}
```
实测 `reset()` 后 `size()` 仍为 3；不归零会让后续 `push_back` 以陈旧 `count` 驱动 `reserve(count+1)`，逻辑错乱。

> `clear()` / `resize()` **有意不析构**（见不变量 1/3），本任务不改，测试需断言「缩容后缓冲槽仍可读」以固化该语义。

### D5 删除过时注释
`ArmatureDrawData::end_frame()` 体为空；行 `armature_draw_data.h:283-285` 说明「把各层 data 截到实际写入的条数…并丢弃空层」的行为已不存在 → **直接删除该注释**（用户指定）。空函数体保留。

## 4. 构建系统接入设计

### 4.1 SCons
```python
opts.Add(BoolVariable("tests", "Build and run C++ unit tests", False))
...
if env.get("tests", False):
    env.Append(CPPDEFINES=["GDDB_TESTS_ENABLED"])
    env.Append(CPPPATH=[Dir("thirdparty").abspath])   # 供 #include <doctest/doctest.h>
    sources += Glob("tests/*.cpp")
```
- 宏名取 `GDDB_TESTS_ENABLED`（项目前缀，避免与 godot-cpp/其它扩展冲突）。
- `tests/` 不属于 `src/`，当前 glob 不会误收；仍需确认 `add_sources_recursively("thirdparty/", ...)` 不把 `thirdparty/doctest/*.cpp` 收进来（doctest 是 header-only，目录内无 `.cpp`；但需在实现时用 `scons -n` 核对）。
- **关键**：`thirdparty/doctest` 不能通过既有 `add_sources_recursively("thirdparty/", sources, [...])` 被编译 —— 它没有 `.cpp`，安全；但 `CPPPATH` 已含 `thirdparty/`，故 `#include <doctest/doctest.h>` 可能**无需**额外 `CPPPATH`。实现时先验证，多余则不加。

### 4.2 CMake
```cmake
option(GODOT_DRAGONBONES_TESTS "Build C++ unit tests" OFF)
if(GODOT_DRAGONBONES_TESTS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE GDDB_TESTS_ENABLED)
    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_main.cpp
                                          ${CMAKE_CURRENT_SOURCE_DIR}/tests/initialized_buffer_test.cpp)
endif()
```
- 与 SCons 用同一个宏名，`register_types.cpp` 只写一份 `#ifdef`。
- CMake 的 `HEADERS` glob 只收 `src/`，不需排除 `tests/`。
- 遵循 spec：用 `option()` 而非 `if(<var> STREQUAL "")`；两个构建系统的源集合必须一致（否则 CI 与本地分叉）。

### 4.3 运行入口
`tests/test_runner.h`（对齐 GodotJS-Ext 的 `jsb_test_runner.h`）：
- 在 `OS::get_singleton()->get_cmdline_args()` 中查找 `--gddb-run-tests`。
- 命中则注册一个自定义 reporter（把输出经 `UtilityFunctions::print` 落地，绕开 doctest 默认 stdout 在部分平台的截断问题），`context.run()`，然后：有 `SceneTree` 就 `quit(exit_code)`，否则 `std::exit(exit_code)`。
- `register_types.cpp` 在 `MODULE_INITIALIZATION_LEVEL_SCENE` 初始化末尾调用 `gddb::tests::try_run()`。
- 退出码：0 通过，非 0 失败 —— 供 CI 使用。

## 5. R1 / R3 复用评估（R8）

对象与判据：

| 归并对象 | 现状 | 判据 |
|---|---|---|
| R1 `DebugDrawGeometry` | 4 个 `LocalVector`（`vertices`/`colors`/`indices`/`vertex_uv`）+ `clear()`/`reset()` | 语义需逐点核对：`LocalVector::clear()` **会析构**，`InitializedBuffer::clear()` **不析构**。仅当使用者把「清空后立即覆写」当作约定时二者可互换 |
| R1 骨骼 scratch (`BoneScratch`) | `LocalVector<DebugBone>`、`LocalVector<StringName>` | `StringName` 非平凡析构，`InitializedBuffer` 的「缓冲区全有效」模型需要 `memnew_arr_placement` 真正构造每个槽 —— 换用反而更重，倾向不改 |
| R3 `SurfaceData` | 4 个 `Packed*Array` + `n_*` 游标 | **不能换**：它必须交给 `RenderingServer`（`Array`/`Packed*Array`），是 GDExtension 边界类型；`InitializedBuffer` 是纯扩展侧缓冲，无法替代 |
| R3 `DrawScratch::surfaces` / `mesh_surfaces` | `LocalVector<SurfaceData>` 等 | `SurfaceData` 含 `Packed*Array`（非平凡析构 + 非平凡搬移），而 `InitializedBuffer::insert` 的 `memmove` 依赖可平凡搬移；**不可换** |

初步判断（待实现期以实测/代码走查确认，AC7 要求给出结论与理由）：
- **R3 的 `SurfaceData` 不可换** —— 它承载跨 GDExtension 边界的 `Packed*Array`，`InitializedBuffer` 无法替代；只有其外层容器（`surfaces`）有讨论空间，收益有限。
- **R1 的 `DebugDrawGeometry` 是候选** —— 它的 4 个 `LocalVector` 与 `InitializedBuffer` 使用模式高度重合（追加、清空、resize 增长）。需确认 `clear()` 与 `reset()` 两个语义是否被同时需要。
- 结论无论正负都要落文档；若结论为「应改且代价可控」，纳入本任务，否则记入 follow-up。

## 6. 兼容性与风险

| 风险 | 缓解 |
|---|---|
| 抽取头文件后 `armature_draw_data.h` 的包含顺序/宏可见性变化 | 头文件自带全部依赖（`error_macros`/`memory`/`type_traits`），不依赖包含者先前 include |
| `insert()` 的 `memmove` 对非平凡搬移类型不安全 | 头文件注释写明约束；当前两个 `T` 均满足 |
| `tests=yes` 的宏泄漏到正式构建 | 宏仅在 `tests` 分支 `CPPDEFINES`；默认 `False`；AC3/AC4 双向验证 |
| `thirdparty/doctest` 未被跟踪 | AC8：`git add` 该目录（或改 submodule + 记录） |
| CMake 与 SCons 源集合分叉 | 同一宏名 + 同一 `tests/*.cpp`；AC3/AC4 两侧各验一次 |
| 测试用例依赖 Godot 运行时（`memrealloc` 走 Godot 分配器） | 测试必须在 Godot 进程内跑（`--gddb-run-tests`），不能做成裸可执行文件 |

## 7. 回滚

- 抽取+修复：单头文件是新增，`armature_draw_data.h` 改动局限在 include 与 D2/D5；回滚即还原这两个文件。
- 构建接入：`tests` 选项默认关闭，回滚只需删除 `tests/` 与三处条件分支（SCons/CMake/register_types）。
