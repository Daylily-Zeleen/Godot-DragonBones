# Design — 修复退出期 Shader/Material 泄露（Linux 上为段错误）+ CI 门禁

## 1. 根因（backtrace 铁证，跨平台同源）

`src/debug_draw.cpp:98-99` 的函数级静态 `Ref<ShaderMaterial>`：

```cpp
static Ref<ShaderMaterial> &get_bone_material() {
    static Ref<ShaderMaterial> material = [] { ... }();   // 仅此一处持有，无任何清理注册
    return material;
}
```

### Linux 崩溃栈（WSL，Godot 4.3-stable linux，`--import`）

```
Thread 1 received signal SIGSEGV, Segmentation fault.
0x0000000000000000 in ?? ()
#1  godot::internal::_call_native_mb_ret<signed char> (..., engine_ptrcall.hpp:57)
#2  godot::RefCounted::unreference (this=0x1ebd0170)         (ref_counted.cpp:56)
#3  godot::Ref<godot::ShaderMaterial>::unref
      (this=0x7fffe9891910 <godot::(anonymous namespace)::get_bone_material()::material>)
#4  godot::Ref<godot::ShaderMaterial>::~Ref                   (ref.hpp:203)
#5  __run_exit_handlers → __GI_exit                            ← CRT 静态析构 / 进程退出
```

frame #3/#4 直接指向我们的静态量；frame #1 是 godot-cpp 的 ptrcall 跳板，此时引擎的
GDExtension 函数指针表已被清空 → 跳向 `0x0` → SIGSEGV。

### 跨平台表现差异（同一 bug）

| 平台 | 表现 | 退出码 |
|---|---|---|
| Windows | `ERROR: RenderingServer::get_singleton() is null` + `leaked at exit` | 0（静默） |
| Linux | **SIGSEGV** + 同样的 `leaked at exit` | 139 |

### 触发路径（实测）

- **非编辑器 `--headless --path demo`**：不创建该材质 → 无泄露（两平台均 exit=0）。
- **`--import`**（非编辑器！）：**两平台都触发** —— Linux 段错误、Windows 报错+泄露。
- **`--editor`**：编辑器枚举检查器属性 → `get_color_ik_bone_outline()`（`debug_draw.cpp:545`）→ 惰性创建。

> 修正前次判断：先前把 `--import` 的段错误与 `--editor` 的报错当成两件事，实为**同一根因的不同触发入口**。

## 2. 这是既有问题（非本次改动引入）

PR #74 的 CI（run 37528759403，早于本次任何改动）中：

```
ERROR: 1 RID allocations of type '...DummyShaderE' were leaked at exit.
WARNING: ObjectDB instances leaked at exit
Segmentation fault (core dumped)   --import exit=139
--import exit=139 (non-zero is expected: ...)
```

原 `Import project assets` step 用 `set +e` 且注释声明"non-zero is expected"，把
**段错误当正常容忍**，因此长期未被发现。

> 另一处修正：先前把 `Unreferenced static string` / `Pages in use exist` 大量 ERROR
> 判为"既有噪声"。实测证明它们是**同一次崩溃的次生输出** —— 修复后这些输出全部消失。

## 3. 修复方案（已实现并跨平台验证）

复用仓库既有的静态清理机制 `DragonBones::add_clean_static_callback`：

- `debug_draw.h` 新增 `static void release_static_material();`（`DebugDraw` 成员，具外部链接）；
- `debug_draw.cpp` 在匿名命名空间**之外**定义它，内部调用 `get_bone_material().unref()`；
- `armature_view.cpp::_bind_methods()` 中与 `blend_materials` 的 `clear_static` **并列注册**（`#ifdef DEBUG_ENABLED` 守卫）。

时序（`main/main.cpp`）：`uninitialize_modules(MODULE_INITIALIZATION_LEVEL_SCENE)`（`:5280`，
清理回调执行处）**早于** `finalize_display()`（`:5325`，RenderingServer 销毁）——
实测此时 `RenderingServer::get_singleton()` 非 null，释放安全。

**替代方案（否）**：改为每帧创建材质 —— `_draw` 是热路径，与仓库既有性能取向冲突。

## 4. 验证（A/B 对照，同一环境同一引擎二进制）

用 CI 编译产物与本地修复版 `.so` 对比（WSL + Godot 4.3-stable linux）：

| 场景 | 旧（无修复） | 新（修复后） |
|---|---|---|
| 1st `--import`（扩展未注册） | **exit=139 SIGSEGV** | exit=0 |
| 2nd `--import`（扩展已注册） | **exit=139 SIGSEGV** + 泄露 | exit=0，**0 ERROR** |
| `--editor --quit-after` | SIGSEGV | exit=0，**0 ERROR** |
| 普通运行 `--quit-after` | exit=0 | exit=0，0 ERROR |
| doctest（Linux） | — | **122/122 断言通过** |

Windows：`--editor` 由"3 行 ERROR"变为 **0 ERROR**；`template_debug`/`template_release` 均编译通过。
Linux：`template_debug` / `template_release` / `tests=yes` 均编译通过；release 三场景全绿。

**无需白名单**：全量 `ERROR:` 扫描在修复后为 0 命中。

## 5. CI 门禁

见 `.github/workflows/build.yml` 的 `Check for exit-time resource leaks and crashes`（已提交 `b5aa8d0`）：
全量扫描 `ERROR:` + 崩溃标记 + 非零退出码 + 防假绿断言，失败即非零退出并中止后续。

**待修正**：当前检查跑的是 `--headless --editor --path demo`，而实测 `--import` 路径
**更早、更直接**地复现（且是既有 step 本就在跑的路径）。见 implement.md 的后续调整项。
