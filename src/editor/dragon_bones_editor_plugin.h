/**************************************************************************/
/*  dragon_bones_editor_plugin.h                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                           Godot-DragonBones                            */
/*        https://github.com/Daylily-Zeleen/Godot-DragonBones             */
/**************************************************************************/
/* Copyright (c) 2024-present 忘忧の (Daylily-Zeleen)                      */
/*               - Contact: daylily-zeleen@foxmail.com                    */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "../db_data.h"
#include "../factory.h"
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/editor_export_plugin.hpp>
#include <godot_cpp/classes/editor_import_plugin.hpp>
#include <godot_cpp/classes/editor_plugin.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/core/object_id.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/variant/callable.hpp>

namespace godot {
// TODO 实现运行时用的 ResourceFormatLoader

class DragonBonesExportPlugin : public EditorExportPlugin {
	GDCLASS(DragonBonesExportPlugin, EditorExportPlugin)

	PackedStringArray added_files;

protected:
	static void _bind_methods();

public:
	virtual String _get_name() const override;

	virtual void _export_begin(const PackedStringArray &features, bool is_debug, const String &path, uint32_t flags) override { added_files.clear(); }
	virtual void _export_end() override { added_files.clear(); }
	virtual void _export_file(const String &path, const String &type, const PackedStringArray &features) override;
};

class DragonBonesImportPlugin : public EditorImportPlugin {
	GDCLASS(DragonBonesImportPlugin, EditorImportPlugin)
protected:
	static void _bind_methods();

public:
	virtual String _get_importer_name() const override;
	virtual String _get_visible_name() const override;
	virtual int32_t _get_preset_count() const override;
	virtual String _get_preset_name(int32_t p_preset_index) const override;
	virtual PackedStringArray _get_recognized_extensions() const override;
	virtual TypedArray<Dictionary> _get_import_options(const String &p_path, int32_t p_preset_index) const override;
	virtual String _get_save_extension() const override;
	virtual String _get_resource_type() const override;
	virtual decltype(EditorImportPlugin()._get_priority()) _get_priority() const override;
	virtual int32_t _get_import_order() const override;
	virtual bool _get_option_visibility(const String &path, const StringName &option_name, const Dictionary &options) const override;
	virtual Error _import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options,
						  const TypedArray<String> &r_platform_variants, const TypedArray<String> &r_gen_files) const override;

public:
	Ref<DragonBonesData> try_import(const String &p_ske_file) const;
};

class DragonBonesEditorPlugin : public EditorPlugin {
	GDCLASS(DragonBonesEditorPlugin, EditorPlugin)

	friend class DragonBonesFactory;

	Ref<DragonBonesExportPlugin> export_plugin;
	Ref<DragonBonesImportPlugin> import_plugin;

	// ---- 2D 画布叠加层：为选中的 DragonBonesArmatureView 绘制贴合实际显示内容的选中范围 ----
	// 以 ObjectID 追踪选中集合（节点释放后不留悬垂指针）；连接到节点 draw 信号的 Callable 随值保存，
	// 断开时用同一实例。
	HashMap<ObjectID, Callable> selected_views;

	// 与 _input 中被消费的按下事件配对的释放事件也一并消费。
	bool body_click_consumed{ false };

	// Sprite2D 式拖动：按住显示内容即可整体拖动选中实例，释放时整段提交撤销。
	bool drag_active{ false };
	Vector2 drag_start_mouse;
	Vector<Node2D *> drag_nodes;
	Vector<Vector2> drag_start_global;
	Vector<Vector2> drag_start_local;

	void _disconnect_selected_views();
	void _on_selection_changed();
	void _drag_commit();
	// 注意：经 Callable 绑定传参，用 uint64_t（ObjectID 缺少 GetTypeInfo 特化，无法作为绑定参数）。
	void _on_selected_view_redraw(uint64_t p_view_id);

	void _on_filesystem_changed();
	void _reimport_dbdata_recursively(class EditorFileSystemDirectory *p_dir, HashMap<String, Ref<DragonBonesData>> &r_datas) const;

	void clear_reimporting_flag();

protected:
	static void _bind_methods() {}

	// 重新导入期间抑制「文件不存在」等预期报错（工厂的 get_file_data 会查询）。
	static bool reimporting;

public:
	// 2D 画布叠加层绘制：为选中的 DragonBonesArmatureView 画出贴合实际显示内容的选中范围。
	// 注意：register_virtuals 需经 decltype 访问该虚函数，必须声明在 public 段。
	virtual void _forward_canvas_draw_over_viewport(Control *p_viewport_control) override;

	// 4.4+ 编辑器只为「_handles() 命中当前被编辑对象」的插件投递 _forward_canvas_draw_over_viewport
	//（EditorNode::_edit_current -> get_handling_sub_editors -> _plugin_over_edit -> editor_plugins_over），
	// 因此必须对本类节点声明接管，否则叠加层永远不会被调用。
	virtual bool _handles(Object *p_object) const override;
	virtual void _edit(Object *p_object) override;

	// 点击命中选择：引擎对 use_rect()==false 的节点只在原点附近命中（CanvasItem::
	// _edit_is_selected_on_click 默认实现，且不可覆盖），在显示内容上点击会被当作空白处点击。
	// 这里在 GUI 分发前拦截画布左键：落在「当前实际显示内容包围盒」内即选中该节点并消费事件。
	virtual void _input(const Ref<InputEvent> &p_event) override;

	virtual void _enter_tree() override;
	virtual void _exit_tree() override;
};

} //namespace godot