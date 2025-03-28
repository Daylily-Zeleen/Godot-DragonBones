#include "dragonbones.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/templates/vmap.hpp>
#include <godot_cpp/variant/array.hpp>

#include "armature.h"
#include "event_object.h"

using namespace godot;

/////////////////////////////////////////////////////////////////
void DragonBones::dispatchDBEvent(const std::string &p_type, dragonBones::EventObject *p_value) {
	using namespace dragonBones;
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	ERR_FAIL_NULL(p_value);
	emit_signal(SNAME("event_dispatched"), Ref<DragonBonesEventObject>(memnew(DragonBonesEventObject(p_value))));
}

void DragonBones::_set_process(bool p_process, bool p_force) {
	if (processing == p_process && !p_force) {
		return;
	}

	set_physics_process_internal(callback_mode_process == ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS && p_process && active);
	set_process_internal(callback_mode_process == ANIMATION_CALLBACK_MODE_PROCESS_IDLE && p_process && active);

	processing = p_process;
}

void DragonBones::rebuild_armature() {
	if (armature) {
		armature->release(); // 已经处理内存的释放
		armature = nullptr;
	}

	if (!factory.is_valid()) {
		armature = factory->create_armature(this, instantiate_dragon_bones_data_name, instantiate_armature_name, instantiate_skin_name);
		if (is_armature_valid()) {
			armature->force_update();
		}
	}

	if (is_inside_tree()) {
		queue_redraw();
	}
}

void DragonBones::set_factory(const Ref<DragonBonesFactory> &p_factory) {
	using namespace dragonBones;
	if (factory == p_factory) {
		return;
	}

	factory = p_factory;

	if (factory.is_null()) {
		notify_property_list_changed();
		return;
	}

	// build Armature display
	rebuild_armature();
	ERR_FAIL_COND(!is_armature_valid());

	notify_property_list_changed();
}

Ref<DragonBonesFactory> DragonBones::get_factory() const {
	return factory;
}

void DragonBones::set_active(bool p_active) {
	active = p_active;
	_set_process(active, true);
}

bool DragonBones::is_active() const {
	return active;
}

void DragonBones::set_debug(bool p_debug) {
	debug = p_debug;

#ifdef DEBUG_ENABLED
	if (debug) {
		debug_mesh = RenderingServer::get_singleton()->mesh_create();
	}
	queue_redraw();
#endif // DEBUG_ENABLED
}

bool DragonBones::is_debug() const {
	return debug;
}

void DragonBones::set_time_scale(float p_time_scale) {
	time_scale = p_time_scale < 0.0 ? 0.0 : p_time_scale;
}

float DragonBones::get_time_scale() const {
	return time_scale;
}

void DragonBones::set_instantiate_dragon_bones_data_name(String p_name) {
	if (p_name == "") {
		p_name = "";
	}
	if (p_name == instantiate_dragon_bones_data_name) {
		return;
	}

	instantiate_dragon_bones_data_name = p_name;
	rebuild_armature();
}

String DragonBones::get_instantiate_dragon_bones_data_name() const {
	return instantiate_dragon_bones_data_name;
}

void DragonBones::set_instantiate_armature_name(String p_name) {
	if (p_name == "") {
		p_name = "";
	}
	if (p_name == instantiate_armature_name) {
		return;
	}

	instantiate_armature_name = p_name;
	rebuild_armature();
}

String DragonBones::get_instantiate_armature_name() const {
	return instantiate_armature_name;
}

void DragonBones::set_instantiate_skin_name(String p_name) {
	if (p_name == "") {
		p_name = "";
	}
	if (p_name == instantiate_skin_name) {
		return;
	}

	instantiate_skin_name = p_name;
	rebuild_armature();
}

String DragonBones::get_instantiate_skin_name() const {
	return instantiate_skin_name;
}

void DragonBones::set_callback_mode_process(AnimationCallbackModeProcess p_mode) {
	callback_mode_process = p_mode;
}

DragonBones::AnimationCallbackModeProcess DragonBones::get_callback_mode_process() const {
	return callback_mode_process;
}

int DragonBones::get_animation_loop_count() const {
	return animation_loop_count;
}

void DragonBones::set_animation_loop_count(int p_animation_loop) {
	animation_loop_count = p_animation_loop;
	if (is_armature_valid()) {
		reset();
		armature->play(armature->get_current_animation(), animation_loop_count);
	}
}

void DragonBones::reset() {
	ERR_FAIL_COND(!is_armature_valid());
	armature->reset(true);
}

DragonBonesArmature *DragonBones::get_armature() {
	ERR_FAIL_COND_V(!is_armature_valid(), nullptr);
	return armature;
}

void DragonBones::set_armature(DragonBonesArmature *) const {
	ERR_FAIL_MSG("DragonBones's property \"armature\" is readonly.");
}

void DragonBones::set_armature_settings(const Dictionary &p_settings) const {
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

Dictionary DragonBones::get_armature_settings() const {
	if (!is_armature_valid()) {
		return {};
	}
#ifdef TOOLS_ENABLED
	return armature->get_settings();
#else //TOOLS_ENABLED
	ERR_FAIL_V_MSG({}, "DragonBones::get_armature_settings() can be call in editor build only.");
#endif // TOOLS_ENABLED
}

bool DragonBones::_set(const StringName &p_name, const Variant &p_property) {
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

bool DragonBones::_get(const StringName &p_name, Variant &r_property) const {
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

void DragonBones::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::DICTIONARY, "armature_settings", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));

#ifdef TOOLS_ENABLED
	if (is_armature_valid() && Engine::get_singleton()->is_editor_hint()) {
		p_list->push_back(PropertyInfo(
				Variant::OBJECT, "armature", PROPERTY_HINT_RESOURCE_TYPE, DragonBonesArmatureProxy::get_class_static(), PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT, DragonBonesArmatureProxy::get_class_static()));
	}
#endif // TOOLS_ENABLED
}

#ifdef TOOLS_ENABLED
void DragonBones::_validate_property(PropertyInfo &p_property) const {
	if (!Engine::get_singleton()->is_editor_hint() || factory.is_null()) {
		return;
	}
	if (p_property.name == SNAME("instantiate_dragon_bones_data_name")) {
		auto dragonbones_data = factory->get_loaded_dragon_bones_data_name_list();
		p_property.hint_string = String(",").join(dragonbones_data);
	} else if (p_property.name == SNAME("instantiate_armature_name")) {
		auto armatures = factory->get_loaded_dragon_bones_armature_name_list(instantiate_dragon_bones_data_name);
		p_property.hint_string = String(",").join(armatures);
	} else if (p_property.name == SNAME("instantiate_skin_name")) {
		auto skins = factory->get_loaded_dragon_bones_main_skin_name_list(instantiate_dragon_bones_data_name, instantiate_armature_name);
		auto default_idx = 0;
		if (default_idx >= 0) {
			skins.remove_at(default_idx);
		}
		p_property.hint_string = String(",").join(skins);
	}
}
#endif // TOOLS_ENABLED

void DragonBones::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_set_process(active);
			if (!processing) {
				set_physics_process_internal(false);
				set_process_internal(false);
			}
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

void DragonBones::_draw() {
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

RID DragonBones::get_draw_mesh(int p_index) {
	if (p_index < draw_meshes.size()) {
		return draw_meshes[p_index];
	} else {
		draw_meshes.push_back(RenderingServer::get_singleton()->mesh_create());
		return draw_meshes[draw_meshes.size() - 1];
	}
}

void DragonBones::for_each_armature_(const Callable &p_action) {
	for_each_armature(
			[&](auto armature, auto depth) {
				return p_action.call(armature, depth).booleanize();
			});
}

void DragonBones::_bind_methods() {
	ClassDB::bind_method(D_METHOD("for_each_armature", "action"), &DragonBones::for_each_armature_);

	ClassDB::bind_method(D_METHOD("set_factory", "dbfactory"), &DragonBones::set_factory);
	ClassDB::bind_method(D_METHOD("get_factory"), &DragonBones::get_factory);

	ClassDB::bind_method(D_METHOD("advance", "delta"), &DragonBones::advance);

	ClassDB::bind_method(D_METHOD("set_animation_loop_count", "loop_count"), &DragonBones::set_animation_loop_count);
	ClassDB::bind_method(D_METHOD("get_animation_loop_count"), &DragonBones::get_animation_loop_count);

	ClassDB::bind_method(D_METHOD("set_time_scale", "speed_scale"), &DragonBones::set_time_scale);
	ClassDB::bind_method(D_METHOD("get_time_scale"), &DragonBones::get_time_scale);

	ClassDB::bind_method(D_METHOD("get_armature"), &DragonBones::get_armature);
	ClassDB::bind_method(D_METHOD("set_armature_readonly"), &DragonBones::set_armature);

	ClassDB::bind_method(D_METHOD("set_active", "active"), &DragonBones::set_active);
	ClassDB::bind_method(D_METHOD("is_active"), &DragonBones::is_active);

	ClassDB::bind_method(D_METHOD("set_debug", "debug"), &DragonBones::set_debug);
	ClassDB::bind_method(D_METHOD("is_debug"), &DragonBones::is_debug);

	ClassDB::bind_method(D_METHOD("set_callback_mode_process", "mode"), &DragonBones::set_callback_mode_process);
	ClassDB::bind_method(D_METHOD("get_callback_mode_process"), &DragonBones::get_callback_mode_process);

	ClassDB::bind_method(D_METHOD("set_instantiate_dragon_bones_data_name", "instantiate_dragon_bones_data_name"), &DragonBones::set_instantiate_dragon_bones_data_name);
	ClassDB::bind_method(D_METHOD("get_instantiate_dragon_bones_data_name"), &DragonBones::get_instantiate_dragon_bones_data_name);

	ClassDB::bind_method(D_METHOD("set_instantiate_armature_name", "instantiate_armature_name"), &DragonBones::set_instantiate_armature_name);
	ClassDB::bind_method(D_METHOD("get_instantiate_armature_name"), &DragonBones::get_instantiate_armature_name);

	ClassDB::bind_method(D_METHOD("set_instantiate_skin_name", "instantiate_skin_name"), &DragonBones::set_instantiate_skin_name);
	ClassDB::bind_method(D_METHOD("get_instantiate_skin_name"), &DragonBones::get_instantiate_skin_name);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "armature", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE, DragonBonesArmature::get_class_static()), "set_armature_readonly", "get_armature");

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
}

DragonBones::DragonBones() {
	dragonbones_instance = memnew(dragonBones::DragonBones(this));
}

DragonBones::~DragonBones() {
	if (armature) {
		armature->release(); // 已经处理内存的释放
		armature = nullptr;
	}

	dragonbones_instance->advanceTime(0.0f); // NOTE: 确保 dragonBones::DragonBones 自身缓存的待销毁对象被销毁!
	memdelete(dragonbones_instance);
	dragonbones_instance = nullptr;

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

HashMap<CanvasItemMaterial::BlendMode, Ref<CanvasItemMaterial>> DragonBones::blend_materials;
void DragonBones::clear_static() {
	blend_materials.clear();
}

RID DragonBones::get_blend_material(CanvasItemMaterial::BlendMode p_blend_mode) {
	auto it = blend_materials.find(p_blend_mode);
	if (it == blend_materials.end()) {
		Ref<CanvasItemMaterial> mat;
		mat.instantiate();
		mat->set_blend_mode(p_blend_mode);
		return blend_materials.insert(p_blend_mode, mat)->value->get_rid();
	}
	return it->value->get_rid();
}
