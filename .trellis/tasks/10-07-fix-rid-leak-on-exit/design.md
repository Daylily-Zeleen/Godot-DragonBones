# Design — 修复退出时 Shader/Material RID 泄露 + CI 泄露/崩溃门禁

## 1. 根因（已实测确认）

`src/debug_draw.cpp:98-99` 的函数级静态 `Ref<ShaderMaterial>`：

```cpp
static Ref<ShaderMaterial> &get_bone_material() {
    static Ref<ShaderMaterial> material = [] { ... }();   // 仅此一处持有
    return material;
}
```

- 该静态量**只在 DLL 卸载 / CRT 静态析构**时才执行 `~ShaderMaterial()` / `~Shader()`；
- 此时 `RenderingServer` 早就在 `finalize_display()`（`main.cpp:5325` → `memdelete(rendering_server)`）中销毁；
- 于是析构报 `Parameter "RenderingServer::get_singleton()" is null`，其内部 RID 未释放 → `RID_Owner` 析构时报告 `1 RID allocations of type ...DummyShaderE were leaked at exit`。

**它没有任何清理注册**（`blend_materials` 有 `clear_static`，这个没有）。这正是两者的差别。

### 触发条件（实测）

| 场景 | 结果 |
|---|---|
| 非编辑器 `--headless --path demo` | 无泄露（不会创建该材质） |
| `--headless --editor --path demo` | **必现** 3 行 ERROR |
| 编辑器 + 禁用扩展 | 无泄露（仅扩展缺失的解析错误） |

编辑器启动时检查器枚举 `debug_draw_*` 属性 → `get_color_ik_bone_outline()`（`debug_draw.cpp:545`）→ `get_bone_material()` **惰性创建**该静态材质。**无需打开场景**即可复现。

### 实测证据（插桩 → 定位 → 反证）

1. `clear_static`（已有的清理回调，在 SCENE 级 uninit、RS 仍存活）插桩：
   ```
   [PROBE] clear_static: RS alive=true size=1
   ```
   → `blend_materials` 被正确清理；**泄露依旧** ⇒ 排除 `blend_materials`。
2. 在同一时机额外调用 `get_bone_material().unref()`：
   ```
   [PROBE] clear_static: RS alive=true size=1
   [PROBE] release: RS alive=true
   [PROBE] released
   ```
   → **所有 ERROR 完全消失**（无 `leaked at exit`、无 `get_singleton() is null`）⇒ 元凶锁定为 `debug_draw.cpp:99` 的静态 `ShaderMaterial`。

### 关键时序证据（`main/main.cpp`）

```
5272-5273  uninitialize_modules(MODULE_INITIALIZATION_LEVEL_EDITOR)
5280-5281  uninitialize_modules(MODULE_INITIALIZATION_LEVEL_SCENE)   ← clean_static 在此执行，RS 仍存活
5325       finalize_display() → memdelete(rendering_server)          ← RS 才销毁
```

**结论**：模块 uninit 时 RenderingServer 仍存活，是可靠的释放时机。修复只需把静态材质纳入既有清理机制，**无需改动释放顺序或引入新钩子**。

> 注：本机 `D:/Dev/godot/godot` 源码为 4.8-dev，与运行时二进制 4.3-stable 版本不同，故引擎侧 `.cpp` 行号不可直接对照；上述结论均以本机实测输出为准。

## 2. 修复方案

**方案 A（采用）**：把静态材质纳入既有的 `DragonBones::add_clean_static_callback` 机制。

- 在 `debug_draw.cpp` 增加一个清空该静态 `Ref` 的函数（如 `clear_static_bone_material()`）；
- 在 `DebugDraw`（或 `DragonBonesArmatureView::_bind_methods`，与 `blend_materials` 同处）注册到 `add_clean_static_callback`；
- 由既有 `clean_static()` 在 SCENE 级 uninit 调用 —— 实测此时 RS 存活，释放安全。

**理由**：最小改动、复用既有机制、与 `blend_materials` 处理一致；实测已验证该时机可行（PROBE 反证）。

**需同时排查**：`saver` / `loader`（`dragon_bones_registration.cpp:54-55`）同为静态 `Ref`，但它们在 uninit 中已显式 `unref()`（`:108-112`），不受影响。

**方案 B（否）**：改为每帧创建材质 —— `_draw` 热路径，与仓库既有性能取向冲突。

## 3. CI 门禁设计

### 3.1 位置

挂在现有 `tests` job（已编译扩展、已下载 Godot）；新增 step 放在 doctest **之后**（doctest 用 `std::exit` 提前结束，走不到正常退出路径）。

### 3.2 检查内容（用户决定：全量 `ERROR:` 扫描，暂不设白名单）

以 `--headless --editor --path demo --quit-after N` 运行（实测可复现；Dummy 驱动无需 GPU）：

- 捕获 stdout+stderr；
- **扫描所有 `ERROR:`** → 命中即失败；
- 扫描崩溃标记：`CrashHandlerException` / `Program crashed` / `signal [0-9]+` → 命中即失败；
- 显式取回管道退出码（`PIPESTATUS`），并断言确实执行到了退出期（防「进程没起来被判绿」）。

导入期噪声（`--import` 阶段）与测试无关，已由既有 `Import project assets` step 处理。

### 3.3 失败中止

step 失败默认中止该 job 后续 step；不新增需要串联的独立 job。

### 3.4 红线验证（R5）

PR 先提交「CI 检查（未带修复）」→ CI 实际变红 → 再提交修复使其转绿。避免写出永不为红的检查。

## 4. 兼容性与风险

| 项 | 说明 |
|---|---|
| 平台 | 现象在 Windows x64 实测；判定基于文本输出，平台无关；需在 CI(Linux) 确认同样复现 |
| 渲染驱动 | Dummy（`--headless`）即可，不需 GPU |
| 噪声 | 非 verbose 的编辑器运行实测**只有那 3 行泄露 ERROR**；`--verbose` 下的 `Pages in use exist` 等属既有现象，CI 不使用 verbose |
| 回滚 | CI 门禁与源码修复互相独立，可分别回滚 |

## 5. 验证策略

1. **修复前**：`--headless --editor --path demo --quit-after N` 必现 3 行 ERROR（已实测）。
2. **修复后**：同命令 0 行 ERROR；非编辑器命令仍 0 行。
3. **门禁自检**：依赖 R5 的先行提交在 CI 上的红色结果作为证据。
4. **回归**：doctest 122 断言全绿；`build` matrix 不受影响。
