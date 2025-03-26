#pragma once

#include <godot_dragonbones.h>

#include <dragonBones/event/EventObject.h>
#include <godot_cpp/classes/ref_counted.hpp>

namespace godot {

class DragonBonesUserData : public RefCounted {
	GDCLASS(DragonBonesUserData, RefCounted)

	using v_size_t = int64_t;

private:
	class dragonBones::UserData *user_data{ nullptr };

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

class DragonBonesEventObject : public RefCounted {
	GDCLASS(DragonBonesEventObject, RefCounted)

protected:
	static void _bind_methods();

public:
	enum Type {
		TYPE_ANIM_START,
		TYPE_ANIM_LOOP_COMPLETE,
		TYPE_ANIM_COMPLETE,
		TYPE_ANIM_FADE_IN,
		TYPE_ANIM_FADE_IN_COMPLETE,
		TYPE_ANIM_FADE_OUT,
		TYPE_ANIM_FADE_OUT_COMPLETE,
		TYPE_FRAME_EVENT,
		TYPE_SOUND_EVENT,
		TYPE_CUSTOM,
		TYPE_MAX,
	};

	DragonBonesEventObject() = default;
	DragonBonesEventObject(const dragonBones::EventObject *origin);

	void set_time(float p_time) { time = p_time; }
	float get_time() const { return time; }

	void set_type(Type p_type) { type = p_type; }
	Type get_type() const { return type; }

	void set_type_text(String p_type_text) { type_text = p_type_text; }
	String get_type_text() const { return type_text; }

	void set_name(String p_name) { name = p_name; }
	String get_name() const { return name; }

	void set_armature(class DragonBonesArmature *p_armature) { armature = p_armature; }
	class DragonBonesArmature *get_armature() const { return armature; }

	void set_bone(String p_bone) { bone = p_bone; }
	String get_bone() const { return bone; }

	void set_slot(String p_slot) { slot = p_slot; }
	String get_slot() const { return slot; }

	void set_data(Ref<DragonBonesUserData> p_data) { data = p_data; }
	Ref<DragonBonesUserData> get_data() const { return data; }

private:
	float time;
	Type type{ TYPE_MAX };
	String type_text;
	String name;
	class DragonBonesArmature *armature;
	String bone;
	String slot;
	// AnimationState *animationState; // TODO
	Ref<DragonBonesUserData> data;
};

}; //namespace godot
VARIANT_ENUM_CAST(godot::DragonBonesEventObject::Type);