#pragma once

#include <dragonBones/armature/Bone.h>
#include <godot_cpp/classes/ref_counted.hpp>

namespace godot {

class DragonBonesBone : public RefCounted {
	GDCLASS(DragonBonesBone, RefCounted);

protected:
	dragonBones::Bone *boneData{ nullptr }; // 生命周期由 dragonBones::Armature 管理
	class DragonBonesArmature *armature{ nullptr };

public:
	enum OffsetMode {
		OFFSET_MODE_NONE,
		OFFSET_MODE_ADDITIVE,
		OFFSET_MODE_OVERRIDE,
	};

	DragonBonesBone() = default;
	DragonBonesBone(dragonBones::Bone *p_bone_data, DragonBonesArmature *p_armature) :
			boneData(p_bone_data), armature(p_armature) {}

	~DragonBonesBone() = default;

public:
	static void _bind_methods();
	String _to_string() const { return vformat("<%s#%s>", get_class_static(), get_instance_id()); }

	bool is_valid() const;
	String get_name() const;
	Ref<DragonBonesBone> get_parent() const;

	// Local
	Vector2 get_position() const;
	void set_position(Vector2 new_pos);

	float get_rotation() const;
	void set_rotation(float rotation);

	Vector2 get_scale() const;
	void set_scale(Vector2 scale);

	Transform2D get_transform() const;
	void set_transform(const Transform2D &p_transform);

	// Global
	void set_global_position(Vector2 new_pos);
	Vector2 get_global_position() const;

	void set_global_rotation(float rotation);
	float get_global_rotation() const;

	void set_global_scale(Vector2 scale);
	Vector2 get_global_scale() const;

	Transform2D get_global_transform() const;
	void set_global_transform(const Transform2D &p_transform);

	// Others
	OffsetMode get_offset_mode() const;
	Transform2D get_offset() const;
	Transform2D get_animation_pose() const;
	Transform2D get_origin() const;
};

} //namespace godot

VARIANT_ENUM_CAST(godot::DragonBonesBone::OffsetMode);