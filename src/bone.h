/**************************************************************************/
/*  bone.h                                                                */
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

#pragma once

#include <godot_dragon_bones.h>

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

public:
	static void _bind_methods();
	String _to_string() const { return vformat("<%s#%s>", get_class_static(), get_instance_id()); }

	bool is_valid() const;
	String get_name() const;
	Ref<DragonBonesBone> get_parent() const;

	// Local
	Vector2 get_position() const;
	void set_position(Vector2 p_new_pos);

	float get_rotation() const;
	void set_rotation(float p_rotation);

	Vector2 get_scale() const;
	void set_scale(Vector2 p_scale);

	Transform2D get_transform() const;
	void set_transform(const Transform2D &p_transform);

	// Global
	void set_global_position(Vector2 p_new_pos);
	Vector2 get_global_position() const;

	void set_global_rotation(float p_rotation);
	float get_global_rotation() const;

	void set_global_scale(Vector2 p_scale);
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