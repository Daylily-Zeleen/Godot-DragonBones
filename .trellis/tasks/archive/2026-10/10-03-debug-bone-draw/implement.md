# 实现方案：调试模式下绘制锥状骨骼与骨骼名称

> 代码标识符保持英文原文，说明用中文。

## 目标

`DragonBonesArmatureView::debug == true` 时，在现有 slot 线框之上叠加绘制骨骼：
锥状（楔形）图形 + 骨骼名称文字；并递归覆盖嵌套 armature。

---

## 一、坐标系（已确认）

- `DragonBonesBone::get_global_transform()` 返回 **armature 局部空间**变换
  （底层 `boneData->global`，由 `dragonBones::Bone::updateGlobalTransform()` 在
  armature 空间内累积）。
- `DragonBonesArmature::transform` 是该 armature 自身的偏移
  （`src/armature.cpp:188`：`p_base_transfrom * transform`）。
- 单层：骨骼到 view 空间 = `armature->transform * bone->get_global_transform()`。
- 嵌套：逐层累乘，见第四节。

---

## 二、嵌套 armature 会递归（已确认，此前判断有误）

**结论：会递归。** 证据链：

1. `thirdparty/dragonBones/armature/Slot.cpp:299-300`
   ```cpp
   _childArmature = static_cast<Armature*>(displayPair.first);
   _display = _childArmature->getDisplay();   // 子 armature 的 display
   ```
2. `Slot::getDisplay()`（`Slot.h:439-442`）返回 `_display`，即上述子 armature 的 display。
3. `src/slot.h:57`：`Display *get_display() const { return static_cast<Display *>(getDisplay()); }`
4. `DragonBonesArmature` 继承 `Display`（`src/armature.h:58`），override 了
   `append_draw_data`（`src/armature.h:150`）。
5. `src/armature.cpp:191-192` 的 `display->append_draw_data(...)` 因此**会进入子 armature**。

> 之前误判为"不递归"，是因为只看到 `for_each_armature` 里判断 `DisplayType::Armature`
> （`src/armature.h:121`），没有追到 `Slot::getDisplay()` 的返回值。

**含义**：绘制骨骼必须同样递归，否则嵌套 armature 的骨骼不会显示。

---

## 三、骨骼长度

上游 `dragonBones::BoneData::length` 存在（`thirdparty/dragonBones/model/ArmatureData.h:362`），
由 `JSONDataParser.cpp:348` 从骨骼数据 `"length"` 字段读取并乘 armature scale。
**`DragonBonesBone` 未暴露** → 新增 `get_length()`。

### `length == 0` 的处理（最终实现）

上游长度用于判定是否绘制筝形：`has_kite = length > PREFER_KITE_LENGTH_RATIO * radius`
（比例 2.5）。**长度不足时只画枢轴圆环 + 圆心连线，不画筝形**，也没有单独的
「红色粗线框锥体」路径——短骨与长骨共用同一套圆环/连线图元，只是少了筝形。

> 早期调研提到参照 Spine 用特殊图元（红色空心锥体、十字）区分零长度骨骼，
> 最终未采用：短骨退化为圆环 + 短线已足够表意，且避免引入第二条视觉规则。

---

## 四、绘制方式：复用 `debug_mesh`，保持现有风格

**结论：不新建 mesh RID，直接在 `debug_mesh` 上加 surface。**

> ⚠️ **本节是规划阶段的方案，最终未完全采用**，仅作历史记录。最终实现见
> 「描边修复记录（2026-10-05）」：改用 **着色器描边**（而非本节的
> `PRIMITIVE_LINES` 描边），并把网格拆成 `wire_mesh`（线框，画在 owner 画布项）
> 与 `debug_mesh`（骨骼，画在独立 `debug_canvas` 画布项）。几何形状也从本节的
> 「截头棱锥」改为「枢轴圆环 + 筝形 + 圆心连线」。

- 现有调试绘制走 `RenderingServer`（`src/armature_view.cpp:477-486`）：
  `mesh_clear` → `mesh_add_surface_from_arrays(PRIMITIVE_LINES)` → `canvas_item_add_mesh`。
- `mesh_add_surface_from_arrays` 可**多次调用**，同一 mesh 允许多个 surface
  （`extension_api-4-3.json` 中 `RenderingServer::mesh_add_surface_from_arrays`
  无"只允许一个 surface"约束）。
- 所以骨骼部分只需再调一次 `mesh_add_surface_from_arrays`，用不同的 primitive：
  - 锥体填充：`PRIMITIVE_TRIANGLES`（值 3）
  - 锥体描边：`PRIMITIVE_LINES`（值 1）
  - 零长度骨骼的粗线框：`PRIMITIVE_LINES`

`draw_colored_polygon` 没有性能优势，反而会引入与现有 `RenderingServer` 路径不一致的
第二条绘制链路（CanvasItem 命令 vs mesh），**放弃**。

### 锥体几何

```
        start (大端)              perp = 方向的单位垂直向量
        |====|  w_start
        |    \
        |     \
        |--|    w_end
        end (小端)
```

顶点：`start ± perp * w_start`、`end ± perp * w_end`；两个三角形索引。
`w_start > w_end` 实现「大端起点、小端终点」。

宽度建议 `w_start = clamp(length * 0.12, MIN_W, MAX_W)`，`w_end = w_start * 0.35`。

### 描边/层次（解决"纯色太平坦"）

按需求自由发挥，采用**描边 + 轻微明暗梯度**：

1. 锥体填充用 `debug_color` 的**暗化版本**（如 `debug_color.darkened(0.35)`）作底色；
2. 沿锥体轮廓画一圈 `PRIMITIVE_LINES` 描边，用 `debug_color` 的**亮化版本**
   （如 `debug_color.lightened(0.5)`）；
3. 顶点色可做梯度：大端略亮、小端略暗，强化方向感。

这样既保留 `debug_color` 的随机区分度，又有清晰边界。

> 先按 `debug_color` 实现看效果，效果不佳再换固定色。

---

## 五、骨骼名称文字

### 字体

`ThemeDB::get_singleton()->get_fallback_font()`，**不硬编码**字体路径。
`draw_string` 签名（`extension_api-4-3.json`，CanvasItem）：
```
draw_string(font, pos, text, alignment=-1, width, font_size=16, modulate, ...)
```
`draw_string_outline` 同在，第 8 参 `size` 为描边宽度。

### 缩放调研（结论：屏幕空间固定大小更符合行业做法）

| 软件 | 做法 |
|---|---|
| **Blender** | 3D 视口骨骼名称是**固定屏幕像素大小**，不随视图缩放。用户普遍反馈"太小看不清"，而**官方没有提供字号设置**——只能改系统 DPI 曲线救国（blenderartists.org/t/enlarging-names-of-bones/626657）。 |
| **Spine** | 名称在视口渲染，且做**重叠避让**（"if it doesn't overlap another name"），骨骼有独立 `Name` 开关。 |
| **龙骨/LoongBones** | 编辑器内骨骼与名称同为编辑辅助层，随舞台缩放。 |

**建议**：采用**屏幕空间固定大小**（不随节点缩放）。理由：

1. 这是 Blender 等 DCC 工具的既定做法，用户有预期；
2. 调试信息属于"辅助层"，不是场景内容——角色被缩到很小时名称仍应可读；
3. 随缩放会导致极端缩放下文字不可读或过度占据画面。

**实现方式**：`_draw()` 内文字尺寸按当前 view 的实际缩放反向补偿：

```cpp
// 取节点到视口的总缩放，字号反向补偿，使屏幕像素大小恒定
const float screen_scale = get_global_transform_with_canvas().get_scale().length();
const int font_size = Math::round(BASE_FONT_SIZE / MAX(screen_scale, 0.0001f));
```

> 需确认 `get_global_transform_with_canvas()` 在此场景是否可用；若不可用，
> 退回用 `get_viewport_rect()` / canvas transform 计算。

**代价**：需要每帧做一次除法并重建字形缓存（字号变化时字体栅格化会重来）。
若动画中缩放恒定则无影响；缩放频繁变化时需评估开销。

**备选（更简单）**：字号固定不补偿。优点是零开销、实现最简；缺点是节点缩小时文字跟着缩小。
建议**先实现固定字号版本，再叠加屏幕空间补偿**，分两步验证效果。

---

## 六、实施步骤

1. `src/bone.h` / `bone.cpp`：新增 `float get_length() const;`
   （读 `boneData->getBoneData()->length`），绑定 ClassDB + `ADD_PROPERTY` 只读属性。
2. `src/armature.h`：新增仅供 `DEBUG_ENABLED` 的骨骼收集入口，例如
   `void append_bone_debug_data(PackedVector2Array &r_verts, PackedColorArray &r_colors,
   const Transform2D &p_base_transform) const;` —— 内部遍历自身 `bones`，
   并对子 armature 递归（沿 `for_each_armature` 或复用 `append_draw_data` 的
   display 递归链，累乘各自 `transform`）。
3. `src/armature_view.cpp`：`_draw()` 的 debug 分支内：
   - 收集骨骼锥体顶点/索引/颜色；
   - 在 `debug_mesh` 上追加 `PRIMITIVE_TRIANGLES`（填充）与 `PRIMITIVE_LINES`（描边/零长度）surface；
   - 之后用 `draw_string_outline` + `draw_string` 叠加名称。
4. 全部包在 `#ifdef DEBUG_ENABLED` 内；`debug == false` 直接跳过。

---

## 七、验收

- `debug = true`：骨骼锥体（含描边）+ 名称显示；`length == 0` 的骨骼显示为红色粗线框锥体。
- 嵌套 armature 的骨骼同样显示。
- 名称在节点缩放时保持可读（屏幕空间固定大小）。
- 随动画实时更新。
- `debug = false` 行为与现状一致。
- `template_debug` / `template_release` 本地构建通过；推 PR 后全平台 CI 通过。


---

## 实施记录（2026-10-03）

### 形状：截头棱锥（Blender octahedral 式）

最终形态是**一端大、一端小**的梯形锥：横截面垂直于骨骼，起点处最宽，向终点收窄
（尖端宽度为根部的 `DEBUG_BONE_TIP_RATIO = 0.4`）。填充色在根部略亮、梢部略暗。

中途一度改成"两端都是尖顶、中部最宽"的双尖菱形，那是把"棱锥"理解错了，已改回。

### 朝向：局部 X 轴

骨骼方向取 `bone_transform.columns[0]`（局部 X 轴）后乘 `length`。

依据是上游 IK 约束：`thirdparty/dragonBones/armature/Constraint.cpp:58-59`
用 `globalTransformMatrix.a / .b * boneLength` 计算骨骼尖端，即沿局部 X 轴。

关于 DragonBones 数据的 `skX` / `skY`：`JSONDataParser.cpp:1802-1805` 在遇到
`skX`/`skY` 格式时解析为 `rotation = skY`、`skew = skX - skY`，所以 rotation
并不一定是 0，X 轴本身就承载了方向。Dragon 示例里 `skX == skY` 恒成立，
于是 `skew = 0`、`rotation = skY`，方向角恰为 `skY`；Godot 侧旋转符号相反，
故屏幕上看到的方向是 `-skY`。运行时实测与之吻合：

| 骨骼 | `skY` | 实测 `xaxis` | 方向 |
|---|---|---|---|
| `body` | -94.97 | -94.97 | 向上 |
| `legR` | -156.01 | -156.01 | 左下 |
| `head` | 11.50 | 11.50 | 右上 |
| `tail` | 79.74 | 79.74 | 下方 |

> 实施过程中曾把方向误改为 `columns[1]`（Y 轴），起因是一个验证脚本把
> `gt.x.x` 当成 `Transform2D.x` 属性的分量来读——`.x` 本身已是 `columns[0]`，
> 于是读到的是错的值。结论建立在错误数据上，代码也跟着错了一次。已回滚。

### 零长度骨骼：按子/父骨骼距离回退

骨架数据里没有 `length` 的骨骼（`root`、`eyeL`、`eyeR`、`IK_Lhand` 等），
长度回退为「到第一个子骨骼的距离」；末端骨骼取「到父骨骼的距离」；仍为 0 则不画。
绘制为空心红色，描边加粗（外环 + 内缩内环 + 连接线段）以区别于正常骨骼。

`PRIMITIVE_LINES` 在 Godot 中不保证线宽，所以加粗靠几何实现而非线宽参数。

> 中途改成过"固定小尺寸标记"，理由是辅助骨骼的子骨骼很远会让标记横跨骨架；
> 但那偏离了既定规则，已改回。

### 骨骼宽度：屏幕像素恒定

`DEBUG_BONE_ROOT_HALF_WIDTH_PX = 3.0f` 是**屏幕像素**值，使用时除以
`get_global_transform().get_scale().y` 换算为局部单位。

最初是长度比例（`0.12 × length`，上限 12），而骨骼长度在 Dragon 示例里
相差 5 倍（`hair` 50 到 `tail` 250），比例会让长骨骼变成横跨半个角色的色块。
改成像素恒定后，粗细不随骨骼长度和缩放变化。

### 字号

**最终实现：固定 14px，不做缩放补偿。**
`font_size = DEBUG_BONE_NAME_FONT_SIZE`（14）。

调研阶段曾考虑屏幕空间固定字号（按 `get_global_transform_with_canvas().get_scale()`
反向补偿），该补偿逻辑一度实装，但已在 `a4ed9b2` 随绘制代码简化一并删除。
现行行为是固定像素字号：节点缩放时文字随之缩放，零开销、零字形缓存重建。

保留调研结论供将来参考：Blender 的 3D 视口骨骼名是固定屏幕像素大小，且官方不提供
字号设置；若要改回屏幕空间固定字号，除数须取 `get_scale().y` 而非向量长度——
均匀 (3,3) 缩放的向量长度是 3√2，会算错。

## build_profile.json 新增类

`Font`、`ThemeDB`、`TextServer`、`GlobalConstants`、`Material`。
`draw_string` 被裁掉的原因是 profile 会剔除参数/返回值类型不在白名单的方法，
其签名引用了 `Font` 与 `TextServer` 枚举；`CanvasItemMaterial` 依赖父类 `Material`。

## 验证样本

对照 DragonBones 编辑器截图时必须用 `Dragon/Dragon_ske`（19 根骨骼）。实施中
一度用 `龙/龙_ske`（60 根骨骼）做对比，样本不同导致对不上。

## 描边修复记录（2026-10-05）

### 材质挂载位置（根因一）

2D 画布网格走 `Item::CommandMesh`，渲染时**只读画布项自身的材质**，从不读
`mesh_surface_get_material()`。实测 Godot 4.3 的 `gl_compatibility` 与 `forward_plus`
都忽略 surface 材质（参考 `renderer_canvas_cull.cpp:1763`、`renderer_canvas_render_rd.cpp:2216`；
4.3 为 `renderer_canvas_render_rd.cpp:1210`）。

因此拆成两条链路：

- `debug_mesh` —— 骨骼填充几何，画在独立的 `debug_canvas` 画布项上，材质挂在该画布项。
  独立画布项是必须的：owner 画布上还画着龙骨本体，直接把材质挂上去会把本体一起染色。
  `debug_canvas` 延迟到 `draw()` 里创建，因为 `set_enabled()` 可能被属性设置器在入树前调用。
- `wire_mesh` —— 线框，直接画在 `owner->get_canvas_item()`，**不挂材质**，故完全不需要
  UV 约定（原先为绕开材质而写的 `line_uv = Vector2(0, 1e6)` 已整段删除）。

`~DebugDraw()` 与 `set_enabled(false)` 都释放这两个网格与画布项。

### UV 约定（最终版）

**每个顶点的 `UV = (到形状中线的横向偏移, 该处半宽)`**，片元里

```
float d = abs(UV.y) - abs(UV.x);
```

即到轮廓的有符号距离：轮廓处 0，内侧为正。再用 `fwidth(d)` 换算成像素，
落在 `outline_px` 内就着描边色。`UV.y` 的符号兼作标志位：负 = 该骨受 IK 约束，
描边改用橙色。

`d = 0` 的零集在 UV 空间里是过原点的 V 形（`|UV.x| = UV.y`）。映射回顶点空间，
**一个三角形里它恰好是从同一个顶点出发的两条边**。因此只要给顶点分配正确的
UV，零集就能贴合任意两条边界——**无需新增任何顶点或三角形**。这也是
`HEAD` 注释「顶点、三角形都不多加」的真正含义。

各形状的 UV 分配：

| 形状 | 三角化 | UV 分配 |
|---|---|---|
| 实心圆盘 `body_circle` | HEAD 原样 | 所有顶点同 UV → `d` 恒定 → 不描边 |
| 环 `body_annulus` | HEAD 的 1 个 `body_quad` | 外沿 `(hw, hw)`、内沿 `(-hw, hw)`，两侧 `|UV.x|` 都等于 `hw` → 内外圈同时 `d = 0` |
| 线段 `body_segment` | HEAD 的 2 个三角形 | 两侧长边分别取 `(hw, hw)` 与 `(-hw, hw)` |
| 端帽 `body_cap` | HEAD 的扇形 | 弧上点 `(radius, radius)`、圆心 `(0, radius)` |
| 筝形 `body_kite` | **HEAD 的 3 个三角形、5 个顶点，不变** | `head (0,0)`、`spring_a (wide, wide)`、`spring_b (-wide, wide)`、`tip_a (0,0)`、`tip_b (-tip, tip)` |

筝形是唯一需要仔细推导的：HEAD 的 3 个三角形逐个数，零集恰好覆盖 5 条棱边——
`(head, sa, sb)` 给出两条肩、`(sa, ta, tb)` 给出腰 `sa-ta` 与小端、`(sa, tb, sb)`
给出腰 `tb-sb`。关键是 `head` 与 `tip_a` 的 UV 必须取 `(0, 0)`：HEAD 把它们填成
`(±wide, wide)` 与 `(±tip, tip)`，使这些顶点被误判在轮廓上，整片糊色。

### 数值验证（模拟重心插值）

```
筝形 5 条棱边中点 d = 0.000，内部点 d = 10.9 ~ 26.9
环 外沿 / 内沿 d = 0.000，壁中 d = 4.000
端帽 弧中点 d = 0.000，圆心 d = 10.000
```

隔离工程（照抄 src 的 UV 与三角化）逐像素扫描：

```
KITE 两肩 : MMM oooo...oooo MMM     KITE 小端 : MM oooo MM
KITE 两腰 : MM ooo...ooo MM         SEG 长边  : MM oooooooo MM
SEG 端帽  : MMM oooo...             RING 环壁 : MMM ooooooo MM   ← 内外圈都描
```

实机 demo（`radius = 40`）逐像素扫描：水平 801 处、垂直 897 处 `M..oo..M`
剖面（两侧描边 + 灰芯）。

### 已知非缺陷

- 静态 `Ref<ShaderMaterial>` 在进程退出时报 RID 未释放。这是 `HEAD` 就有的行为
  （`git show HEAD:src/debug_draw.cpp` 同为函数内 `static`），非本轮引入，不影响运行。
- 实机截图上「连线整条洋红」是截图压缩 + 二三十根骨骼叠压导致的判读失真；单根隔离与
  逐像素扫描均确认灰芯存在。
- `outline_px` 与形状宽度同量级时，灰芯会很窄、视觉上接近实心。这是参数问题，
  不是几何问题：连线半宽只有 `radius * 0.08`，`outline_px` 取大值就会吃掉它。

### 走过的一段弯路（已回退，保留以免重蹈）

曾把 UV 约定改成「每个顶点的 `UV.x` = 到本三角形所负责的那条轮廓边的垂直距离」。
该约定下**一个三角形只能承担一条轮廓边**，于是被迫按边重新三角化：筝形 3 → 5 个
三角形（多一个质心顶点）、连线 2 → 4 个（多一个中点顶点），环壁也拆成 4 个三角形。
这恰好废掉了用着色器描边的意义——`HEAD` 注释写得很清楚：「顶点、三角形都不多加」。
该方案已整体回退，`body_tri_outline` / `body_tri_fill` / `edge_dist` 辅助函数一并删除。
现行方案（上文「UV 约定（最终版）」）零新增几何。
