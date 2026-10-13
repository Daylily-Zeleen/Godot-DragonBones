/**************************************************************************/
/*  dragon_bones_editor_plugin.cpp                                        */
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

#include "dragon_bones_editor_plugin.h"

#include "../armature_view.h"
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_file_system_directory.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/editor_selection.hpp>
#include <godot_cpp/classes/editor_undo_redo_manager.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

bool DragonBonesEditorPlugin::reimporting{ false };

// ===========================================
void DragonBonesExportPlugin::_bind_methods() {
}

String DragonBonesExportPlugin::_get_name() const {
	return "Godot-DragonBones";
}

void DragonBonesExportPlugin::_export_file(const String &path, const String &type, const PackedStringArray &features) {
	if (type != DragonBonesData::get_class_static()) {
		return;
	}

	if (path.is_empty()) {
		return;
	}

	Ref<DragonBonesData> data = ResourceLoader::get_singleton()->load(path, DragonBonesData::get_class_static(), ResourceLoader::CACHE_MODE_IGNORE);
	if (data.is_null()) {
		return;
	}

	PackedStringArray files_to_add;

	const String ske_path = data->resolve_ske_path();
	if (!ske_path.is_empty()) {
		files_to_add.push_back(ske_path);
	}

	// 仅图集模式有需要携带的 json；散图模式的纹理源是目录（普通资源，与导出无关）。
	if (data->get_texture_source_type() == DragonBonesData::TEXTURE_SOURCE_ATLAS) {
		const String atlas_path = data->resolve_texture_source_path();
		if (!atlas_path.is_empty()) {
			files_to_add.push_back(atlas_path);
		}
	}

	for (const String &fp : files_to_add) {
		if (added_files.has(fp)) {
			continue;
		}

		ERR_CONTINUE_MSG(!FileAccess::file_exists(fp), vformat("DragonBones data file not found: \"%s\".", fp));
		auto fa = FileAccess::open(fp, FileAccess::READ);
		add_file(DragonBonesFactory::convert_to_imported_path(fp), fa->get_buffer(fa->get_length()), false);
		added_files.append(fp);
	}
}

// ===========================================
void DragonBonesImportPlugin::_bind_methods() {
}

String DragonBonesImportPlugin::_get_importer_name() const {
	return DragonBonesData::get_class_static().capitalize().replace(" ", "_").to_lower();
}

String DragonBonesImportPlugin::_get_visible_name() const {
	return DragonBonesData::get_class_static();
}

int32_t DragonBonesImportPlugin::_get_preset_count() const {
	return 1;
}

String DragonBonesImportPlugin::_get_preset_name(int32_t preset_index) const {
	return preset_index == 0 ? "Default" : "";
}

PackedStringArray DragonBonesImportPlugin::_get_recognized_extensions() const {
	PackedStringArray ret;
	// 只对二进制格式进行导入
	ret.push_back(DragonBonesFactory::SRC_BIN_EXT);
	return ret;
}

TypedArray<Dictionary> DragonBonesImportPlugin::_get_import_options(const String &p_path, int32_t p_preset_index) const {
	return {};
}

String DragonBonesImportPlugin::_get_save_extension() const {
	return DragonBonesData::SAVED_EXT;
}

String DragonBonesImportPlugin::_get_resource_type() const {
	return DragonBonesData::get_class_static();
}

decltype(EditorImportPlugin()._get_priority()) DragonBonesImportPlugin::_get_priority() const {
	return 2; // 提高优先级
}

int32_t DragonBonesImportPlugin::_get_import_order() const {
	return 0;
}

bool DragonBonesImportPlugin::_get_option_visibility(const String &path, const StringName &option_name, const Dictionary &options) const {
	return true;
}

Error DragonBonesImportPlugin::_import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options,
									   const TypedArray<String> &r_platform_variants, const TypedArray<String> &r_gen_files) const {
	auto data = try_import(p_source_file);

	if (data.is_null()) {
		return FAILED;
	}

	auto ext = p_source_file.get_extension();
	auto save_path = p_source_file.trim_suffix(ext) + _get_save_extension();
	return ResourceSaver::get_singleton()->save(data, save_path);
}

Ref<DragonBonesData> DragonBonesImportPlugin::try_import(const String &p_ske_file) const {
	String base_path = p_ske_file.get_basename();
	if (!base_path.ends_with("_ske")) {
		return {};
	}

	base_path = base_path.trim_suffix("_ske");

	// 两种导出形态：
	//   图集模式：<base>_tex.json + 整张图集；
	//   散图模式（Images 导出）：无 _tex.json，<base>_texture/ 下每个部件一张独立 PNG。
	const String tex_atlas_file = base_path + "_tex.json";
	const String scattered_texture_dir = base_path + "_texture";
	const bool has_texture_atlas = FileAccess::file_exists(tex_atlas_file);
	const bool has_scattered_textures = !has_texture_atlas && DirAccess::dir_exists_absolute(scattered_texture_dir);

	if (!has_texture_atlas && !has_scattered_textures) {
		return {};
	}

	Ref<DragonBonesData> ret;
	ret.instantiate();

	// 只存文件名/文件夹名（不存绝对路径）：加载时在资源同目录解析。
	// 这样资源与数据文件一起移动即可继续工作，无需任何跟随逻辑。
	ret->set_ske_file(p_ske_file.get_file());
	if (has_texture_atlas) {
		ret->set_texture_source_type(DragonBonesData::TEXTURE_SOURCE_ATLAS);
		ret->set_texture_source(tex_atlas_file.get_file());
	} else {
		ret->set_texture_source_type(DragonBonesData::TEXTURE_SOURCE_SCATTERED);
		ret->set_texture_source(scattered_texture_dir.get_file());
	}
	ret->imported = true;

	return ret;
}

///////////////////////////////
const auto SETTING_AUTO_GENERATE_DBDATA = "Godot_DragonBones/auto_generate_dbdata";

void DragonBonesEditorPlugin::_reimport_dbdata_recursively(EditorFileSystemDirectory *p_dir, HashMap<String, Ref<DragonBonesData>> &r_datas) const {
	if (!p_dir) {
		return;
	}

	for (int32_t i = 0; i < p_dir->get_file_count(); ++i) {
		const String fp = p_dir->get_file_path(i);
		if (fp.get_extension().to_lower() != "json") {
			// 仅对json进行处理
			continue;
		}

		constexpr decltype(fp.length()) json_extension_length = sizeof("json") - 1;
		const String save_path = fp.substr(0, fp.length() - json_extension_length) + DragonBonesData::SAVED_EXT;
		if (FileAccess::file_exists(save_path)) {
			// 已存在，避免因为龙骨文件过多导致编辑器卡顿
			continue;
		}

		Ref<DragonBonesData> data = import_plugin->try_import(fp);
		if (data.is_valid()) {
			r_datas.insert(save_path, data);
		}
	}

	for (int32_t i = 0; i < p_dir->get_subdir_count(); ++i) {
		_reimport_dbdata_recursively(p_dir->get_subdir(i), r_datas);
	}
}

void DragonBonesEditorPlugin::_on_filesystem_changed() {
	if (reimporting) {
		return;
	}
	reimporting = true;

	if (ProjectSettings::get_singleton()->get_setting(SETTING_AUTO_GENERATE_DBDATA, false)) {
		// 默认不进行导入。需要手动创建资源。
		HashMap<String, Ref<DragonBonesData>> datas;
		_reimport_dbdata_recursively(EditorInterface::get_singleton()->get_resource_filesystem()->get_filesystem(), datas);

		if (!datas.is_empty()) {
			for (auto &kv : datas) {
				const String path = kv.key;
				const Ref<DragonBonesData> data = kv.value;

				Error err = ResourceSaver::get_singleton()->save(data, path, ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS | ResourceSaver::FLAG_CHANGE_PATH);

				if (err != OK) {
					ERR_PRINT(vformat("Save DragonBones data failed: %s", UtilityFunctions::error_string(err)));
				}
			}
		}
	}

	for (DragonBonesData *data : DragonBonesData::get_instants()) {
		data->try_refresh_data();
	}
	callable_mp(this, &DragonBonesEditorPlugin::clear_reimporting_flag).call_deferred();
}

void DragonBonesEditorPlugin::clear_reimporting_flag() { reimporting = false; }

// ---------------------------------------------------------------------------
// 2D 画布叠加层：为选中的 DragonBonesArmatureView 绘制贴合实际显示内容的选中范围。
// 刷新全部事件驱动（selection_changed / 节点 draw 信号 -> update_overlays()），
// 无选中时零开销；不使用 _forward_canvas_force_draw_over_viewport（那会无条件逐帧重绘）。
// ---------------------------------------------------------------------------

void DragonBonesEditorPlugin::_disconnect_selected_views() {
	for (const KeyValue<ObjectID, Callable> &kv : selected_views) {
		if (Object *object = ObjectDB::get_instance(kv.key)) {
			object->disconnect("draw", kv.value);
		}
	}
	selected_views.clear();
}

void DragonBonesEditorPlugin::_on_selection_changed() {
	_disconnect_selected_views();

	const TypedArray<Node> selected_nodes = EditorInterface::get_singleton()->get_selection()->get_selected_nodes();
	for (int i = 0; i < selected_nodes.size(); ++i) {
		DragonBonesArmatureView *view = Object::cast_to<DragonBonesArmatureView>(selected_nodes[i]);
		if (view == nullptr) {
			continue;
		}

		const ObjectID id = ObjectID(view->get_instance_id());
		// 节点重绘（姿态推进/数据重建等都会触发）时刷新叠加层。
		const Callable callable = callable_mp(this, &DragonBonesEditorPlugin::_on_selected_view_redraw).bind(uint64_t(id));
		view->connect("draw", callable);
		selected_views.insert(id, callable);
	}

	update_overlays();
}

void DragonBonesEditorPlugin::_on_selected_view_redraw(uint64_t p_view_id) {
	// 仅当该节点仍在监听集合内（未改选/未释放）时才需要刷新叠加层。
	if (selected_views.has(ObjectID(p_view_id))) {
		update_overlays();
	}
}

void DragonBonesEditorPlugin::_forward_canvas_draw_over_viewport(Control *p_viewport_control) {
	if (selected_views.is_empty()) {
		return;
	}

	// 编辑器 2D 选框色：4.3~4.6 为引擎内硬编码，4.7+ 改为 EditorSettings
	// editors/2d/selection_rectangle_color（默认值同色）。EditorSettings 未纳入绑定裁剪清单
	//（build_profile.json），故取不到用户自定义值，统一用该默认色近似。
	const Color selection_color(1.0, 0.6, 0.4, 0.7);
	// 与引擎一致按编辑器缩放取整，保证高 DPI 下仍是清晰细线（100% 时恰好 1px）。
	const float line_width = Math::round(EditorInterface::get_singleton()->get_editor_scale());

	for (const KeyValue<ObjectID, Callable> &kv : selected_views) {
		DragonBonesArmatureView *view = Object::cast_to<DragonBonesArmatureView>(ObjectDB::get_instance(kv.key));
		if (view == nullptr || !view->is_inside_tree()) {
			continue; // 节点已释放或暂时不在场景树中，跳过
		}

		Rect2 bounds = view->get_editor_selection_rect();
		if (bounds.size.is_zero_approx()) {
			continue; // 无数据/加载失败/无显示内容：不绘制也不报错
		}

		// 节点本地空间 -> 叠加层（编辑器 2D 视口控件本地）空间的变换，与引擎绘制选中框的公式一致：
		// 4.7+ canvas_item_editor_plugin.cpp 为 `transform * ci->get_screen_transform()`
		//（4.3 为 `transform * ci->get_global_transform_with_canvas()`，在编辑器内两者相等）；
		// 其中编辑器画布变换 transform 正是 _draw_viewport() 开头 set_global_canvas_transform()
		// 写入场景视口的那个，可经 Viewport::get_global_canvas_transform() 读回。而编辑器场景根是
		// embedding_subwindows 的 SubViewport（editor_node.cpp），get_popup_base_transform() 恒为
		// identity，故 get_screen_transform() 即 get_global_transform_with_canvas()，
		// 本乘积与各版本引擎公式等价。
		const Transform2D overlay_transform = view->get_viewport()->get_global_canvas_transform() * view->get_screen_transform();

		// 显示内容包围盒：在叠加层上以四角点的闭合折线绘制，带旋转/缩放的变换下不会失真。
		const Vector2 endpoints[4] = {
			overlay_transform.xform(bounds.position),
			overlay_transform.xform(bounds.position + Vector2(bounds.size.x, 0)),
			overlay_transform.xform(bounds.position + bounds.size),
			overlay_transform.xform(bounds.position + Vector2(0, bounds.size.y)),
		};
		PackedVector2Array rect_points;
		rect_points.resize(5);
		for (int i = 0; i < 5; ++i) {
			rect_points[i] = endpoints[i % 4];
		}
		p_viewport_control->draw_polyline(rect_points, selection_color, line_width);
	}
}

bool DragonBonesEditorPlugin::_handles(Object *p_object) const {
	// 本类节点接管；多选时编辑器推送 MultiNodeEdit（该类未暴露给 GDExtension，按类名识别），
	// 使多选场景下叠加层仍被投递——实际绘制集合仍以 EditorSelection 为准。
	if (Object::cast_to<DragonBonesArmatureView>(p_object) != nullptr) {
		return true;
	}
	static const String multi_node_edit_class = String("MultiNodeEdit");
	return p_object != nullptr && p_object->get_class() == multi_node_edit_class;
}

void DragonBonesEditorPlugin::_edit(Object *p_object) {
	// 无需持有被编辑对象：绘制集合以 EditorSelection 为准，此处仅为进入 over 列表的凭证。
	(void)p_object;
}

void DragonBonesEditorPlugin::_input(const Ref<InputEvent> &p_event) {
	// 拖动中的鼠标运动：整体平移选中实例（世界位移按画布缩放换算）。
	const Ref<InputEventMouseMotion> mm = p_event;
	if (!mm.is_null() && drag_active) {
		Node *scene = EditorInterface::get_singleton()->get_edited_scene_root();
		Viewport *scene_vp = scene != nullptr ? scene->get_viewport() : nullptr;
		if (scene_vp == nullptr) {
			return;
		}
		const Vector2 delta_world = scene_vp->get_global_canvas_transform().affine_inverse().basis_xform(mm->get_position() - drag_start_mouse);
		for (int i = 0; i < drag_nodes.size(); ++i) {
			Node2D *node = drag_nodes[i];
			if (node != nullptr && node->is_inside_tree()) {
				node->set_global_position(drag_start_global[i] + delta_world);
			}
		}
		get_viewport()->set_input_as_handled();
		return;
	}

	const Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_null() || mb->get_button_index() != MOUSE_BUTTON_LEFT) {
		return;
	}
	if (!mb->is_pressed()) {
		// 释放：结束拖动并整段提交撤销；未拖动时也吞掉配对释放，避免画布收到「无按下的释放」。
		if (drag_active) {
			_drag_commit();
			drag_active = false;
			drag_nodes.clear();
			drag_start_global.clear();
			drag_start_local.clear();
			get_viewport()->set_input_as_handled();
			body_click_consumed = false;
		} else if (body_click_consumed) {
			get_viewport()->set_input_as_handled();
			body_click_consumed = false;
		}
		return;
	}
	if (drag_active) {
		return; // 不可达：拖动中的重复按下忽略。
	}
	if (mb->is_alt_pressed() || mb->is_shift_pressed() || mb->is_ctrl_pressed() || mb->is_meta_pressed()) {
		return; // 修饰键点击保留引擎原生语义（加选/减选/框选）。
	}

	Node *scene = EditorInterface::get_singleton()->get_edited_scene_root();
	if (scene == nullptr || !scene->is_inside_tree()) {
		return;
	}
	Viewport *scene_vp = scene->get_viewport();
	const Control *canvas_host = Object::cast_to<Control>(scene_vp->get_parent());
	// 画布宿主不可见（如正在 3D 屏）时不接管。
	if (canvas_host == nullptr || !canvas_host->is_visible_in_tree()) {
		return;
	}
	const Rect2 canvas_rect = canvas_host->get_global_rect();
	if (!canvas_rect.has_point(mb->get_position())) {
		return; // 点击不在 2D 画布区域（其他 dock / 工具栏等），交给编辑器处理。
	}

	const Vector2 world_pos = scene_vp->get_global_canvas_transform().affine_inverse().xform(mb->get_position() - canvas_rect.position);

	// 命中测试：点击落在某实例「当前实际显示内容」包围盒内即选中该实例。
	DragonBonesArmatureView *hit = nullptr;
	const TypedArray<Node> views = scene->find_children("*", "DragonBonesArmatureView", true, false);
	for (int i = 0; i < views.size(); ++i) {
		DragonBonesArmatureView *view = Object::cast_to<DragonBonesArmatureView>(views[i]);
		if (view == nullptr) {
			continue;
		}
		Rect2 bounds = view->get_editor_selection_rect();
		if (bounds.size.is_zero_approx()) {
			continue; // 无数据/加载失败：不命中。
		}
		if (bounds.has_point(view->get_global_transform().affine_inverse().xform(world_pos))) {
			hit = view;
			break;
		}
	}
	if (hit == nullptr) {
		return;
	}

	EditorSelection *selection = EditorInterface::get_singleton()->get_selection();
	const TypedArray<Node> selected_nodes_now = selection->get_selected_nodes();
	bool already_selected = false;
	for (int i = 0; i < selected_nodes_now.size(); ++i) {
		if (Object::cast_to<Node>(selected_nodes_now[i]) == hit) {
			already_selected = true;
			break;
		}
	}
	if (!already_selected) {
		selection->clear();
		selection->add_node(hit);
	}

	// Sprite2D 式拖动：选中后即可整体拖动全部选中的本类实例（引擎语义）。
	// 拖动集合必须基于「本次按下后的选区」构造：
	// 命中实例此前未选中时，本次按下等价于单选它；已选中时整组拖动。
	drag_active = true;
	drag_start_mouse = mb->get_position();
	drag_nodes.clear();
	drag_start_global.clear();
	drag_start_local.clear();
	if (already_selected) {
		for (int i = 0; i < selected_nodes_now.size(); ++i) {
			DragonBonesArmatureView *view = Object::cast_to<DragonBonesArmatureView>(selected_nodes_now[i]);
			if (view == nullptr) {
				continue;
			}
			drag_nodes.push_back(view);
			drag_start_global.push_back(view->get_global_position());
			drag_start_local.push_back(view->get_position());
		}
	} else {
		drag_nodes.push_back(hit);
		drag_start_global.push_back(hit->get_global_position());
		drag_start_local.push_back(hit->get_position());
	}

	// 无论该节点此前是否已选中，都吞掉本次按下：引擎的原点命中测不到它，
	// 不吞掉会被画布当作空白处点击（反选/误开框选）。
	body_click_consumed = true;
	get_viewport()->set_input_as_handled();
}

void DragonBonesEditorPlugin::_drag_commit() {
	bool moved = false;
	for (int i = 0; i < drag_nodes.size(); ++i) {
		if (drag_nodes[i] != nullptr && drag_nodes[i]->is_inside_tree() && drag_nodes[i]->get_global_position() != drag_start_global[i]) {
			moved = true;
			break;
		}
	}
	if (!moved) {
		return; // 原地点击：无位移，不产生历史记录。
	}
	EditorUndoRedoManager *undo_redo = get_undo_redo();
	if (undo_redo == nullptr) {
		return;
	}
	undo_redo->create_action(String("移动 DragonBones 节点"));
	for (int i = 0; i < drag_nodes.size(); ++i) {
		Node2D *node = drag_nodes[i];
		if (node == nullptr || !node->is_inside_tree()) {
			continue;
		}
		undo_redo->add_do_property(node, "position", node->get_position());
		undo_redo->add_undo_property(node, "position", drag_start_local[i]);
	}
	undo_redo->commit_action();
}

void DragonBonesEditorPlugin::_enter_tree() {
	import_plugin.instantiate();
	add_import_plugin(import_plugin);

	export_plugin.instantiate();
	add_export_plugin(export_plugin);

	EditorInterface::get_singleton()->get_resource_filesystem()->connect("filesystem_changed", callable_mp(this, &DragonBonesEditorPlugin::_on_filesystem_changed));

	// 选中集合变化时重建 ArmatureView 监听并刷新叠加层。
	EditorInterface::get_singleton()->get_selection()->connect("selection_changed", callable_mp(this, &DragonBonesEditorPlugin::_on_selection_changed));

	if (!ProjectSettings::get_singleton()->has_setting(SETTING_AUTO_GENERATE_DBDATA)) {
		ProjectSettings::get_singleton()->set_setting(SETTING_AUTO_GENERATE_DBDATA, false);
		ProjectSettings::get_singleton()->set_initial_value(SETTING_AUTO_GENERATE_DBDATA, false);
		ProjectSettings::get_singleton()->set_as_basic(SETTING_AUTO_GENERATE_DBDATA, true);
	}
}

void DragonBonesEditorPlugin::_exit_tree() {
	remove_import_plugin(import_plugin);
	import_plugin.unref();

	remove_export_plugin(export_plugin);
	export_plugin.unref();

	EditorInterface::get_singleton()->get_resource_filesystem()->disconnect("filesystem_changed", callable_mp(this, &DragonBonesEditorPlugin::_on_filesystem_changed));

	// 注销/禁用插件时清空全部叠加层监听，保证无残留。
	_disconnect_selected_views();
	EditorInterface::get_singleton()->get_selection()->disconnect("selection_changed", callable_mp(this, &DragonBonesEditorPlugin::_on_selection_changed));
}