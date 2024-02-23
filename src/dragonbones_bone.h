#pragma once

#include "dragonBones/armature/Bone.h"
#include "godot_cpp/classes/node2d.hpp"
#include "godot_cpp/classes/ref_counted.hpp"

namespace godot {

class DragonBonesBone : public RefCounted {
	GDCLASS(DragonBonesBone, RefCounted);

protected:
	dragonBones::Bone *boneData{ nullptr }; // 生命周期由 dragonBones::ArmatureData 管理
	Node2D *armature{ nullptr };

public:
	DragonBonesBone() = default;
	DragonBonesBone(dragonBones::Bone *p_bone_data, Node2D *p_armature) :
			boneData(p_bone_data), armature(p_armature) {}

	~DragonBonesBone() = default;

public:
	static void _bind_methods();

	String get_name() const;

	Vector2 get_bone_position() const;
	void set_bone_position(Vector2 new_pos);

	Vector2 get_bone_scale() const;
	void set_bone_scale(Vector2 scale);

	float get_bone_rotation() const;
	void set_bone_rotation(float rotation);

	void set_bone_global_position(Vector2 new_pos);
	Vector2 get_bone_global_position() const;

	void set_bone_global_rotation(float rotation);
	float get_bone_global_rotation() const;

	void set_bone_global_scale(Vector2 scale);
	Vector2 get_bone_global_scale() const;

	Vector2 get_bone_offset_position() const;
	Vector2 get_bone_animation_position() const;
	Vector2 get_bone_origin_position() const;

	Transform2D get_bone_transform() const;
	void set_bone_transform(const Transform2D &p_transform);

	Transform2D get_bone_global_transform() const;
	void set_bone_global_transform(const Transform2D &p_transform);

	bool has_data() const;
};

} //namespace godot