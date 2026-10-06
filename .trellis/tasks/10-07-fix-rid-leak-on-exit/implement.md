# Implement — 修复退出期 RID 泄露 + CI 泄露/崩溃门禁

执行顺序按用户给定的三步：**1) 建任务（已完成）→ 2) 改 CI 并提交推送，使 CI 复现失败 → 3) 修复，不提交**。

## 步骤 0：准备工作（已完成）

- [x] 复现泄露，确认触发条件（`--headless --editor --path demo` 必现 3 行 ERROR）
- [x] 定位根因：`src/debug_draw.cpp:99` 静态 `Ref<ShaderMaterial>` 无清理注册（插桩反证）
- [x] 确认 CI 可用：Dummy 驱动即可复现，不需 GPU

## 步骤 1：改 CI，让泄露/崩溃能导致失败（提交 + 推送）

**目标**：`.github/workflows/build.yml` 的 `tests` job 新增检查步骤。

- [ ] 1.1 在 `Run C++ unit tests` step **之后**新增 step（例如 `Check for exit-time errors`）：
  - 运行：`"$GODOT" --headless --editor --path demo --quit-after 120`，stdout+stderr 落盘（`tee`）；
  - **全量扫描 `ERROR:`**：命中即打印匹配行并非零退出；
  - 扫描崩溃标记：`CrashHandlerException`、`Program crashed`、`signal [0-9]+` → 命中即非零退出；
  - 显式取回管道退出码（`PIPESTATUS`），并断言确实跑到退出期（输出含 `Godot Engine v` 行），防「进程没起来被判绿」。
- [ ] 1.2 本地先自查：同一命令在当前（未修复）代码上确实命中 3 行 ERROR，检查脚本判定为失败。
- [ ] 1.3 提交（中文 commit message），推送到新分支并创建 PR。
- [ ] 1.4 **确认 CI 真的红了**（记录 run/job 链接作为 R5/AC5 证据）。若 CI 未红，说明检查无效，回到 1.1 修正。

**验证命令**：
```bash
# 本地模拟（Linux CI 用 linux 二进制；本地 Windows 用 console 版）
Godot_v4.3-stable_win64_console.exe --headless --editor --path demo --quit-after 120 2>&1 | tee out.log
grep -c "ERROR:" out.log     # 期望 >= 1（未修复时）
```

## 步骤 2：修复（**不提交，等用户审查**）

- [ ] 2.1 在 `src/debug_draw.cpp` 为静态 `Ref<ShaderMaterial>` 增加释放入口：
  - 加一个函数清空该静态引用（注意 `get_bone_material()` 位于匿名命名空间内，需在 `namespace godot` 内提供可外部链接的包装）；
  - 首选用 `DebugDraw` 的静态成员函数，或与 `blend_materials` 同处注册。
- [ ] 2.2 把该入口注册到 `DragonBones::add_clean_static_callback`（`armature_view.cpp:866` 同处，或 `debug_draw` 自己的 `_bind_methods`/`bind` 时机）。
  - **注意**：需确认注册确实发生在模块初始化阶段（`blend_materials` 的注册点在 `DragonBonesArmatureView::_bind_methods()`）。
- [ ] 2.3 `clang-format -i` 触碰的文件（仓库要求，见 `.trellis/spec/gdextension/quality-guidelines.md`）。
- [ ] 2.4 构建：`scons platform=windows target=template_debug arch=x86_64 -j1`。
- [ ] 2.5 验证：
  - `--headless --editor --path demo --quit-after 60` → **0 行 `ERROR:`**（AC1）；
  - `--headless --path demo --quit-after 30` → 0 行（AC2）；
  - `--headless --path demo res://dragonbones_demo/demo.tscn --quit-after 30` → 0 行（AC2）。
- [ ] 2.6 回归：`scons ... tests=yes` 后跑 `--gddb-run-tests`，122 断言仍全绿（AC6）。
- [ ] 2.7 **停下，向用户汇报，不提交**（用户明确要求待其审查）。

## 风险与回滚点

| 风险 | 应对 |
|---|---|
| 匿名命名空间导致链接失败 | 把释放入口做成 `DebugDraw` 静态成员（头文件声明），而非自由函数 |
| 注册时机晚于 RS 销毁 | 复用 `blend_materials` 已验证可行的注册点；若无效，退回「将材质降为 View 实例成员」 |
| CI 在 Linux 行为与 Windows 不同 | 1.4 必须看到 CI 实际变红；若不复现，需在 Linux 上重新定位触发路径 |
| 检查误伤既有噪声 | 实测非 verbose 运行仅有那 3 行 ERROR；若 CI 出现其它 ERROR 导致误红，按用户意见再议白名单 |

## 提交前检查

- [ ] `git status` 确认只含预期文件（不提交 `demo/` 既有改动与构建产物）
- [ ] commit message 用中文
- [ ] 步骤 2 的改动**不提交**
