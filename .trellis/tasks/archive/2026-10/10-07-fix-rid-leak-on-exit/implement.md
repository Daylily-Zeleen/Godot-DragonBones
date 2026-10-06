# Implement — 修复退出期 RID 泄露 + CI 泄露/崩溃门禁

执行顺序按用户给定的三步：**1) 建任务（已完成）→ 2) 改 CI 并提交推送，使 CI 复现失败 → 3) 修复，不提交**。

## 步骤 0：准备工作（已完成）

- [x] 复现泄露，确认触发条件（`--headless --editor --path demo` 必现 3 行 ERROR）
- [x] 定位根因：`src/debug_draw.cpp:99` 静态 `Ref<ShaderMaterial>` 无清理注册（插桩反证）
- [x] 确认 CI 可用：Dummy 驱动即可复现，不需 GPU

## 步骤 1：改 CI，让泄露/崩溃能导致失败（已完成，提交 `b5aa8d0`）

**目标**：`.github/workflows/build.yml` 的 `tests` job 新增检查步骤。

- [x] 1.1 在 `Run C++ unit tests` step **之后**新增 step `Check for exit-time resource leaks and crashes`：
  - 运行：`"$GODOT" --headless --editor --path demo --quit-after 120`，stdout+stderr 落盘（`tee`）；
  - **全量扫描 `ERROR:`**：命中即打印匹配行并非零退出；
  - 扫描崩溃标记：`CrashHandlerException`、`Program crashed`、`signal [0-9]+` → 命中即非零退出；
  - 显式取回管道退出码（`PIPESTATUS`），并断言确实跑到退出期（输出含 `Godot Engine v` 行），防「进程没起来被判绿」。
- [x] 1.2 本地先自查：同一命令在当前（未修复）代码上确实命中 3 行 ERROR，检查脚本判定为失败。
- [x] 1.3 提交（中文 commit message），推送到新分支并创建 PR。
- [x] 1.4 **确认 CI 真的红了**（记录 run/job 链接作为 R5/AC5 证据）。若 CI 未红，说明检查无效，回到 1.1 修正。

**验证命令**：
```bash
# 本地模拟（Linux CI 用 linux 二进制；本地 Windows 用 console 版）
Godot_v4.3-stable_win64_console.exe --headless --editor --path demo --quit-after 120 2>&1 | tee out.log
grep -c "ERROR:" out.log     # 期望 >= 1（未修复时）
```

## 步骤 2：修复（已由用户改写定稿，提交 `d834b27`）

最终实现（用户改写，比原方案更简洁 —— 直接在材质首次创建的 lambda 内注册）：

- `src/debug_draw.cpp`：
  - `#include "dragon_bones.h"`；
  - 匿名命名空间内前置声明 `static void release_bone_material();`；
  - 在 `get_bone_material()` 的 lambda 内、`return ret;` 之前调用
    `DragonBones::add_clean_static_callback(release_bone_material);`；
  - 命名空间外定义 `static void release_bone_material() { get_bone_material().unref(); }`。
- 仅 `src/debug_draw.cpp` 一个文件、+9 行。原方案中 `debug_draw.h` 的成员声明与
  `armature_view.cpp::_bind_methods()` 的注册点**已撤销**（不需头文件暴露，注册点也无需集中）。

- [x] 2.1 为静态 `Ref<ShaderMaterial>` 增加释放入口
- [x] 2.2 注册到既有静态清理机制 `DragonBones::add_clean_static_callback`
- [x] 2.3 构建（Windows `-j1`；Linux WSL `-j4`）均通过
- [x] 2.4 双平台验证：
  - Windows：`--headless --editor --path demo --quit-after 60` → **0 行 `ERROR:`**（AC1）
  - Linux：`--import`（首次/再次）、`--editor`、普通运行 → **exit=0 且 0 行 ERROR**（AC1/AC2）
  - A/B 对照：CI 旧 `.so`（无修复）1st/2nd `--import` 均 **139 SIGSEGV + RID 泄露**；
    修复版全绿 —— 同环境同引擎二进制，仅扩展不同
- [x] 2.5 回归：Linux doctest **122/122 断言通过**（AC6）；`template_debug`/`template_release`/`tests=yes` 均编译通过
- [x] 2.6 提交推送（`d834b27`），CI 校验通过

## 步骤 3：CI 校验（已完成）

- [x] 修复前：run `37532862480`（`b5aa8d0`）→ **failure**（`🧪 C++ unit tests` exit 139）
- [x] 修复后：run `37543911036`（`d834b27`）→ **success，23/23 job 全绿**
- [x] 确认新 step `Check for exit-time resource leaks and crashes` 确实执行（`runner exit=0`，无 `::error::`）

## 遗留（本次未做，用户未表态）

CI 检查目前跑 `--headless --editor --path demo`；实测 `--import` 路径更早更直接地复现
（且 `Import project assets` step 仍用 `set +e` 容忍段错误）。若需收紧，可把检查挂到 `--import`
并去掉该容忍。

## 风险与回滚点

| 风险 | 应对 |
|---|---|
| 匿名命名空间导致链接失败 | 把释放入口做成 `DebugDraw` 静态成员（头文件声明），而非自由函数 |
| 注册时机晚于 RS 销毁 | 复用 `blend_materials` 已验证可行的注册点；若无效，退回「将材质降为 View 实例成员」 |
| CI 在 Linux 行为与 Windows 不同 | 1.4 必须看到 CI 实际变红；若不复现，需在 Linux 上重新定位触发路径 |
| 检查误伤既有噪声 | 实测非 verbose 运行仅有那 3 行 ERROR；若 CI 出现其它 ERROR 导致误红，按用户意见再议白名单 |

## 提交前检查

- [x] `git status` 确认只含预期文件（未提交 `demo/` 既有改动与构建产物）
- [x] commit message 用中文
- [x] 步骤 2 的改动经用户审查后由用户定稿并提交（`d834b27`）
