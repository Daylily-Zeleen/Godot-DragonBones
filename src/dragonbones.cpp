#include "dragonbones.h"

#include "dragonBones/event/EventObject.h"
#include "godot_cpp/classes/engine.hpp"

#include "dragonbones_armature.h"
#include "godot_cpp/classes/lightmap_gi.hpp"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/templates/vmap.hpp"
#include "godot_cpp/variant/array.hpp"

using namespace godot;

#define SNAME(sn) ([] {static const StringName ret{sn};return ret; }())

/////////////////////////////////////////////////////////////////
void DragonBones::_cleanup(bool p_for_destructor) {
	b_initialized = false;

	if (!p_for_destructor) {
		// 析构时子节点已被释放
		if (main_armature) {
			main_armature->dispose(true);
		} else {
			ERR_PRINT("Unreachable case.");
		}
	}

	if (p_instance) {
		memdelete(p_instance);
		p_instance = nullptr;
	}

	m_res.unref();
}

void DragonBones::dispatchDBEvent(const std::string &p_type, dragonBones::EventObject *p_value) {
	using namespace dragonBones;
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	DragonBonesArmature *armature_proxy = static_cast<DragonBonesArmature *>(p_value->getArmature()->getDisplay());
	String anim_name = to_gd_str(p_value->animationState->name);

	if (p_type == EventObject::START) {
		emit_signal("start", armature_proxy, anim_name);
	} else if (p_type == EventObject::LOOP_COMPLETE) {
		emit_signal("loop_completed", armature_proxy, anim_name);
	} else if (p_type == EventObject::COMPLETE) {
		emit_signal("completed", armature_proxy, anim_name);
	} else if (p_type == EventObject::FADE_IN) {
		emit_signal("fade_in_start", armature_proxy, anim_name);
	} else if (p_type == EventObject::FADE_IN_COMPLETE) {
		emit_signal("fade_in_completed", armature_proxy, anim_name);
	} else if (p_type == EventObject::FADE_OUT) {
		emit_signal("fade_out_start", armature_proxy, anim_name);
	} else if (p_type == EventObject::FADE_OUT_COMPLETE) {
		emit_signal("fade_out_completed", armature_proxy, anim_name);
	} else if (p_type == EventObject::FRAME_EVENT) {
		String event_name = to_gd_str(p_value->name);
		Ref<DragonBonesUserData> user_data{ memnew(DragonBonesUserData(p_value->getData())) };
		// TODO:: 是否需要包装 EventObj 与 ActionData？
		emit_signal("frame_event", armature_proxy, anim_name, event_name, user_data);
	} else if (p_type == EventObject::SOUND_EVENT) {
		String anim_name = to_gd_str(p_value->animationState->name);
		String event_name = to_gd_str(p_value->name);
		Ref<DragonBonesUserData> user_data{ memnew(DragonBonesUserData(p_value->getData())) };
		emit_signal("sound_event", armature_proxy, anim_name, event_name, user_data);
	}
}

void DragonBones::_set_process(bool p_process, bool p_force) {
	if (processing == p_process && !p_force) {
		return;
	}

	set_physics_process_internal(callback_mode_process == ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS && p_process && b_active);
	set_process_internal(callback_mode_process == ANIMATION_CALLBACK_MODE_PROCESS_IDLE && p_process && b_active);

	processing = p_process;
}

void DragonBones::_on_resource_changed() {
#ifdef TOOLS_ENABLED
	auto armatures_settings = get_armature_settings();
#endif // TOOLS_ENABLED
	// 重设资源本身
	auto to_set = m_res;
	set_factory({});
	set_factory(to_set);
#ifdef TOOLS_ENABLED
	if (main_armature->is_initialized()) {
		set_armature_settings(armatures_settings);
	}
#endif // TOOLS_ENABLED
}

void DragonBones::set_factory(const Ref<DragonBonesFactory> &_p_data) {
	using namespace dragonBones;
	if (m_res == _p_data) {
		return;
	}

	if (main_armature->is_initialized()) {
		main_armature->stop_all_animations(false, true);
	}

	static const StringName sn{ "changed" };
	auto cb = callable_mp(this, &DragonBones::_on_resource_changed);
	if (m_res.is_valid() && m_res->is_connected(sn, cb)) {
		m_res->disconnect(sn, cb);
	}

	_cleanup(false);
	m_res = _p_data;

	if (m_res.is_null()) {
		notify_property_list_changed();
		return;
	} else if (!m_res->is_connected(sn, cb)) {
		m_res->connect(sn, cb);
	}

	if (!m_res->can_create_dragon_bones_instance()) {
#ifdef TOOLS_ENABLED
		if (!m_res->is_imported()) {
			// 只对非导入工厂打印错误信息
			WARN_PRINT(vformat("DragonBonesFactory \"%s\" is invalid, please setup its properties.", m_res));
		}
#else // !TOOLS_ENABLED
		WARN_PRINT(vformat("DragonBonesFactory \"%s\" is invalid, please setup its properties.", m_res));
#endif //  TOOLS_ENABLED
		return;
	}

	// build Armature display
	p_instance = m_res->create_dragon_bones(this, main_armature, instantiate_dragon_bones_data_name, instantiate_armature_name, instantiate_skin_name);
	ERR_FAIL_COND(!main_armature->is_initialized());

	// update flip
	set_flip_x(b_flip_x);
	set_flip_y(b_flip_y);
	// Update time scale
	set_time_scale(f_time_scale);

	main_armature->setup_recursively();

	b_initialized = true;

	// update color and opacity and blending
	main_armature->update_childs(true, true);

	// update material inheritance
	main_armature->update_material_inheritance_recursively(armatures_inherit_material);

	main_armature->advance(0);

	notify_property_list_changed();
	queue_redraw();
}

Ref<DragonBonesFactory> DragonBones::get_factory() const {
	return m_res;
}

void DragonBones::set_inherit_material(bool _b_enable) {
	armatures_inherit_material = _b_enable;
	if (main_armature->is_initialized()) {
		main_armature->update_material_inheritance_recursively(armatures_inherit_material);
	}
}

bool DragonBones::is_material_inherited() const {
	return armatures_inherit_material;
}

void DragonBones::set_active(bool _b_active) {
	b_active = _b_active;
	_set_process(b_active, true);
}

bool DragonBones::is_active() const {
	return b_active;
}

void DragonBones::set_debug(bool _b_debug) {
	b_debug = _b_debug;
}

bool DragonBones::is_debug() const {
	return b_debug;
}

void DragonBones::set_time_scale(float p_time_scale) {
	f_time_scale = p_time_scale < 0.0 ? 0.0 : p_time_scale;
}

float DragonBones::get_time_scale() const {
	return f_time_scale;
}

void DragonBones::set_instantiate_dragon_bones_data_name(String p_name) {
	if (p_name == "[default]") {
		p_name = "";
	}
	if (p_name == instantiate_dragon_bones_data_name) {
		return;
	}

	instantiate_dragon_bones_data_name = p_name;
	_on_resource_changed();
}

String DragonBones::get_instantiate_dragon_bones_data_name() const {
	return instantiate_dragon_bones_data_name;
}

void DragonBones::set_instantiate_armature_name(String p_name) {
	if (p_name == "[default]") {
		p_name = "";
	}
	if (p_name == instantiate_armature_name) {
		return;
	}

	instantiate_armature_name = p_name;
	_on_resource_changed();
}

String DragonBones::get_instantiate_armature_name() const {
	return instantiate_armature_name;
}

void DragonBones::set_instantiate_skin_name(String p_name) {
	if (p_name == "[default]") {
		p_name = "";
	}
	if (p_name == instantiate_skin_name) {
		return;
	}

	instantiate_skin_name = p_name;
	_on_resource_changed();
}

String DragonBones::get_instantiate_skin_name() const {
	return instantiate_skin_name;
}

void DragonBones::set_callback_mode_process(AnimationCallbackModeProcess _mode) {
	callback_mode_process = _mode;
}

DragonBones::AnimationCallbackModeProcess DragonBones::get_callback_mode_process() const {
	return callback_mode_process;
}

int DragonBones::get_animation_loop() const {
	return c_loop;
}

void DragonBones::set_animation_loop(int p_animation_loop) {
	c_loop = p_animation_loop;
	if (b_initialized && b_playing) {
		_reset();
		main_armature->play(main_armature->get_current_animation(), c_loop);
	}
}

void DragonBones::_reset() {
	ERR_FAIL_COND(!main_armature->is_initialized());
	main_armature->reset(true);
}

DragonBonesArmature *DragonBones::get_armature() {
	if (main_armature->is_initialized()) {
		// Only return armature when it is initialized.
		return main_armature;
	}
	return nullptr;
}

void DragonBones::set_armature(DragonBonesArmature *) const {
	ERR_FAIL_MSG("DragonBones's property \"armature\" is readonly.");
}

void DragonBones::set_flip_x(bool _b_flip) {
	b_flip_x = _b_flip;
	if (main_armature->is_initialized()) {
		main_armature->set_flip_x(_b_flip, true);
	}
}

bool DragonBones::is_flipped_x() const {
	return b_flip_x;
}

void DragonBones::set_flip_y(bool _b_flip) {
	b_flip_y = _b_flip;
	if (main_armature->is_initialized()) {
		main_armature->set_flip_y(_b_flip, true);
	}
}

bool DragonBones::is_flipped_y() const {
	return b_flip_y;
}

void DragonBones::set_armature_settings(const Dictionary &p_settings) const {
	if (main_armature->is_initialized()) {
		main_armature->set_settings(p_settings);
	} else {
#ifdef TOOLS_ENABLED
		if (!m_res->is_imported()) {
			// 只对非导入工厂打印错误信息，导入工厂将在后续重新导入
			WARN_PRINT_ED("main_armature is invalid, can't set armature settings.");
		}
#else // !TOOLS_ENABLED
		WARN_PRINT_ED("main_armature is invalid, can't set armature settings.");
#endif // !TOOLS_ENABLED
	}
}

Dictionary DragonBones::get_armature_settings() const {
	if (!main_armature->is_initialized()) {
		return {};
	}
#ifdef TOOLS_ENABLED
	return main_armature->get_settings();
#else //TOOLS_ENABLED
	ERR_FAIL_V_MSG({}, "DragonBones::get_armature_settings() can be call in editor build only.");
#endif // TOOLS_ENABLED
}

bool DragonBones::_set(const StringName &_str_name, const Variant &_c_r_value) {
	if (_str_name == SNAME("armature_settings")) {
		set_armature_settings(_c_r_value);
		return true;
	}
#ifdef TOOLS_ENABLED
	else if (_str_name == SNAME("main_armature")) {
		return true; // 禁止设置
	}
#endif //  TOOLS_ENABLED

	return false;
}

bool DragonBones::_get(const StringName &_str_name, Variant &_r_ret) const {
	if (_str_name == SNAME("armature_settings")) {
		_r_ret = get_armature_settings();
		return true;
	}
#ifdef TOOLS_ENABLED
	else if (_str_name == SNAME("main_armature")) {
		// Avoid instantiation when getting default value.
		if (main_armature && main_armature->is_initialized() && main_armature_ref.is_null()) {
			main_armature_ref.instantiate();
		}

		if (main_armature_ref.is_valid()) {
			main_armature_ref->armature_node = main_armature;
		}

		_r_ret = main_armature_ref;
		return true;
	}
#endif // TOOLS_ENABLED
	return false;
}

void DragonBones::_get_property_list(List<PropertyInfo> *_p_list) const {
	_p_list->push_back(PropertyInfo(Variant::DICTIONARY, "armature_settings", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));

#ifdef TOOLS_ENABLED
	if (main_armature && Engine::get_singleton()->is_editor_hint()) {
		_p_list->push_back(PropertyInfo(
				Variant::OBJECT, "main_armature", PROPERTY_HINT_RESOURCE_TYPE, DragonBonesArmatureProxy::get_class_static(), PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT, DragonBonesArmatureProxy::get_class_static()));
	}
#endif // TOOLS_ENABLED
}

#ifdef TOOLS_ENABLED
void DragonBones::_validate_property(PropertyInfo &p_property) const {
	if (!Engine::get_singleton()->is_editor_hint() || m_res.is_null()) {
		return;
	}
	if (p_property.name == SNAME("instantiate_dragon_bones_data_name")) {
		String hint = "[default]";

		for (const auto &name : m_res->get_loaded_dragon_bones_data_name_list()) {
			hint += ",";
			hint += name;
		}

		p_property.hint_string = hint;
	} else if (p_property.name == SNAME("instantiate_armature_name")) {
		String hint = "[default]";

		for (const auto &name : m_res->get_loaded_dragon_bones_armature_name_list(instantiate_dragon_bones_data_name)) {
			hint += ",";
			hint += name;
		}

		p_property.hint_string = hint;
	} else if (p_property.name == SNAME("instantiate_skin_name")) {
		String hint = "[default]";

		for (const auto &name : m_res->get_loaded_dragon_bones_main_skin_name_list(instantiate_dragon_bones_data_name, instantiate_armature_name)) {
			if (name == "default") {
				continue;
			}
			hint += ",";
			hint += name;
		}

		p_property.hint_string = hint;
	}
}
#endif // TOOLS_ENABLED

void DragonBones::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_set_process(b_active);
			if (!processing) {
				set_physics_process_internal(false);
				set_process_internal(false);
			}
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (b_active && callback_mode_process == ANIMATION_CALLBACK_MODE_PROCESS_IDLE) {
				advance(get_process_delta_time() * f_time_scale);
			}
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (b_active && callback_mode_process == ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS) {
				advance(get_physics_process_delta_time() * f_time_scale);
			}
		} break;
	}
}

static const Ref<CanvasItemMaterial> &get_blend_material(CanvasItemMaterial::BlendMode p_blend_mode) {
	static VMap<CanvasItemMaterial::BlendMode, Ref<CanvasItemMaterial>> blend_materials;
	auto idx = blend_materials.find(p_blend_mode);
	if (idx < 0) {
		Ref<CanvasItemMaterial> mat;
		mat.instantiate();
		mat->set_blend_mode(p_blend_mode);
		idx = blend_materials.insert(p_blend_mode, mat);
	}
	return blend_materials.getv(idx);
}

void DragonBones::_draw() {
	if (!main_armature) {
		return;
	}

	VMap<int, LocalVector<DrawData>> draw_data;
	main_armature->append_draw_data(draw_data);

	if (draw_data.is_empty()) {
		return;
	}

	const auto RS = RenderingServer::get_singleton();
	if (draw_mesh.is_valid()) {
		RS->mesh_clear(draw_mesh);
	} else {
		draw_mesh = RS->mesh_create();
	}

	const auto pairs = draw_data.get_array();

	RID texture; // TODO 根据纹理不同创建不同的材质或另外的mesh

	struct SurfaceData {
		PackedInt32Array indices;
		PackedVector2Array vertices;
		PackedColorArray colors;
		PackedVector2Array uv;

		CanvasItemMaterial::BlendMode blend_mode;

		SurfaceData() :
				blend_mode(CanvasItemMaterial::BLEND_MODE_MIX) {}
		SurfaceData(CanvasItemMaterial::BlendMode p_blend_mode) :
				blend_mode(p_blend_mode) {}
	};

	LocalVector<SurfaceData> surfaces{ SurfaceData(draw_data.getv(0)[0].blend_mode) };

	for (auto i = 0; i < draw_data.size(); ++i) {
		for (const auto &draw_data : pairs[i].value) {
			auto &surface_data = surfaces[surfaces.size() - 1];

			if (surface_data.blend_mode != draw_data.blend_mode) {
				surfaces.push_back(SurfaceData(draw_data.blend_mode));
				surface_data = surfaces[surfaces.size() - 1];
			}

			int base_index = surface_data.vertices.size();
			int insert_begin_index = surface_data.indices.size();

			surface_data.indices.resize(surface_data.indices.size() + draw_data.indices.size());
			for (int idx = 0; idx < draw_data.indices.size(); ++idx) {
				surface_data.indices[idx + insert_begin_index] = draw_data.indices[idx] + base_index;
			}

			surface_data.vertices.append_array(draw_data.transform.xform(draw_data.vertices));
			surface_data.colors.append_array(draw_data.colors);
			surface_data.uv.append_array(draw_data.uvs);

			surface_data.blend_mode = draw_data.blend_mode;

			if (draw_data.texture.is_valid()) {
				texture = draw_data.texture->get_rid();
			}
		}
	}

	for (int i = 0; i < surfaces.size(); ++i) {
		const SurfaceData &surface_data = surfaces[i];
		Array arr;
		arr.resize(RenderingServer::ARRAY_MAX);
		arr[RenderingServer::ARRAY_INDEX] = surface_data.indices;
		arr[RenderingServer::ARRAY_VERTEX] = surface_data.vertices;
		arr[RenderingServer::ARRAY_COLOR] = surface_data.colors;
		arr[RenderingServer::ARRAY_TEX_UV] = surface_data.uv;

		RS->mesh_add_surface_from_arrays(draw_mesh, RenderingServer::PRIMITIVE_TRIANGLES, arr);
		if (surface_data.blend_mode != CanvasItemMaterial::BLEND_MODE_MIX) {
			auto mat = get_blend_material(surface_data.blend_mode);
			RS->mesh_surface_set_material(draw_mesh, i, mat->get_rid());
		}
	}

	RS->canvas_item_add_mesh(get_canvas_item(), draw_mesh, get_canvas_transform(), get_modulate(), texture);
}

void DragonBones::for_each_armature_(const Callable &p_action) {
	for_each_armature(
			[&](auto main_armature, auto depth) {
				return p_action.call(main_armature, depth).booleanize();
			});
}

void DragonBones::_bind_methods() {
	ClassDB::bind_method(D_METHOD("for_each_armature", "action"), &DragonBones::for_each_armature_);

	ClassDB::bind_method(D_METHOD("set_factory", "dbfactory"), &DragonBones::set_factory);
	ClassDB::bind_method(D_METHOD("get_factory"), &DragonBones::get_factory);

	ClassDB::bind_method(D_METHOD("set_inherit_material"), &DragonBones::set_inherit_material);
	ClassDB::bind_method(D_METHOD("is_material_inherited"), &DragonBones::is_material_inherited);

	ClassDB::bind_method(D_METHOD("set_flip_x", "enable_flip"), &DragonBones::set_flip_x);
	ClassDB::bind_method(D_METHOD("is_flipped_x"), &DragonBones::is_flipped_x);
	ClassDB::bind_method(D_METHOD("set_flip_y", "enable_flip"), &DragonBones::set_flip_y);
	ClassDB::bind_method(D_METHOD("is_flipped_y"), &DragonBones::is_flipped_y);

	ClassDB::bind_method(D_METHOD("advance", "delta"), &DragonBones::advance);

	ClassDB::bind_method(D_METHOD("set_animation_loop", "loop_count"), &DragonBones::set_animation_loop);
	ClassDB::bind_method(D_METHOD("get_animation_loop"), &DragonBones::get_animation_loop);

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

	ADD_GROUP("Flip", "flip_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_x"), "set_flip_x", "is_flipped_x");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_y"), "set_flip_y", "is_flipped_y");

	ADD_GROUP("Animation Settings", "animation_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "animation_loop", PROPERTY_HINT_RANGE, "0,100,1,or_greater"), "set_animation_loop", "get_animation_loop");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "animation_time_scale", PROPERTY_HINT_RANGE, "0,10,0.01"), "set_time_scale", "get_time_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "animation_callback_mode_process", PROPERTY_HINT_ENUM, "Physics,Idle,Manual"), "set_callback_mode_process", "get_callback_mode_process");

	ADD_GROUP("Instantiate Settings", "instantiate_");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "instantiate_dragon_bones_data_name", PROPERTY_HINT_ENUM_SUGGESTION, "[default]"), "set_instantiate_dragon_bones_data_name", "get_instantiate_dragon_bones_data_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "instantiate_armature_name", PROPERTY_HINT_ENUM_SUGGESTION, "[default]"), "set_instantiate_armature_name", "get_instantiate_armature_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "instantiate_skin_name", PROPERTY_HINT_ENUM_SUGGESTION, "[default]"), "set_instantiate_skin_name", "get_instantiate_skin_name");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "armatures_use_this_material"), "set_inherit_material", "is_material_inherited");

	// 信号
	const auto armature_prop = PropertyInfo(Variant::OBJECT, "armature", PROPERTY_HINT_NONE, "", PROPERTY_HINT_NONE, DragonBonesArmature::get_class_static());
	const auto anim_name_prop = PropertyInfo(Variant::STRING, "anim_name");

	ADD_SIGNAL(MethodInfo("start", armature_prop, anim_name_prop));
	ADD_SIGNAL(MethodInfo("completed", armature_prop, anim_name_prop));
	ADD_SIGNAL(MethodInfo("loop_completed", armature_prop, anim_name_prop));
	ADD_SIGNAL(MethodInfo("fade_in_start", armature_prop, anim_name_prop));
	ADD_SIGNAL(MethodInfo("fade_in_completed", armature_prop, anim_name_prop));
	ADD_SIGNAL(MethodInfo("fade_out_start", armature_prop, anim_name_prop));
	ADD_SIGNAL(MethodInfo("fade_out_completed", armature_prop, anim_name_prop));

	const auto event_name_prop = PropertyInfo(Variant::STRING, "event_name");
	const auto user_data_prop = PropertyInfo(Variant::OBJECT, "event_data", PROPERTY_HINT_NONE, "", PROPERTY_HINT_NONE, DragonBonesUserData::get_class_static());
	ADD_SIGNAL(MethodInfo("frame_event", armature_prop, anim_name_prop, event_name_prop, user_data_prop));
	ADD_SIGNAL(MethodInfo("sound_event", armature_prop, anim_name_prop, event_name_prop, user_data_prop));

	// 枚举
	BIND_ENUM_CONSTANT(ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS);
	BIND_ENUM_CONSTANT(ANIMATION_CALLBACK_MODE_PROCESS_IDLE);
	BIND_ENUM_CONSTANT(ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);
}

DragonBones::DragonBones() {
	// 内部节点
	main_armature = memnew(DragonBonesArmature);
	main_armature->dragon_bones = this;
}

DragonBones::~DragonBones() {
	_cleanup(true);
	if (draw_mesh.is_valid()) {
		RenderingServer::get_singleton()->free_rid(draw_mesh);
	}
}

////////////////
PackedInt32Array DragonBonesUserData::get_ints() const {
	PackedInt32Array ret;
	if (!user_data) {
		return ret;
	}

	if (user_data->ints.size()) {
		ret.resize(user_data->ints.size());
		memcpy(ret.ptrw(), user_data->ints.data(), user_data->ints.size() * sizeof(int));
	}

	return ret;
}

void DragonBonesUserData::set_ints(const PackedInt32Array &) {
	ERR_FAIL_MSG("\"ints\" is readonly.");
}

PackedFloat32Array DragonBonesUserData::get_floats() const {
	PackedFloat32Array ret;
	if (!user_data) {
		return ret;
	}

	if (user_data->floats.size()) {
		ret.resize(user_data->floats.size());
		memcpy(ret.ptrw(), user_data->floats.data(), user_data->floats.size() * sizeof(int));
	}

	return ret;
}

void DragonBonesUserData::set_floats(const PackedFloat32Array &) {
	ERR_FAIL_MSG("\"floats\" is readonly.");
}

PackedStringArray DragonBonesUserData::get_strings() const {
	PackedStringArray ret;
	if (!user_data) {
		return ret;
	}

	if (user_data->strings.size()) {
		for (size_t i = 0; i < user_data->strings.size(); ++i) {
			ret[i] = to_gd_str(user_data->strings[i]);
		}
	}

	return ret;
}

void DragonBonesUserData::set_strings(const PackedStringArray &) {
	ERR_FAIL_MSG("\"strings\" is readonly.");
}

int DragonBonesUserData::get_int(DragonBonesUserData::v_size_t p_index) const {
	if (!user_data) {
		return {};
	}
	ERR_FAIL_INDEX_V(p_index, user_data->ints.size(), {});
	return user_data->ints[p_index];
}

float DragonBonesUserData::get_float(DragonBonesUserData::v_size_t p_index) const {
	if (!user_data) {
		return {};
	}
	ERR_FAIL_INDEX_V(p_index, user_data->floats.size(), {});
	return user_data->floats[p_index];
}

String DragonBonesUserData::get_string(DragonBonesUserData::v_size_t p_index) const {
	if (!user_data) {
		return {};
	}
	ERR_FAIL_INDEX_V(p_index, user_data->strings.size(), {});
	return to_gd_str(user_data->strings[p_index]);
}

DragonBonesUserData::v_size_t DragonBonesUserData::get_ints_size() const {
	if (!user_data) {
		return {};
	}
	return user_data->ints.size();
}
DragonBonesUserData::v_size_t DragonBonesUserData::get_floats_size() const {
	if (!user_data) {
		return {};
	}
	return user_data->floats.size();
}
DragonBonesUserData::v_size_t DragonBonesUserData::get_strings_size() const {
	if (!user_data) {
		return {};
	}
	return user_data->strings.size();
}

void DragonBonesUserData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("has_data"), &DragonBonesUserData::has_data);

	ClassDB::bind_method(D_METHOD("get_ints_size"), &DragonBonesUserData::get_ints_size);
	ClassDB::bind_method(D_METHOD("get_floats_size"), &DragonBonesUserData::get_floats_size);
	ClassDB::bind_method(D_METHOD("get_strings_size"), &DragonBonesUserData::get_strings_size);

	ClassDB::bind_method(D_METHOD("get_int", "index"), &DragonBonesUserData::get_int, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_float", "index"), &DragonBonesUserData::get_float, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_string", "index"), &DragonBonesUserData::get_string, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("get_ints"), &DragonBonesUserData::get_ints);
	ClassDB::bind_method(D_METHOD("set_ints_readonly", "_val"), &DragonBonesUserData::set_ints);

	ClassDB::bind_method(D_METHOD("get_floats"), &DragonBonesUserData::get_floats);
	ClassDB::bind_method(D_METHOD("set_floats_readonly", "_val"), &DragonBonesUserData::set_floats);

	ClassDB::bind_method(D_METHOD("get_strings"), &DragonBonesUserData::get_strings);
	ClassDB::bind_method(D_METHOD("set_strings_readonly", "_val"), &DragonBonesUserData::set_strings);

	ADD_PROPERTY(PropertyInfo(Variant::PACKED_INT32_ARRAY, "ints", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "set_ints_readonly", "get_ints");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "floats", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "set_floats_readonly", "get_floats");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "strings", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_READ_ONLY), "set_strings_readonly", "get_strings");
}