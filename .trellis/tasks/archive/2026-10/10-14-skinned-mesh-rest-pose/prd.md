# 蒙皮网格渲染修复与 rest pose 姿态一致

## Goal

修复 DragonBones 蒙皮网格（skinned mesh）的渲染缺陷，使编辑器与运行时的显示与骨架数据一致：
1. `[none]`（未播放动画）初始状态下，全部部件应处于骨架的 **rest pose**（绑定姿态）位置；
2. 播放/切换动画后姿态正确；
3. 不出现任何形式的网格拉伸/碎片化。

## Background（已确认事实与已修部分）

### 真实根因（已定位并修复）：`Slot_GD::_updateMesh` 的 int16 溢出
- `src/slot.cpp:_updateMesh`（`weightData != nullptr` 分支）原代码
  `auto weightFloatOffset = intArray[weightData->offset + (unsigned)BinaryOffset::WeightFloatOffset];`
  中 `intArray` 的类型是 `const int16_t *`（`thirdparty/dragonBones/model/DragonBonesData.h:120`），
  而解析器写入该偏移时用的是 `_floatArray.size()`（`thirdparty/dragonBones/parser/JSONDataParser.cpp:647,656`）——
  **偏移按无符号 16 位存放，超过 32767 即被读成负数**。
- 该行原为 `int weightFloatOffset`（32 位，回绕有效），由 `7549e92`「一些可读性优化」改成 `auto`，
  `auto` 从缓冲区推导出 `int16_t`；随后的 `if (weightFloatOffset < 0) weightFloatOffset += 65536;`
  回绕在 16 位变量内被再次截断，**回绕完全失效**；再经 `(std::size_t)` 强转得到天文数字偏移 →
  越界读堆内存，读出什么值就把顶点算成什么值。
- 同一 bug 类还有第二处：`hasFFD` 分支的 `int vertexOffset` 曾写成
  `(std::size_t)intArray[...MeshFloatOffset]`，该转换让 `if (vertexOffset < 0)` 恒为假，回绕同样失效。
- 现象（同一越界读的两种取值，非两类独立缺陷）：读出 0 或极小值 → 顶点塌到各骨骼关节位置附近，
  视觉上像部件缩成一团；读出大值 → 爆炸式拉伸。堆布局抖动导致表现非确定性。
- idle 数据集实测：37 个蒙皮槽位中 **`WeightFloatOffset` 溢出 3 个**（`arm_L`=35288、`front_hair`=33112、`headwear`=37966）；
  另有 **`MeshFloatOffset` 溢出 2 个**（`arm_L`=33832、`headwear`=37118）。
- 修复：两处均改为 `int32_t` 并在 32 位宽度下回绕。
- 验证：`[DBG]` 探针 `iV` 由 `-30763` 变为正确的 `34773`；8 次运行的顶点输出逐字节一致；
  窗口渲染多次 `md5=c8cacb7af8a5` 稳定。

### 缺陷 A 与 B（结论已推翻，两处改动均已丢弃）
经 A/B 构建实测 + 离线数据对照，此前对 `src/factory.cpp::_buildSlot` 与 `src/armature.cpp::force_update`
的两处改动**均不成立**：

- **`factory.cpp` 的三步块冗余**：`BaseFactory::buildArmature`（`thirdparty/dragonBones/factory/BaseFactory.cpp:451-466`）
  在 `_buildArmature` 之后**必定**调用 `_buildBones` + `_buildSlots`，而 `_buildSlots`（`:134-186`）
  自己就会执行 `setRawDisplayDatas` / `_setDisplayList` / `_setDisplayIndex` 三步。
  因此 `_buildSlot` 内重复这三步对结果零影响：实测 `[none]` 态像素 md5 完全相同（`c8cacb7af8a5`），
  `deform_null` 计数亦相同（38 false / 2 true，true 的恰是数据里本就无蒙皮的 `neck` / `topwear`）。
  先前「`_displayIndex` 停在 -1」的推断错误：`_setDisplayIndex` 在 `_buildSlots` 中必然被调用。

- **`armature.cpp` 的骨骼遍历补丁有害**：`force_update`（`src/armature.cpp:579`）本已调用 `advance(0.0)`，
  即 `Armature::advanceTime`（`thirdparty/dragonBones/armature/Armature.cpp:210-222`），其中已含
  `bone->update()` 全体骨骼 + `slot->update()`。额外插入一层 `bone->update(-1)` 会把 rest pose 的槽位盒
  整体平移一个 per-slot 常量（如 `back_hair` 偏移 (587.69, -1673.84)），实测与数据自身完全不符。

**裁决口径**（以数据自带的 `aabb` 为权威参照，`DragonBones Pro` 导出的绑定姿态包围盒）：
用 `(槽位名, 顶点数)` 精确配对，只取 idle 的 37 个蒙皮槽位，比较运行时顶点盒并集与 `aabb` 四条边。
判据脚本为临时的离线 Python 复算（解析 `idle_ske.json` 的 `vertices` + `weights` + `slotPose` + `bonePose`，
按 `src/slot.cpp:_updateMesh` 的公式重算期望盒），未入库；复现方法见
`.trellis/spec/gdextension/type-conversion.md` § Narrow Runtime Buffers → "Verifying a data set"。

| 配置 | 并集 x | 并集 y | aabb 四边偏差 | 结论 |
|---|---|---|---|---|
| **F0 = HEAD + int32 修复（当前定稿）** | [-108.061, 1283.399] | [-1732.046, 14.517] | 0.578 / 0.110 / 0.019 / 0.080 | **四边全部吻合** |
| F3 = 双补丁 + int32 修复 | [-803.529, 587.692] | [-651.953, 1673.841] | 696.0 / 695.8 / 1080.1 / 1659.2 | 全部不吻合 |

数据 `aabb` = x∈[-107.483, 1283.509]、y∈[-1732.027, 14.597]。

### 缺陷 C / D 归因
- **缺陷 C（部件缩成一团）** 与 **缺陷 D（爆炸拉伸）** 是同一根因（int16 偏移溢出后的越界读）的两种取值后果：
  读出 0 或极小值 → 顶点塌到各骨骼关节位置附近；读出大值 → 爆炸式拉伸。随堆布局改变而不同。修复后两者一并消失。
- 缺陷 D 的剩余观察：idle 动画态两帧 md5 不同（属正常逐帧变化，非缺陷）。

### 已确认非问题（数据侧）
- `idle_ske.json` 的 37 个蒙皮槽位 weights 全部自洽（按「每顶点前导计数 + N 对 (boneIdx, weight)」格式解析：
  权重和 = 1 偏差 > 1e-4 的仅 3 处、偏差 ~1e-4；骨骼索引全部 < 44，无越界）；顶点数 == uvs 数，无截断。
- 槽位名 ←→ skin 一一对应，无缺失/多余；37 个蒙皮 + 2 个非蒙皮（`neck`、`topwear`）。
- `idle_tex.json` 的 28 个 SubTexture：无重名、无缺失引用、无未引用、无越出 4096×4096 边界。
- `uvs` 全部落在 [0, 1]。
- `neck` / `topwear` 的 display 缺 `type` 字段不构成错误——官方 `Sheep_ske.json` 同样有 8 处缺 `type`（缺省即 `image`）。
- `animation[0].slot` 24 条仅含 `colorFrame`、无 `displayFrame`，属合法数据。

## Requirements

- **R1**：`[none]` 状态下所有部件处于 rest pose 正确位置（无错位、无塌缩）。
- **R2**：播放/切换/停止动画后姿态正确，无拉伸、无碎片。
- **R3**：Dragon（无蒙皮）、Sheep（8 蒙皮网格）、idle（37 蒙皮网格）三套 demo 资源在 `[none]` 与动画两态下均正确。
- **R4**：4.3-stable 与 4.8.dev 两版引擎行为一致。
- **R5**：不引入运行时回归（现有旋转/缩放/翻转/flip、嵌套子骨架、散图模式仍正确）。

## Fix

**`src/slot.cpp` 两处改动**（`_updateMesh` 内）：

```cpp
// intArray 是 int16_t*，偏移按无符号 16 位存放：>32767 读成负数，
// 且必须用 32 位接收，否则 +=65536 回绕失效，强转后越界读出垃圾数据。
int32_t weightFloatOffset = intArray[weightData->offset + (unsigned)BinaryOffset::WeightFloatOffset];
if (weightFloatOffset < 0) {
    weightFloatOffset += 65536;
}
```

`hasFFD` 分支的 `vertexOffset` 同样改为 `int32_t`，去掉会让 `if (vertexOffset < 0)` 恒为假的 `(std::size_t)` 中间转换。

`src/factory.cpp` 与 `src/armature.cpp` 保持 HEAD 原状。

## Acceptance Criteria

1. 编辑器打开 demo：三套资源在 `[none]` 态下部件全部贴合骨架 rest pose（与 DragonBones Pro 中的绑定姿态一致）。
2. 切换动画并停止回 `[none]`：姿态正确往返，无残留错位。
3. 运行时（游戏内）同样正确，且前后帧一致。
4. 无新增编辑器/运行时错误输出。

## Out of Scope

- 编辑器选中范围/点击/拖动功能（已在 PR #81 交付，分支 `feat/armature-view-selection-bounds`）。
- 引擎钩子相关改动（`_edit_*` 不可达结论已在前任务确认）。

## Open Questions

- 无。缺陷 C 成因已定位（int16 偏移溢出），与缺陷 D 同源。

## Verification

- 构建：`scons platform=windows target=template_debug debug_symbols=yes -j8`，无警告/错误。
- 冒烟：`godot --path demo --headless --quit-after 250` × 3 轮，`rc=0`、错误输出 0 条。
- 确定性：修复后多次窗口渲染 `md5` 完全一致（修复前存在时隐时现的塌缩/拉伸）。
- 几何裁决：见上文表格，F0 与数据 `aabb` 四边吻合（最大偏差 0.578），F3 全部不吻合。
- 视觉复核：窗口渲染截图可辨认完整人形（头发/脸/眼/嘴/衣服/手臂/手/奶瓶），无部件塌缩或尖刺拉伸。
