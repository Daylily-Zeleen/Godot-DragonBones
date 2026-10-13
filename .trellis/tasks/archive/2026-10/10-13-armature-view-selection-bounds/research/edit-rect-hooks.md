# Q1：编辑器选择范围虚函数（`_edit_*`）的暴露情况与可覆盖性

调研输入（本地可证）：

- 本仓库 `thirdparty/godot-cpp` = commit `b051e694cf493d417f5aa945c656cb77ae9011a9`（`git log -1`）。
- 引擎源码：本地 `D:/Dev/godot/godot`（HEAD `9d6da1df747a1731ab2bce95225bc87b3afaa578`，`version.py`：4.8.dev，分支 `daylily-zeleen/self_use`）；同一仓库内存在 `4.3-stable` … `4.7-stable` 标签，用于逐版本核对（`git show <tag>:<path>`）。
- 结论标注：`[可证]` = 本地文件/命令直接得出；`[推断]` = 由代码机制推导（附依据）；`[未验证]` = 本地无法确认（无网络）。

## 0. 结论（摘要）

1. `[可证]` godot-cpp b051e69 **完全没有暴露任何 `_edit_*`**：
   - `thirdparty/godot-cpp/gen/include/godot_cpp/classes/canvas_item.hpp:58` 声明 `class CanvasItem`；其中唯一虚函数是 `_draw`（`:177 virtual void _draw();`，绑定在 `:181-185`，`:184 BIND_VIRTUAL_METHOD(T, _draw, 0)`）。
   - `thirdparty/godot-cpp/gen/include/godot_cpp/classes/node2d.hpp:47` 声明 `class Node2D`；`:87-88` 的 `register_virtuals` 仅链式转发给 `CanvasItem`，**没有任何虚函数**。
   - 手写目录 `thirdparty/godot-cpp/include/godot_cpp/classes/` 只有 3 个文件：`editor_plugin_registration.hpp`、`ref.hpp`、`wrapped.hpp`，同样没有 `_edit_*`。
   - 对 godot-cpp 全子模块 grep `_edit_` 只命中 5 份 `gdextension/extension_api-4-{3,4,5,6,7}.json` 中无关的 `set_edit_alpha` / `set_rotation_edit_mode` 等，以及构建产物 `.obj`；`_edit_get_rect` / `_edit_use_rect` / `_edit_get_transform` / `_edit_is_selected_on_click` 在任何 API dump 中都不存在（python 解析 5 份 dump：`CanvasItem` 的 virtual 只有 `['_draw']`，`Node2D` 为空，`Control` 为 `_has_point/_gui_input/...` 等）。
2. `[可证]` 引擎里 `_edit_*` 确实存在，但全部是**普通 C++ 虚函数**（`virtual ...` + `override`），既不是 `GDVIRTUAL`，也没有进入 extension API dump。=> **GDExtension 扩展类无法覆盖它们**（详见第 3 节机制链）。
3. `[可证]` 编辑器侧也**没有脚本回落**：对 4.3-stable `editor/plugins/canvas_item_editor_plugin.cpp` 与 4.7-stable `editor/scene/canvas_item_editor_plugin.cpp` grep `has_method` / `get_script_instance` 均为空 —— 纯 C++ 虚调用，GDScript 也覆盖不了。
4. `[可证]` 编辑器如何画“选中范围”（4.3-stable `editor/plugins/canvas_item_editor_plugin.cpp:3493-3517`）：
   - `if (ci->_edit_use_rect())` → 用 `ci->_edit_get_rect()` 的四角连线画选中框（颜色 `Color(1, 0.6, 0.4, 0.7)`）；
   - `else` → 在 `ci->_edit_get_transform()`（此处 4.3 写法为 `xform * ci->get_transform().affine_inverse() * ci->_edit_get_transform()`）位置画 `position_icon` 贴图 —— 即当前看到的“默认选框/图标”。
   - `Node2D` **不覆盖** `_edit_get_rect` / `_edit_use_rect`（4.3-stable `scene/2d/node_2d.h:59-74` 只覆盖 state/position/scale/rotation/`_edit_set_rect`），所以 Node2D 派生的扩展节点天然走 `else` 分支。
5. `[可证]` 需求里提到的其余几只的实际语义（4.3-stable 行号）：
   - `_edit_set_rect`（`:220`）是“拖动缩放手柄时写回节点”的入口（`Node2D::_edit_set_rect` 实现于 `scene/2d/node_2d.cpp:85+`，内部 `ERR_FAIL_COND(!_edit_use_rect())`）；
   - `_edit_get_rotation`（`:216`）默认返回 0（`Node2D` 覆盖，见 `scene/2d/node_2d.h:70-71`）；
   - `_edit_is_selected_on_click`（`:199`）默认：有 rect → 命中 rect；无 rect → `p_point.length() < p_tolerance`（`scene/main/canvas_item.cpp:48-54`）—— 这就是“点击精灵本体（远离原点）选不中”的直接原因；
   - `_edit_get_transform`（`:229`）是唯一的“编辑用变换”，并在 `scene/main/canvas_item.cpp:1160` 被 `ClassDB::bind_method` 绑定（`#ifdef TOOLS_ENABLED` 块内，`#endif` 在 `:1161`）。
6. `[可证]` GDExtension **可用**的替代钩子已存在：
   - `thirdparty/godot-cpp/gen/include/godot_cpp/classes/editor_plugin.hpp:119-121`：`virtual void _forward_canvas_draw_over_viewport(Control *p_viewport_control);`（以及 `_forward_canvas_force_draw_over_viewport`、`_forward_3d_draw_over_viewport`），并在 `:146-151` 用 `BIND_VIRTUAL_METHOD` 注册 → 可从扩展覆盖；
   - `:105 int32_t update_overlays() const;` 已绑定（触发叠加层重绘）。
   - 引擎侧投递链：4.7-stable `editor/plugins/editor_plugin.h:123-124`（`GDVIRTUAL1(_forward_canvas_draw_over_viewport, Control *)`）、`:189-190`、`editor/plugins/editor_plugin_list.cpp:65-74`（逐个插件 `forward_canvas_draw_over_viewport`）。

## 1. `_edit_*` 完整签名清单（4.3-stable `scene/main/canvas_item.h:196-230`）

```cpp
196  /* EDITOR */
197  #ifdef TOOLS_ENABLED
199  virtual bool _edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const;
202  virtual void _edit_set_state(const Dictionary &p_state) {}
203  virtual Dictionary _edit_get_state() const { return Dictionary(); }
206  virtual void _edit_set_position(const Point2 &p_position) = 0;   // 纯虚
207  virtual Point2 _edit_get_position() const = 0;                  // 纯虚
210  virtual void _edit_set_scale(const Size2 &p_scale) = 0;         // 纯虚
211  virtual Size2 _edit_get_scale() const = 0;                      // 纯虚
214  virtual bool _edit_use_rotation() const { return false; }
215  virtual void _edit_set_rotation(real_t p_rotation) {}
216  virtual real_t _edit_get_rotation() const { return 0.0; }
219  virtual bool _edit_use_rect() const { return false; }
220  virtual void _edit_set_rect(const Rect2 &p_rect) {}
221  virtual Rect2 _edit_get_rect() const { return Rect2(0, 0, 0, 0); }
222  virtual Size2 _edit_get_minimum_size() const { return Size2(-1, -1); }
225  virtual bool _edit_use_pivot() const { return false; }
226  virtual void _edit_set_pivot(const Point2 &p_pivot) {}
227  virtual Point2 _edit_get_pivot() const { return Point2(); }
229  virtual Transform2D _edit_get_transform() const;
     #endif  // ≈:230
```

对照：同文件 `:183` 才是 `GDVIRTUAL0(_draw)`。**`_edit_*` 没有任何 GDVIRTUAL 宏** → 没有跨 GDExtension 边界的 trampoline（见第 3 节）。

## 2. 逐版本 guard / 行号表（`scene/main/canvas_item.h`）

| 引擎版本 | `TOOLS_ENABLED` 块 | `DEBUG_ENABLED` 块 | 备注 |
|---|---|---|---|
| 4.3-stable | `:197-230`（全部 `_edit_*` 都在这里） | 无独立块 | `_edit_is_selected_on_click:199`、`_edit_use_rect:219`、`_edit_get_rect:221`、`_edit_get_transform:229` |
| 4.4-stable | `:206-234` | `:236-245` | rect 三只移入 DEBUG：`_edit_is_selected_on_click:240`、`_edit_use_rect:243`、`_edit_get_rect:244` |
| 4.5-stable | `:211-239` | `:241-250` | `:245` / `:248` / `:249` |
| 4.6-stable | `:221-249` | `:251-260` | `:255` / `:258` / `:259` |
| 4.7-stable | `:…-274` | `:276-285` | `:280` / `:283` / `:284`；DEBUG 块内注释（`:277-278`）：“Those need to be available in debug runtime, to allow for node selection.” |
| 4.8.dev（本地工作树） | `:246-274` | `:276-285`（`:280` / `:283` / `:284`） | 结构与 4.7 相同 |

`[推断]`（依据上表 + 4.7:277-278 注释）：自 4.4 起，`_edit_is_selected_on_click` / `_edit_use_rect` / `_edit_get_rect` 在**非 tools 的 debug 运行期**（`template_debug`，`DEBUG_ENABLED`）也存在——用于运行期节点选择；但性质没变，仍是普通 C++ 虚函数，扩展依旧无法覆盖。

各版本其它 `_edit_*` 行号（4.3 起往上看表；此处仅列差异点）：

- 4.4-stable：`_edit_set_rect:225`、`_edit_get_transform:233`
- 4.5-stable：`_edit_set_rect:230`、`_edit_get_transform:238`
- 4.6-stable：`_edit_set_rect:240`、`_edit_get_transform:248`
- 4.7-stable：`_edit_set_rect:265`、`_edit_get_transform:273`

## 3. 机制链：为什么“扩展类里写同名 C++ 虚函数”不会生效

### 3.1 对象模型：引擎对象与扩展实例是两个对象 `[可证]`

- `thirdparty/godot-cpp/src/classes/wrapped.cpp:55-79`（`Wrapped::Wrapped(const StringName &p_godot_class)`）：
  - `_owner = gdextension_interface::classdb_construct_object*(p_godot_class…)` —— **引擎侧对象由引擎构造**（引擎自己的 C++ 类与 vtable）；
  - 随后 `object_set_instance(_owner, extension_class_name, this)` 把**扩展侧实例 `this`** 绑到引擎对象上；
  - `wrapped.hpp:57` 注释：“Base for all engine classes, to contain the pointer to the engine instance.”，成员 `GodotObject *_owner`（`wrapped.hpp:144`）。
- 实例化路径：引擎 4.3-stable `core/object/class_db.cpp:530-536`（`_instantiate_internal` → `extension->create_instance(...)`）；godot-cpp 侧 `include/godot_cpp/core/class_db.hpp:114-131`（`_create_instance_func<T>` 里 `new (DefaultAllocator{}) T`，并 `return new_object->_owner;`）。
- 即：`CanvasItem*` 指向的是**引擎对象**；扩展的 `DragonBonesArmatureView` C++ 对象通过 `object_set_instance` 挂在它身上。

`[推断]`（依据上面三条）：引擎对 `CanvasItem*` 的调用分两类——
- **普通 C++ 虚函数**（`_edit_*`）走引擎对象的 vtable → 落在引擎实现上，扩展里同名函数不会被调用；
- **GDVIRTUAL** 走第 3.2 节的 trampoline，能进扩展。

### 3.2 GDVIRTUAL（如 `_draw`）为什么可以 `[可证]`

- 引擎侧生成器 `core/object/make_virtuals.py:21-36`（4.3/4.8 均同构）：trampoline 先查脚本实例，随后
  `if (_get_extension()) { … _get_extension()->call_virtual_with_data(_get_extension_instance(), &_gdvirtual_<name>_sn, _gdvirtual_<name>, args…) }`；
  其中 `_gdvirtual_<name>` 由 `_gdvirtual_init_method_ptr(hash, ptr, name, compat)` 解析（同文件 `:22-27`）。
- 扩展侧注册：`godot-cpp/include/godot_cpp/core/class_db.hpp:227-233`（`BIND_VIRTUAL_METHOD` 宏）→ `src/core/class_db.cpp:338-350`（`ClassDB::bind_virtual_method` 存入 `type.virtual_methods[p_method] = {p_call, p_hash}`）→ 引擎通过 `GDExtensionClassCreationInfo.get_virtual_func = &ClassDB::get_virtual_func` 查询（`class_db.hpp:283-288`；引擎侧占位实现 4.8 工作树 `core/object/class_db.cpp:227-229` `placeholder_class_get_virtual`）。
- 该链路只对“引擎里有 `_gdvirtual_*` trampoline 且名字/哈希匹配”的方法生效。`_edit_*` 没有 trampoline、也不在 API dump 中 → 既不生成 godot-cpp 声明，也不可能注册覆盖。

### 3.3 补给：为什么“同名虚函数”在 C++ 编译期也困难

在扩展里写 `Rect2 _edit_get_rect() const` 时，`godot::CanvasItem`（godot-cpp 生成头）**没有**这个成员，无法 `override`；即便不带 `override` 声明也只是扩展类自己的新函数，与引擎 vtable 无关（承接 3.1 的 `[推断]`：引擎调用不会命中它）。

## 4. 编辑器实际调用点（后续验收/回归对照）

4.3-stable `editor/plugins/canvas_item_editor_plugin.cpp`：

- `:334-338` / `:373-375` / `:398-400` / `:408-409`：吸附（snapping）使用 `_edit_use_rect()` + `_edit_get_rect()`
- `:546-552` / `:587`：`_get_item_rects` 类逻辑用 `_edit_get_rect()`
- `:638`：点击选取 → `ci->_edit_is_selected_on_click(...)`
- `:736-740`：框选判定 → `_edit_use_rect()` + `_edit_get_rect()`
- `:854-857`：拖动前记录 `se->pre_drag_rect = ci->_edit_get_rect()`（否则 `Rect2()`）
- `:1730-1731` / `:1798` / `:3474` / `:3539-3540`：移动/缩放手柄与拖动前预览
- `:3493-3517`：**选中框绘制**（`use_rect` ? 画 rect : 画 `position_icon`）→ 与“默认选框”现象对应

4.7-stable `editor/scene/canvas_item_editor_plugin.cpp`（同构，行号变化）：

- `:359-361` / `:398-400` / `:423-434`：吸附与中心点
- `:583-589` / `:624`：rect 收集
- `:682`：点击选取
- `:788-789`：框选
- `:915-916`：`_save_canvas_item_state` 记录 `pre_drag_rect`
- `:1829`：缩放手柄
- `:3777-3841`：选中框绘制（`:3840` 为 `position_icon` 的 else 分支）
- `:4125-4136`：未选中时画 `EditorPositionUnselected`

## 5. 不确定项 / 需人工确认

1. `[未验证]` 官方文档是否明文声明“GDExtension 不能覆盖非 GDVIRTUAL 的 C++ 虚函数”（本地无网络，无法引用文档原文）。本文件的“不可覆盖”结论来自引擎/godot-cpp 源码机制，且与“godot-cpp 未生成对应声明”这一事实一致。
2. `[未验证]` 未来引擎版本是否会把 `_edit_*` 改为 GDVIRTUAL（4.8.dev 仍为普通虚函数；若上游改动，本结论需复检）。
3. `[未验证]` `_forward_canvas_draw_over_viewport` 的叠加层与编辑器内置选中框/手柄的**绘制先后顺序**（是否会被内置选框遮挡）——需在编辑器里实测。
4. `[未验证]` 运行时（`template_debug` + 远端场景树）节点选择 UI 是否读取 `_edit_get_rect()`（4.7 注释暗示会读；本次未追实现）。
