#pragma once

#include <godot_dragon_bones.h>

#include <dragonBones/core/DragonBones.h>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/templates/hash_map.hpp>

#include "armature.h"
#include "factory.h"

namespace godot {
class DragonBonesArmatureView : public Node2D {
	GDCLASS(DragonBonesArmatureView, Node2D)

public:
	enum AnimationCallbackModeProcess {
		ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS = 0,
		ANIMATION_CALLBACK_MODE_PROCESS_IDLE = 1,
		ANIMATION_CALLBACK_MODE_PROCESS_MANUAL = 2,
	};

	using AnimFadeOutMode = DragonBonesArmature::AnimFadeOutMode;

private:
	Ref<DragonBonesFactory> factory;
	DragonBonesArmature *armature{ nullptr };
	AnimationCallbackModeProcess callback_mode_process{ ANIMATION_CALLBACK_MODE_PROCESS_IDLE };

	String instantiate_dragon_bones_data_name{ "" };
	String instantiate_armature_name{ "" };
	String instantiate_skin_name{ "" };

	float time_scale{ 1.0f };
	int animation_loop_count{ 0 };
	bool active{ true };
	bool debug{ false };

	LocalVector<RID> draw_meshes;

#ifdef DEBUG_ENABLED
	RID debug_mesh;
#endif // DEBUG_ENABLED

protected:
	static void _bind_methods();
	_DEFINE_TO_STRING()

	bool _set(const StringName &p_name, const Variant &p_property);
	bool _get(const StringName &p_name, Variant &r_property) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

#ifdef TOOLS_ENABLED
	void _validate_property(PropertyInfo &p_property) const;
#endif // TOOLS_ENABLED

	void _notification(int p_what);

public:
	DragonBonesArmatureView();
	~DragonBonesArmatureView();

	virtual void _draw() override;

	// setters/getters
	void set_factory(const Ref<DragonBonesFactory> &p_factory);
	Ref<DragonBonesFactory> get_factory() const;

	void set_active(bool p_active);
	bool is_active() const;

	void set_time_scale(float p_time_scale);
	float get_time_scale() const;

	void set_instantiate_dragon_bones_data_name(String p_name);
	String get_instantiate_dragon_bones_data_name() const;

	void set_instantiate_armature_name(String p_name);
	String get_instantiate_armature_name() const;

	void set_instantiate_skin_name(String p_name);
	String get_instantiate_skin_name() const;

	void set_callback_mode_process(AnimationCallbackModeProcess p_mode);
	AnimationCallbackModeProcess get_callback_mode_process() const;

	int get_animation_loop_count() const;
	void set_animation_loop_count(int p_animation_loop_count);

	void advance(float p_delta) {
		if (armature) {
			armature->advance(p_delta, true);
		}
	}

	void set_debug(bool p_debug);
	bool is_debug() const;

	DragonBonesArmature *get_armature();

	static void clear_static();

public:
	// 包装
	void dispatch_event(const Ref<class DragonBonesEventObject> &p_event_object);

	bool has_animation(const String &p_animation_name) const;
	PackedStringArray get_animations();

	String get_current_animation_on_layer(int p_layer) const;
	String get_current_animation_in_group(const String &p_group_name) const;

	float tell_animation(const String &p_animation_name) const;
	void seek_animation(const String &p_animation_name, float p_progress);

	bool is_playing() const;
	void play(const String &p_animation_name, int p_loop_count = -1);

	void play_from_time(const String &p_animation_name, float p_time, int p_loop_count = -1);
	void play_from_progress(const String &p_animation_name, float p_progress, int p_loop_count = -1);
	void stop(const String &p_animation_name, bool b_reset = false, bool p_recursively = false);
	void stop_all_animations(bool b_reset = false, bool p_recursively = false);
	void fade_in(const String &p_animation_name, float p_time,
			int p_loop_count, int p_layer, const String &p_group, AnimFadeOutMode p_fade_out_mode);

	bool has_slot(const String &p_slot_name) const;
	Ref<DragonBonesSlot> get_slot(const String &p_slot_name);
	SlotsDictionary get_slots();

	ConstraintsDictionary get_ik_constraints();
	void set_ik_constraint(const String &p_name, Vector2 p_position);
	void set_ik_constraint_bend_positive(const String &p_name, bool p_bend_positive);

	BonesDictionary get_bones();
	Ref<DragonBonesBone> get_bone(const String &p_name);

	Rect2 get_rect() const;
	Rect2 get_global_rect() const;

	// setget
	void set_current_animation(const String &p_animation);
	String get_current_animation() const;

	void set_animation_progress(float p_progress);
	float get_animation_progress() const;

	void set_flip_x_(bool p_flip_x) { set_flip_x(p_flip_x); }
	void set_flip_x(bool p_flip_x, bool p_recursively = false);
	bool is_flipped_x() const;

	void set_flip_y_(bool p_flip_y) { set_flip_y(p_flip_y); }
	void set_flip_y(bool p_flip_y, bool p_recursively = false);
	bool is_flipped_y() const;

	Ref<Texture2D> get_texture_override() const;
	void set_texture_override(const Ref<Texture2D> &p_texture_override);

private:
	bool is_armature_valid() const { return armature != nullptr && armature->is_valid(); }
	RID get_draw_mesh(int p_index);

	void reset();
	void rebuild_armature();

	void set_armature_settings(const Dictionary &p_settings) const;
	Dictionary get_armature_settings() const;

	static HashMap<CanvasItemMaterial::BlendMode, Ref<CanvasItemMaterial>> blend_materials;
	static RID get_blend_material(CanvasItemMaterial::BlendMode p_blend_mode);

#ifdef TOOLS_ENABLED
	mutable Ref<DragonBonesArmatureProxy> armature_ref;
#endif // TOOLS_ENABLED
};

} //namespace godot

VARIANT_ENUM_CAST(godot::DragonBonesArmatureView::AnimationCallbackModeProcess);
