#pragma once

#include "dragonBones/core/DragonBones.h"
#include "dragonbones_armature.h"
#include "dragonbones_factory.h"

namespace godot {
/// TODO: 修改dragonBones库的new delete,供给Godot追踪内存
class DragonBones : public Node2D, public dragonBones::IEventDispatcher {
	GDCLASS(DragonBones, Node2D)

public:
	// sound IEventDispatcher
	virtual void addDBEventListener(const std::string &type, const std::function<void(dragonBones::EventObject *)> &listener) override {}
	virtual void removeDBEventListener(const std::string &type, const std::function<void(dragonBones::EventObject *)> &listener) override {}
	virtual bool hasDBEventListener(const std::string &type) const override { return true; }

	virtual void dispatchDBEvent(const std::string &p_type, dragonBones::EventObject *p_value) override;

	enum AnimationCallbackModeProcess {
		ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS = 0,
		ANIMATION_CALLBACK_MODE_PROCESS_IDLE = 1,
		ANIMATION_CALLBACK_MODE_PROCESS_MANUAL = 2,
	};

private:
	dragonBones::DragonBones *p_instance{ nullptr };

	Ref<DragonBonesFactory> m_res;
	DragonBonesArmature *main_armature{ nullptr };
	AnimationCallbackModeProcess callback_mode_process{ ANIMATION_CALLBACK_MODE_PROCESS_IDLE };
	String instantiate_dragon_bones_data_name{ "" };
	String instantiate_armature_name{ "" };
	String instantiate_skin_name{ "" };
	float f_time_scale{ 1.0f };
	float f_progress{ 0.0f };
	int c_loop{ 0 };
	bool b_active{ true };
	bool processing{ false };
	bool b_playing{ false };
	bool b_debug{ false };
	bool b_initialized{ false };
	bool b_try_playing{ false };

	bool b_flip_x{ false };
	bool b_flip_y{ false };

	bool armatures_inherit_material{ true };

	RID draw_mesh;

protected:
	static void _bind_methods();
	_DEFINE_TO_STRING()

	bool _set(const StringName &_str_name, const Variant &_c_r_value);
	bool _get(const StringName &_str_name, Variant &_r_ret) const;
	void _get_property_list(List<PropertyInfo> *_p_list) const;

#ifdef TOOLS_ENABLED
	void _validate_property(PropertyInfo &p_property) const;
#endif // TOOLS_ENABLED

	void _notification(int p_what);

public:
	DragonBones();
	~DragonBones();

	virtual void _draw() override;

	void _cleanup(bool p_for_destructor = false);

	// to initial pose current animation
	void _reset();

	// setters/getters
	void set_factory(const Ref<DragonBonesFactory> &_p_data);
	Ref<DragonBonesFactory> get_factory() const;

	void set_inherit_material(bool _b_enable);
	bool is_material_inherited() const;

	void set_active(bool _b_active);
	bool is_active() const;

	void set_time_scale(float p_time_scale);
	float get_time_scale() const;

	void set_instantiate_dragon_bones_data_name(String p_name);
	String get_instantiate_dragon_bones_data_name() const;

	void set_instantiate_armature_name(String p_name);
	String get_instantiate_armature_name() const;

	void set_instantiate_skin_name(String p_name);
	String get_instantiate_skin_name() const;

	void set_callback_mode_process(AnimationCallbackModeProcess _mode);
	AnimationCallbackModeProcess get_callback_mode_process() const;

	int get_animation_loop() const;
	void set_animation_loop(int p_animation_loop);

	void advance(float p_delta) {
		if (p_instance) {
			p_instance->advanceTime(p_delta);
		}
	}

	void set_debug(bool _b_debug);
	bool is_debug() const;

	/* deprecated */ void set_flip_x(bool _b_flip);
	/* deprecated */ bool is_flipped_x() const;
	/* deprecated */ void set_flip_y(bool _b_flip);
	/* deprecated */ bool is_flipped_y() const;

	DragonBonesArmature *get_armature();
	void set_armature(DragonBonesArmature *) const; // readonly

	void for_each_armature_(const Callable &p_action);

	template <class FUNC, std::enable_if_t<std::is_invocable_v<FUNC, DragonBonesArmature *, int>> *_dummy = nullptr>
	void for_each_armature(FUNC &&p_action) {
		if (!main_armature) {
			return;
		}

		if constexpr (std::is_invocable_r_v<bool, FUNC, DragonBonesArmature *, int>) {
			if (p_action(main_armature, 0)) {
				return;
			}
		} else {
			p_action(main_armature, 0);
		}
		main_armature->for_each_armature_recursively(p_action, 1);
	}

private:
	void _set_process(bool p_process, bool p_force = false);
	void _on_resource_changed();

	void set_armature_settings(const Dictionary &p_settings) const;
	Dictionary get_armature_settings() const;

#ifdef TOOLS_ENABLED
	mutable Ref<DragonBonesArmatureProxy> main_armature_ref;
#endif // TOOLS_ENABLED
};

/**
 */
class DragonBonesUserData : public RefCounted {
	GDCLASS(DragonBonesUserData, RefCounted)

	using v_size_t = int64_t;

private:
	dragonBones::UserData *user_data{ nullptr };

protected:
	static void _bind_methods();
	_DEFINE_TO_STRING()

public:
	DragonBonesUserData() = default;
	DragonBonesUserData(dragonBones::UserData *p_user_data) :
			user_data(p_user_data) {};

	bool has_data() const { return user_data; }

	PackedInt32Array get_ints() const;
	void set_ints(const PackedInt32Array &); // readonly

	PackedFloat32Array get_floats() const;
	void set_floats(const PackedFloat32Array &); // readonly

	PackedStringArray get_strings() const;
	void set_strings(const PackedStringArray &); // readonly

	int get_int(v_size_t p_index = 0) const;
	float get_float(v_size_t p_index = 0) const;
	String get_string(v_size_t p_index = 0) const;

	v_size_t get_ints_size() const;
	v_size_t get_floats_size() const;
	v_size_t get_strings_size() const;
};

} //namespace godot

VARIANT_ENUM_CAST(godot::DragonBones::AnimationCallbackModeProcess);
