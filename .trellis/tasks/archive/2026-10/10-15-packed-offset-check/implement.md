# Implement — 加载期打包偏移越限检查

> 中文摘要：实现记录。检查落在 `src/`，不改 vendored；调试构建生效。

## 交付物

| 文件 | 内容 |
|---|---|
| `src/packed_offset_check.h` | `PACKED_OFFSET_LIMIT`（65535）与 `check_packed_offsets()` 声明 |
| `src/packed_offset_check.cpp` | 按骨架 JSON 复算偏移，返回越限摘要（合法时空串） |
| `src/factory.cpp` | `load_data` 内 `#ifdef DEBUG_ENABLED` 调用检查，越限 `ERR_FAIL_COND_V_MSG(ERR_INVALID_DATA)` |
| `tests/packed_offset_check_test.h` | 6 个用例；`tests/test_main.cpp` 登记 |
| `README.md` / `README.zh.md` | 各 3 行说明检查范围、失败表现、构建差异 |
| `.trellis/spec/gdextension/type-conversion.md` | § Narrow Runtime Buffers 补 Load-time guard |

## 关键决策

| 决策 | 选择 | 理由 |
|---|---|---|
| 落点 | `src/` | 偏移是 JSON 里的确定性累加，可离线复算，无需改 vendored |
| 判据 | 峰值**偏移**，非缓冲总量 | 单个大网格的偏移可能是 0，窄模式下不读错，按总量判会误报 |
| 采集时机 | 加载时读骨架文件 | 截断后无法从缓冲反推真值（`85878→20342`，20342 本身合法） |
| 复算顺序 | 严格镜像解析器 | 偏移是 `.size()` 快照，顺序错则峰值错 |
| 生效范围 | 仅调试构建 | 检查是诊断手段，发布构建不为它付开销 |
| `.dbbin` | 不检查 | 不经 JSON 打包器，16 位偏移由导出器写死 |

## 覆盖的偏移类

- `floatArray`：`MeshFloatOffset`（`_parseMesh`）、`WeightFloatOffset`（带 `weights` 时）
- `intArray`：`colorOffset`（`_parseSlotColorFrame` 的显式颜色块）

每类都记「写入前的缓冲大小」作为偏移，故复算必须先记录再增长。

## 验证记录

| 项 | 结果 |
|---|---|
| 单测 | 21 cases / 138 assertions 全绿 |
| 变异验证 1 | 删除颜色块记录 → 颜色用例失败（证明该用例真走 intArray 路径） |
| 变异验证 2 | 改为增长后记录（等价按总量判定）→ 2 个用例失败（证明「峰值非总量」语义被锁定） |
| 越限资源（floatArray） | 报 `floatArray peak 121666 at "headwear"`，与独立 Python 复算逐值吻合 |
| 越限资源（intArray） | 报 `intArray peak 70584 at "eyelash_up_c2_2"`，与 `sim_color.py` 吻合 |
| 既有 6 套资源 | 零误报 |
| 构建 | SCons（debug/release）与 CMake 均通过 |
| 构建差异 | release 库中无该报错文案、无符号引用；debug 库中存在 |
| 行为差异 | 同一越限资源：debug 拒绝，release 放行 |
| vendored | `git status thirdparty/dragonBones/` 干净 |

## 未覆盖（已知边界）

- `DeformCount` / `DeformValueCount`：也是 `.size()` 快照，读取处不回绕。单帧 FFD 写入的 float 数超 65535 会读错；当前最大资源实测约 88，够不着。
- `MeshVertexCount`（`src/slot.cpp:170`）：用 `auto` 接收（→`int16_t`），顶点数超 32767 的网格会读成负数。属既有问题，非本次引入。
