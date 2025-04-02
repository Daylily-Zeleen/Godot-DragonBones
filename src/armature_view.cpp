#include "armature_view.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/main_loop.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/templates/vmap.hpp>
#include <godot_cpp/variant/array.hpp>

#include "armature.h"
#include "dragon_bones.h"
#include "event_object.h"

using namespace godot;

void DragonBonesArmatureView::rebuild_armature() {
	if (armature) {
		armature->release(); // 已经处理内存的释放
		armature = nullptr;
	}

	if (factory.is_valid()) {
		armature = factory->create_armature(this, instantiate_dragon_bones_data_name, instantiate_armature_name, instantiate_skin_name);
		if (is_armature_valid()) {
			armature->force_update();
		}
	}

	if (is_inside_tree()) {
		queue_redraw();
	}
}

void DragonBonesArmatureView::set_factory(const Ref<DragonBonesFactory> &p_factory) {
	using namespace dragonBones;
	if (factory == p_factory) {
		return;
	}

	factory = p_factory;

	rebuild_armature();
	notify_property_list_changed();
}

Ref<DragonBonesFactory> DragonBonesArmatureView::get_factory() const {
	return factory;
}

void DragonBonesArmatureView::set_active(bool p_active) {
	active = p_active;
	set_physics_process_internal(callback_mode_process == ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS && active);
	set_process_internal(callback_mode_process == ANIMATION_CALLBACK_MODE_PROCESS_IDLE && active);
}

bool DragonBonesArmatureView::is_active() const {
	return active;
}

void DragonBonesArmatureView::set_debug(bool p_debug) {
	debug = p_debug;

#ifdef DEBUG_ENABLED
	if (debug) {
		debug_mesh = RenderingServer::get_singleton()->mesh_create();
	}
	queue_redraw();
#endif // DEBUG_ENABLED
}

bool DragonBonesArmatureView::is_debug() const {
	return debug;
}

void DragonBonesArmatureView::set_time_scale(float p_time_scale) {
	time_scale = p_time_scale < 0.0 ? 0.0 : p_time_scale;
}

float DragonBonesArmatureView::get_time_scale() const {
	return time_scale;
}

void DragonBonesArmatureView::set_instantiate_dragon_bones_data_name(String p_name) {
	if (p_name == "") {
		p_name = "";
	}
	if (p_name == instantiate_dragon_bones_data_name) {
		return;
	}

	instantiate_dragon_bones_data_name = p_name;
	rebuild_armature();
}

String DragonBonesArmatureView::get_instantiate_dragon_bones_data_name() const {
	return instantiate_dragon_bones_data_name;
}

void DragonBonesArmatureView::set_instantiate_armature_name(String p_name) {
	if (p_name == "") {
		p_name = "";
	}
	if (p_name == instantiate_armature_name) {
		return;
	}

	instantiate_armature_name = p_name;
	rebuild_armature();
}

String DragonBonesArmatureView::get_instantiate_armature_name() const {
	return instantiate_armature_name;
}

void DragonBonesArmatureView::set_instantiate_skin_name(String p_name) {
	if (p_name == "") {
		p_name = "";
	}
	if (p_name == instantiate_skin_name) {
		return;
	}

	instantiate_skin_name = p_name;
	rebuild_armature();
}

String DragonBonesArmatureView::get_instantiate_skin_name() const {
	return instantiate_skin_name;
}

void DragonBonesArmatureView::set_callback_mode_process(AnimationCallbackModeProcess p_mode) {
	callback_mode_process = p_mode;
	set_active(active);
}

DragonBonesArmatureView::AnimationCallbackModeProcess DragonBonesArmatureView::get_callback_mode_process() const {
	return callback_mode_process;
}

int DragonBonesArmatureView::get_animation_loop_count() const {
	return animation_loop_count;
}

void DragonBonesArmatureView::set_animation_loop_count(int p_animation_loop) {
	animation_loop_count = p_animation_loop;
	if (is_armature_valid()) {
		reset();
		armature->play(armature->get_current_animation(), animation_loop_count);
	}
}

void DragonBonesArmatureView::reset() {
	ERR_FAIL_COND(!is_armature_valid());
	armature->reset(true);
}

DragonBonesArmature *DragonBonesArmatureView::get_armature_display() {
	ERR_FAIL_COND_V(!is_armature_valid(), nullptr);
	return armature;
}

void DragonBonesArmatureView::set_armature_settings(const Dictionary &p_settings) const {
	if (is_armature_valid()) {
		armature->set_settings(p_settings);
	} else {
#ifdef TOOLS_ENABLED
		if (!factory->is_imported()) {
			// 只对非导入工厂打印错误信息，导入工厂将在后续重新导入
			WARN_PRINT_ED("armature is invalid, can't set armature settings.");
		}
#else // !TOOLS_ENABLED
		WARN_PRINT_ED("armature is invalid, can't set armature settings.");
#endif // !TOOLS_ENABLED
	}
}

Dictionary DragonBonesArmatureView::get_armature_settings() const {
	if (!is_armature_valid()) {
		return {};
	}
#ifdef TOOLS_ENABLED
	return armature->get_settings();
#else //TOOLS_ENABLED
	ERR_FAIL_V_MSG({}, "DragonBonesArmatureView::get_armature_settings() can be call in editor build only.");
#endif // TOOLS_ENABLED
}

bool DragonBonesArmatureView::_set(const StringName &p_name, const Variant &p_property) {
	if (p_name == SNAME("armature_settings")) {
		set_armature_settings(p_property);
		return true;
	}
#ifdef TOOLS_ENABLED
	else if (p_name == SNAME("armature")) {
		return true; // 禁止设置
	}
#endif //  TOOLS_ENABLED

	return false;
}

bool DragonBonesArmatureView::_get(const StringName &p_name, Variant &r_property) const {
	if (p_name == SNAME("armature_settings")) {
		r_property = get_armature_settings();
		return true;
	}
#ifdef TOOLS_ENABLED
	else if (p_name == SNAME("armature")) {
		// Avoid instantiation when getting default value.
		if (is_armature_valid() && armature_ref.is_null()) {
			armature_ref.instantiate();
		}

		if (armature_ref.is_valid()) {
			armature_ref->armature = armature;
		}

		r_property = armature_ref;
		return true;
	}
#endif // TOOLS_ENABLED
	return false;
}

void DragonBonesArmatureView::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::DICTIONARY, "armature_settings", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));

#ifdef TOOLS_ENABLED
	if (is_armature_valid() && Engine::get_singleton()->is_editor_hint()) {
		p_list->push_back(PropertyInfo(
				Variant::OBJECT, "armature", PROPERTY_HINT_RESOURCE_TYPE, DragonBonesArmatureProxy::get_class_static(), PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT, DragonBonesArmatureProxy::get_class_static()));
	}
#endif // TOOLS_ENABLED
}

#ifdef TOOLS_ENABLED
void DragonBonesArmatureView::_validate_property(PropertyInfo &p_property) const {
	if (!Engine::get_singleton()->is_editor_hint() || factory.is_null()) {
		return;
	}
	if (p_property.name == SNAME("instantiate_dragon_bones_data_name")) {
		auto dragon_bones_data_list = factory->get_loaded_dragon_bones_data_name_list();
		p_property.hint_string = String(",").join(dragon_bones_data_list);
	} else if (p_property.name == SNAME("instantiate_armature_name")) {
		auto armatures = factory->get_loaded_dragon_bones_armature_name_list(instantiate_dragon_bones_data_name);
		p_property.hint_string = String(",").join(armatures);
	} else if (p_property.name == SNAME("instantiate_skin_name")) {
		auto skins = factory->get_loaded_dragon_bones_skin_name_list(instantiate_dragon_bones_data_name, instantiate_armature_name);
		p_property.hint_string = String(",").join(skins);
	}
}
#endif // TOOLS_ENABLED

void DragonBonesArmatureView::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			set_active(active);
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (active && callback_mode_process == ANIMATION_CALLBACK_MODE_PROCESS_IDLE) {
				advance(get_process_delta_time() * time_scale);
			}
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (active && callback_mode_process == ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS) {
				advance(get_physics_process_delta_time() * time_scale);
			}
		} break;
	}
}

void DragonBonesArmatureView::_draw() {
	if (!is_armature_valid()) {
		return;
	}

	// Collect draw data.
	VMap<int, LocalVector<DrawData>> draw_data;
	armature->append_draw_data(draw_data);

	if (draw_data.is_empty()) {
		return;
	}

	const auto RS = RenderingServer::get_singleton();
	const auto pairs = draw_data.get_array();

	struct SurfaceData {
		PackedInt32Array indices;
		PackedVector2Array vertices;
		PackedColorArray colors;
		PackedVector2Array vertices_uv;
		RID texture;
		CanvasItemMaterial::BlendMode blend_mode;

		SurfaceData() :
				blend_mode(CanvasItemMaterial::BLEND_MODE_MIX) {}
		SurfaceData(RID p_texture, CanvasItemMaterial::BlendMode p_blend_mode) :
				texture(p_texture), blend_mode(p_blend_mode) {}
	};

	// Prepare mesh data.
	const auto &first_draw_data = pairs[0].value[0];
	using Surfaces = LocalVector<SurfaceData>;
	LocalVector<Surfaces> meshes{ { { first_draw_data.texture, first_draw_data.blend_mode } } };
	for (auto i = 0; i < draw_data.size(); ++i) {
		for (const auto &draw_data : pairs[i].value) {
			Surfaces &surfaces = meshes[meshes.size() - 1];
			auto &surface_data = surfaces[surfaces.size() - 1];

			if (surface_data.texture != draw_data.texture) {
				surface_data = SurfaceData(draw_data.texture, draw_data.blend_mode);
				surfaces = Surfaces{ std::move(surface_data) };
				meshes.push_back(std::move(surfaces));
			} else if (surface_data.blend_mode != draw_data.blend_mode) {
				surface_data = SurfaceData(draw_data.texture, draw_data.blend_mode);
				surfaces.push_back(std::move(surface_data));
			}

			int base_index = surface_data.vertices.size();
			int insert_begin_index = surface_data.indices.size();

			surface_data.indices.resize(surface_data.indices.size() + draw_data.indices.size());
			for (int idx = 0; idx < draw_data.indices.size(); ++idx) {
				surface_data.indices[idx + insert_begin_index] = draw_data.indices[idx] + base_index;
			}

			surface_data.vertices.append_array(draw_data.transform.xform(draw_data.vertices));
			surface_data.colors.append_array(draw_data.colors);
			surface_data.vertices_uv.append_array(draw_data.vertices_uv);
		}
	}

	// Clear surfaces.
	for (RID mesh : draw_meshes) {
		RS->mesh_clear(mesh);
	}

	// Add rendering commands.
	for (int mesh_idx = 0; mesh_idx < meshes.size(); ++mesh_idx) {
		const RID mesh = get_draw_mesh(mesh_idx);
		const Surfaces &surfaces = meshes[mesh_idx];
		for (int surface_idx = 0; surface_idx < surfaces.size(); ++surface_idx) {
			const SurfaceData &surface_data = surfaces[surface_idx];
			Array arr;
			arr.resize(RenderingServer::ARRAY_MAX);
			arr[RenderingServer::ARRAY_INDEX] = surface_data.indices;
			arr[RenderingServer::ARRAY_VERTEX] = surface_data.vertices;
			arr[RenderingServer::ARRAY_COLOR] = surface_data.colors;
			arr[RenderingServer::ARRAY_TEX_UV] = surface_data.vertices_uv;

			RS->mesh_add_surface_from_arrays(mesh, RenderingServer::PRIMITIVE_TRIANGLES, arr);
			auto mat = get_blend_material(surface_data.blend_mode);
			RS->mesh_surface_set_material(mesh, surface_idx, mat);
		}

		RS->canvas_item_add_mesh(get_canvas_item(), mesh, get_canvas_transform(), get_modulate(), surfaces[0].texture);
	}

#ifdef DEBUG_ENABLED
	if (debug) {
		// Prepare debug mesh data.
		PackedInt32Array debug_mesh_indices;
		PackedVector2Array debug_vertices;
		PackedColorArray debug_colors;

		for (auto i = 0; i < draw_data.size(); ++i) {
			for (const auto &draw_data : pairs[i].value) {
				int base_index = debug_vertices.size();
				int insert_begin_index = debug_mesh_indices.size();

				debug_mesh_indices.resize(debug_mesh_indices.size() + draw_data.indices.size());
				for (int idx = 0; idx < draw_data.indices.size(); ++idx) {
					debug_mesh_indices[idx + insert_begin_index] = draw_data.indices[idx] + base_index;
				}

				debug_vertices.append_array(draw_data.transform.xform(draw_data.vertices));

				PackedColorArray colors;
				colors.resize(draw_data.vertices.size());
				colors.fill(draw_data.debug_color);
				debug_colors.append_array(colors);
			}
		}

		// Triangles to lines.
		PackedInt32Array debug_lines_indices;
		debug_lines_indices.resize(debug_mesh_indices.size() * 2);
		for (int i = 0; i < debug_mesh_indices.size(); i += 3) {
			int base_index = 2 * i;
			debug_lines_indices[base_index] = debug_mesh_indices[i];
			debug_lines_indices[base_index + 1] = debug_mesh_indices[i + 1];

			debug_lines_indices[base_index + 2] = debug_mesh_indices[i + 1];
			debug_lines_indices[base_index + 3] = debug_mesh_indices[i + 2];

			debug_lines_indices[base_index + 4] = debug_mesh_indices[i + 2];
			debug_lines_indices[base_index + 5] = debug_mesh_indices[i];
		}

		// Add debug rendering commands.
		RS->mesh_clear(debug_mesh);
		Array arr;
		arr.resize(RenderingServer::ARRAY_MAX);
		arr[RenderingServer::ARRAY_INDEX] = debug_lines_indices;
		arr[RenderingServer::ARRAY_VERTEX] = debug_vertices;
		arr[RenderingServer::ARRAY_COLOR] = debug_colors;
		RS->mesh_add_surface_from_arrays(debug_mesh, RenderingServer::PRIMITIVE_LINES, arr);
		RS->canvas_item_add_mesh(get_canvas_item(), debug_mesh, get_canvas_transform(), get_modulate());
	}
#endif // DEBUG_ENABLED
}

void DragonBonesArmatureView::dispatch_event(const Ref<DragonBonesEventObject> &p_event_object) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	emit_signal(SNAME("event_dispatched"), p_event_object);
}

RID DragonBonesArmatureView::get_draw_mesh(int p_index) {
	if (p_index < draw_meshes.size()) {
		return draw_meshes[p_index];
	} else {
		draw_meshes.push_back(RenderingServer::get_singleton()->mesh_create());
		return draw_meshes[draw_meshes.size() - 1];
	}
}

void DragonBonesArmatureView::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_factory", "factory"), &DragonBonesArmatureView::set_factory);
	ClassDB::bind_method(D_METHOD("get_factory"), &DragonBonesArmatureView::get_factory);

	ClassDB::bind_method(D_METHOD("advance", "delta"), &DragonBonesArmatureView::advance);

	ClassDB::bind_method(D_METHOD("set_animation_loop_count", "loop_count"), &DragonBonesArmatureView::set_animation_loop_count);
	ClassDB::bind_method(D_METHOD("get_animation_loop_count"), &DragonBonesArmatureView::get_animation_loop_count);

	ClassDB::bind_method(D_METHOD("set_time_scale", "speed_scale"), &DragonBonesArmatureView::set_time_scale);
	ClassDB::bind_method(D_METHOD("get_time_scale"), &DragonBonesArmatureView::get_time_scale);

	ClassDB::bind_method(D_METHOD("get_armature_display"), &DragonBonesArmatureView::get_armature_display);

	ClassDB::bind_method(D_METHOD("set_active", "active"), &DragonBonesArmatureView::set_active);
	ClassDB::bind_method(D_METHOD("is_active"), &DragonBonesArmatureView::is_active);

	ClassDB::bind_method(D_METHOD("set_debug", "debug"), &DragonBonesArmatureView::set_debug);
	ClassDB::bind_method(D_METHOD("is_debug"), &DragonBonesArmatureView::is_debug);

	ClassDB::bind_method(D_METHOD("set_callback_mode_process", "mode"), &DragonBonesArmatureView::set_callback_mode_process);
	ClassDB::bind_method(D_METHOD("get_callback_mode_process"), &DragonBonesArmatureView::get_callback_mode_process);

	ClassDB::bind_method(D_METHOD("set_instantiate_dragon_bones_data_name", "instantiate_dragon_bones_data_name"), &DragonBonesArmatureView::set_instantiate_dragon_bones_data_name);
	ClassDB::bind_method(D_METHOD("get_instantiate_dragon_bones_data_name"), &DragonBonesArmatureView::get_instantiate_dragon_bones_data_name);

	ClassDB::bind_method(D_METHOD("set_instantiate_armature_name", "instantiate_armature_name"), &DragonBonesArmatureView::set_instantiate_armature_name);
	ClassDB::bind_method(D_METHOD("get_instantiate_armature_name"), &DragonBonesArmatureView::get_instantiate_armature_name);

	ClassDB::bind_method(D_METHOD("set_instantiate_skin_name", "instantiate_skin_name"), &DragonBonesArmatureView::set_instantiate_skin_name);
	ClassDB::bind_method(D_METHOD("get_instantiate_skin_name"), &DragonBonesArmatureView::get_instantiate_skin_name);

	// 包装 ===========
	ClassDB::bind_method(D_METHOD("has_animation", "animation_name"), &DragonBonesArmatureView::has_animation);
	ClassDB::bind_method(D_METHOD("get_animations"), &DragonBonesArmatureView::get_animations);
	ClassDB::bind_method(D_METHOD("is_playing"), &DragonBonesArmatureView::is_playing);

	ClassDB::bind_method(D_METHOD("tell_animation", "animation_name"), &DragonBonesArmatureView::tell_animation);
	ClassDB::bind_method(D_METHOD("seek_animation", "animation_name", "progress"), &DragonBonesArmatureView::seek_animation);

	ClassDB::bind_method(D_METHOD("play", "animation_name", "loop_count"), &DragonBonesArmatureView::play, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("play_from_time", "animation_name", "time", "loop_count"), &DragonBonesArmatureView::play_from_time, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("play_from_progress", "animation_name", "progress", "loop_count"), &DragonBonesArmatureView::play_from_progress, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("stop", "animation_name", "reset", "recursively"), &DragonBonesArmatureView::stop, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("stop_all_animations", "reset", "recursively"), &DragonBonesArmatureView::stop_all_animations, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("fade_in", "animation_name", "time", "loop", "layer", "group", "fade_out_mode"), &DragonBonesArmatureView::fade_in);

	ClassDB::bind_method(D_METHOD("has_slot", "slot_name"), &DragonBonesArmatureView::has_slot);
	ClassDB::bind_method(D_METHOD("get_slot", "slot_name"), &DragonBonesArmatureView::get_slot);
	ClassDB::bind_method(D_METHOD("get_slots"), &DragonBonesArmatureView::get_slots);

	ClassDB::bind_method(D_METHOD("get_ik_constraints"), &DragonBonesArmatureView::get_ik_constraints);
	ClassDB::bind_method(D_METHOD("set_ik_constraint", "constraint_name", "new_position"), &DragonBonesArmatureView::set_ik_constraint);
	ClassDB::bind_method(D_METHOD("set_ik_constraint_bend_positive", "constraint_name", "bend_positive"), &DragonBonesArmatureView::set_ik_constraint_bend_positive);

	ClassDB::bind_method(D_METHOD("get_bones"), &DragonBonesArmatureView::get_bones);
	ClassDB::bind_method(D_METHOD("get_bone", "bone_name"), &DragonBonesArmatureView::get_bone);

	ClassDB::bind_method(D_METHOD("get_rect"), &DragonBonesArmatureView::get_rect);
	ClassDB::bind_method(D_METHOD("get_global_rect"), &DragonBonesArmatureView::get_global_rect);

	// Setter Getter
	ClassDB::bind_method(D_METHOD("set_current_animation", "current_animation"), &DragonBonesArmatureView::set_current_animation);
	ClassDB::bind_method(D_METHOD("get_current_animation"), &DragonBonesArmatureView::get_current_animation);

	ClassDB::bind_method(D_METHOD("set_animation_progress", "progress"), &DragonBonesArmatureView::set_animation_progress);
	ClassDB::bind_method(D_METHOD("get_animation_progress"), &DragonBonesArmatureView::get_animation_progress);

	ClassDB::bind_method(D_METHOD("set_flip_x_", "flip_x"), &DragonBonesArmatureView::set_flip_x_);
	ClassDB::bind_method(D_METHOD("is_flipped_x"), &DragonBonesArmatureView::is_flipped_x);

	ClassDB::bind_method(D_METHOD("set_flip_y_", "flip_y"), &DragonBonesArmatureView::set_flip_y_);
	ClassDB::bind_method(D_METHOD("is_flipped_y"), &DragonBonesArmatureView::is_flipped_y);

	ClassDB::bind_method(D_METHOD("set_texture_override", "texture_override"), &DragonBonesArmatureView::set_texture_override);
	ClassDB::bind_method(D_METHOD("get_texture_override"), &DragonBonesArmatureView::get_texture_override);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "current_animation", PROPERTY_HINT_ENUM, "", PROPERTY_USAGE_NONE), "set_current_animation", "get_current_animation");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "animation_progress", PROPERTY_HINT_RANGE, "0.0,1.0,0.0001", PROPERTY_USAGE_NONE), "set_animation_progress", "get_animation_progress");

	ADD_GROUP("Flip", "flip_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_x", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_flip_x_", "is_flipped_x");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_y", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_flip_y_", "is_flipped_y");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture_override", PROPERTY_HINT_RESOURCE_TYPE, Texture2D::get_class_static(), PROPERTY_USAGE_NONE), "set_texture_override", "get_texture_override");
	// ================================

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "factory", PROPERTY_HINT_RESOURCE_TYPE, DragonBonesFactory::get_class_static()), "set_factory", "get_factory");

	// This is how we set top level properties
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "active"), "set_active", "is_active");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug"), "set_debug", "is_debug");

	ADD_GROUP("Animation Settings", "animation_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "animation_loop_count", PROPERTY_HINT_RANGE, "0,100,1,or_greater"), "set_animation_loop_count", "get_animation_loop_count");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "animation_time_scale", PROPERTY_HINT_RANGE, "0,10,0.01"), "set_time_scale", "get_time_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "animation_callback_mode_process", PROPERTY_HINT_ENUM, "Physics,Idle,Manual"), "set_callback_mode_process", "get_callback_mode_process");

	ADD_GROUP("Instantiate Settings", "instantiate_");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "instantiate_dragon_bones_data_name", PROPERTY_HINT_ENUM_SUGGESTION, ""), "set_instantiate_dragon_bones_data_name", "get_instantiate_dragon_bones_data_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "instantiate_armature_name", PROPERTY_HINT_ENUM_SUGGESTION, ""), "set_instantiate_armature_name", "get_instantiate_armature_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "instantiate_skin_name", PROPERTY_HINT_ENUM_SUGGESTION, ""), "set_instantiate_skin_name", "get_instantiate_skin_name");

	// 信号
	ADD_SIGNAL(MethodInfo("event_dispatched", PropertyInfo(Variant::OBJECT, "event_object", PROPERTY_HINT_NONE, "", PROPERTY_HINT_NONE, DragonBonesEventObject::get_class_static())));

	// 枚举
	BIND_ENUM_CONSTANT(ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS);
	BIND_ENUM_CONSTANT(ANIMATION_CALLBACK_MODE_PROCESS_IDLE);
	BIND_ENUM_CONSTANT(ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);

	// Enum
	BIND_ENUM_CONSTANT(AnimFadeOutMode::FADE_OUT_NONE);
	BIND_ENUM_CONSTANT(AnimFadeOutMode::FADE_OUT_SAME_LAYER);
	BIND_ENUM_CONSTANT(AnimFadeOutMode::FADE_OUT_SAME_GROUP);
	BIND_ENUM_CONSTANT(AnimFadeOutMode::FADE_OUT_SAME_LAYER_AND_GROUP);
	BIND_ENUM_CONSTANT(AnimFadeOutMode::FADE_OUT_ALL);
	BIND_ENUM_CONSTANT(AnimFadeOutMode::FADE_OUT_SINGLE);
}

DragonBonesArmatureView::DragonBonesArmatureView() {
}

DragonBonesArmatureView::~DragonBonesArmatureView() {
	if (armature) {
		armature->release(); // 已经处理内存的释放
		armature = nullptr;
	}

	DragonBones::get_singleton()->flush();

	for (auto mesh : draw_meshes) {
		RenderingServer::get_singleton()->free_rid(mesh);
	}
	draw_meshes.clear();

#ifdef DEBUG_ENABLED
	if (debug_mesh.is_valid()) {
		RenderingServer::get_singleton()->free_rid(debug_mesh);
	}
#endif // DEBUG_ENABLED
}

HashMap<CanvasItemMaterial::BlendMode, Ref<CanvasItemMaterial>> DragonBonesArmatureView::blend_materials;
void DragonBonesArmatureView::clear_static() {
	blend_materials.clear();
}

RID DragonBonesArmatureView::get_blend_material(CanvasItemMaterial::BlendMode p_blend_mode) {
	auto it = blend_materials.find(p_blend_mode);
	if (it == blend_materials.end()) {
		Ref<CanvasItemMaterial> mat;
		mat.instantiate();
		mat->set_blend_mode(p_blend_mode);
		return blend_materials.insert(p_blend_mode, mat)->value->get_rid();
	}
	return it->value->get_rid();
}

// ---------

bool DragonBonesArmatureView::has_animation(const String &p_animation_name) const {
	ERR_FAIL_NULL_V(armature, false);
	return armature->has_animation(p_animation_name);
}
PackedStringArray DragonBonesArmatureView::get_animations() {
	ERR_FAIL_NULL_V(armature, PackedStringArray());
	return armature->get_animations();
}
String DragonBonesArmatureView::get_current_animation_on_layer(int p_layer) const {
	ERR_FAIL_NULL_V(armature, String());
	return armature->get_current_animation_on_layer(p_layer);
}
String DragonBonesArmatureView::get_current_animation_in_group(const String &p_group_name) const {
	ERR_FAIL_NULL_V(armature, String());
	return armature->get_current_animation_in_group(p_group_name);
}
float DragonBonesArmatureView::tell_animation(const String &p_animation_name) const {
	ERR_FAIL_NULL_V(armature, 0.0f);
	return armature->tell_animation(p_animation_name);
}
void DragonBonesArmatureView::seek_animation(const String &p_animation_name, float p_progress) {
	ERR_FAIL_NULL(armature);
	armature->seek_animation(p_animation_name, p_progress);
}
bool DragonBonesArmatureView::is_playing() const {
	ERR_FAIL_NULL_V(armature, false);
	return armature->is_playing();
}
void DragonBonesArmatureView::play(const String &p_animation_name, int p_loop_count) {
	ERR_FAIL_NULL(armature);
	armature->play(p_animation_name, p_loop_count);
}
void DragonBonesArmatureView::play_from_time(const String &p_animation_name, float p_time, int p_loop_count) {
	ERR_FAIL_NULL(armature);
	armature->play_from_time(p_animation_name, p_time, p_loop_count);
}
void DragonBonesArmatureView::play_from_progress(const String &p_animation_name, float p_progress, int p_loop_count) {
	ERR_FAIL_NULL(armature);
	armature->play_from_progress(p_animation_name, p_progress, p_loop_count);
}
void DragonBonesArmatureView::stop(const String &p_animation_name, bool b_reset, bool p_recursively) {
	ERR_FAIL_NULL(armature);
	armature->stop(p_animation_name, b_reset, p_recursively);
}
void DragonBonesArmatureView::stop_all_animations(bool b_reset, bool p_recursively) {
	ERR_FAIL_NULL(armature);
	armature->stop_all_animations(b_reset, p_recursively);
}
void DragonBonesArmatureView::fade_in(const String &p_animation_name, float p_time,
		int p_loop_count, int p_layer, const String &p_group, AnimFadeOutMode p_fade_out_mode) {
	ERR_FAIL_NULL(armature);
	armature->fade_in(p_animation_name, p_time, p_loop_count, p_layer, p_group, p_fade_out_mode);
}

bool DragonBonesArmatureView::has_slot(const String &p_slot_name) const {
	ERR_FAIL_NULL_V(armature, false);
	return armature->has_slot(p_slot_name);
}
Ref<DragonBonesSlot> DragonBonesArmatureView::get_slot(const String &p_slot_name) {
	ERR_FAIL_NULL_V(armature, {});
	return armature->get_slot(p_slot_name);
}
SlotsDictionary DragonBonesArmatureView::get_slots() {
	ERR_FAIL_NULL_V(armature, {});
	return armature->get_slots();
}

ConstraintsDictionary DragonBonesArmatureView::get_ik_constraints() {
	ERR_FAIL_NULL_V(armature, ConstraintsDictionary());
	return armature->get_ik_constraints();
}
void DragonBonesArmatureView::set_ik_constraint(const String &p_name, Vector2 p_position) {
	ERR_FAIL_NULL(armature);
	armature->set_ik_constraint(p_name, p_position);
}
void DragonBonesArmatureView::set_ik_constraint_bend_positive(const String &p_name, bool p_bend_positive) {
	ERR_FAIL_NULL(armature);
	armature->set_ik_constraint_bend_positive(p_name, p_bend_positive);
}

BonesDictionary DragonBonesArmatureView::get_bones() {
	ERR_FAIL_NULL_V(armature, {});
	return armature->get_bones();
}
Ref<DragonBonesBone> DragonBonesArmatureView::get_bone(const String &p_name) {
	ERR_FAIL_NULL_V(armature, {});
	return armature->get_bone(p_name);
}

Rect2 DragonBonesArmatureView::get_rect() const {
	ERR_FAIL_NULL_V(armature, {});
	Rect2 rect = armature->get_rect();
	return get_transform().inverse().xform(rect);
}

Rect2 DragonBonesArmatureView::get_global_rect() const {
	ERR_FAIL_NULL_V(armature, {});
	Rect2 rect = armature->get_rect();
	return get_global_transform().inverse().xform(rect);
}

// setget
void DragonBonesArmatureView::set_current_animation(const String &p_animation) {
	ERR_FAIL_NULL(armature);
	armature->set_current_animation(p_animation);
}
String DragonBonesArmatureView::get_current_animation() const {
	ERR_FAIL_NULL_V(armature, "");
	return armature->get_current_animation();
}

void DragonBonesArmatureView::set_animation_progress(float p_progress) {
	ERR_FAIL_NULL(armature);
	armature->set_animation_progress(p_progress);
}
float DragonBonesArmatureView::get_animation_progress() const {
	ERR_FAIL_NULL_V(armature, 0.0f);
	return armature->get_animation_progress();
}

void DragonBonesArmatureView::set_flip_x(bool p_flip_x, bool p_recursively) {
	ERR_FAIL_NULL(armature);
	armature->set_flip_x(p_flip_x, p_recursively);
}
bool DragonBonesArmatureView::is_flipped_x() const {
	ERR_FAIL_NULL_V(armature, false);
	return armature->is_flipped_x();
}

void DragonBonesArmatureView::set_flip_y(bool p_flip_y, bool p_recursively) {
	ERR_FAIL_NULL(armature);
	armature->set_flip_y(p_flip_y, p_recursively);
}
bool DragonBonesArmatureView::is_flipped_y() const {
	ERR_FAIL_NULL_V(armature, false);
	return armature->is_flipped_y();
}

Ref<Texture2D> DragonBonesArmatureView::get_texture_override() const {
	ERR_FAIL_NULL_V(armature, Ref<Texture2D>());
	return armature->get_texture_override();
}
void DragonBonesArmatureView::set_texture_override(const Ref<Texture2D> &p_texture_override) {
	ERR_FAIL_NULL(armature);
	armature->set_texture_override(p_texture_override);
}