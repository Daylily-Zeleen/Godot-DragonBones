#pragma once

#include <godot_dragonbones.h>

#include <dragonBones/core/DragonBones.h>
#include <godot_cpp/classes/node2d.hpp>

#include "armature.h"
#include "factory.h"

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
	dragonBones::DragonBones *dragonbones_instance{ nullptr };
	friend DragonBonesFactory;

	Ref<DragonBonesFactory> factory;
	DragonBonesArmature *armature{ nullptr };
	AnimationCallbackModeProcess callback_mode_process{ ANIMATION_CALLBACK_MODE_PROCESS_IDLE };
	String instantiate_dragon_bones_data_name{ "" };
	String instantiate_armature_name{ "" };
	String instantiate_skin_name{ "" };
	float time_scale{ 1.0f };
	int c_loop{ 0 };
	bool b_active{ true };
	bool processing{ false };
	bool b_playing{ false };
	bool b_debug{ false };
	bool b_try_playing{ false };

	bool b_flip_x{ false };
	bool b_flip_y{ false };

	LocalVector<RID> draw_meshes;

#ifdef DEBUG_ENABLED
	RID debug_mesh;
#endif // DEBUG_ENABLED

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

	// setters/getters
	void set_factory(const Ref<DragonBonesFactory> &_p_data);
	Ref<DragonBonesFactory> get_factory() const;

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
		if (dragonbones_instance) {
			dragonbones_instance->advanceTime(p_delta);
		}
	}

	void set_debug(bool _b_debug);
	bool is_debug() const;

	DragonBonesArmature *get_armature();
	void set_armature(DragonBonesArmature *) const; // readonly

	void for_each_armature_(const Callable &p_action);

	template <class FUNC, std::enable_if_t<std::is_invocable_v<FUNC, DragonBonesArmature *, int>> *_dummy = nullptr>
	void for_each_armature(FUNC &&p_action) {
		if (!armature) {
			return;
		}

		if constexpr (std::is_invocable_r_v<bool, FUNC, DragonBonesArmature *, int>) {
			if (p_action(armature, 0)) {
				return;
			}
		} else {
			p_action(armature, 0);
		}
		armature->for_each_armature_recursively(p_action, 1);
	}

	static void clear_static();

private:
	bool is_armature_valid() const { return armature != nullptr && armature->is_valid(); }
	RID get_draw_mesh(int p_index);

	void reset();
	void rebuild_armature();

	void _set_process(bool p_process, bool p_force = false);
	// void _on_resource_changed();

	void set_armature_settings(const Dictionary &p_settings) const;
	Dictionary get_armature_settings() const;

	static HashMap<CanvasItemMaterial::BlendMode, Ref<CanvasItemMaterial>> blend_materials;
	static RID get_blend_material(CanvasItemMaterial::BlendMode p_blend_mode);
#ifdef TOOLS_ENABLED
	mutable Ref<DragonBonesArmatureProxy> armature_ref;
#endif // TOOLS_ENABLED
};

} //namespace godot

VARIANT_ENUM_CAST(godot::DragonBones::AnimationCallbackModeProcess);
