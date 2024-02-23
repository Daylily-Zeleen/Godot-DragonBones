#include "dragonbones_bone.h"

using namespace godot;

void DragonBonesBone::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_name"), &DragonBonesBone::get_name);
	ClassDB::bind_method(D_METHOD("has_data"), &DragonBonesBone::has_data);

	ClassDB::bind_method(D_METHOD("get_bone_position"), &DragonBonesBone::get_bone_position);
	ClassDB::bind_method(D_METHOD("set_bone_position", "new_position"), &DragonBonesBone::set_bone_position);

	ClassDB::bind_method(D_METHOD("get_bone_scale"), &DragonBonesBone::get_bone_scale);
	ClassDB::bind_method(D_METHOD("set_bone_scale", "new_scale"), &DragonBonesBone::set_bone_scale);

	ClassDB::bind_method(D_METHOD("get_bone_rotation"), &DragonBonesBone::get_bone_rotation);
	ClassDB::bind_method(D_METHOD("set_bone_rotation", "deg_in_rad"), &DragonBonesBone::set_bone_rotation);

	ClassDB::bind_method(D_METHOD("set_bone_global_scale", "new_scale"), &DragonBonesBone::set_bone_global_scale);
	ClassDB::bind_method(D_METHOD("get_bone_global_scale"), &DragonBonesBone::get_bone_global_scale);

	ClassDB::bind_method(D_METHOD("set_bone_global_position", "new_position"), &DragonBonesBone::set_bone_global_position);
	ClassDB::bind_method(D_METHOD("get_bone_global_position"), &DragonBonesBone::get_bone_global_position);

	ClassDB::bind_method(D_METHOD("get_bone_global_rotation"), &DragonBonesBone::get_bone_global_rotation);
	ClassDB::bind_method(D_METHOD("set_bone_global_rotation", "deg_in_rad"), &DragonBonesBone::set_bone_global_rotation);

	ClassDB::bind_method(D_METHOD("get_bone_transform"), &DragonBonesBone::get_bone_transform);
	ClassDB::bind_method(D_METHOD("set_bone_transform", "transform"), &DragonBonesBone::set_bone_transform);

	ClassDB::bind_method(D_METHOD("get_bone_global_transform"), &DragonBonesBone::get_bone_global_transform);
	ClassDB::bind_method(D_METHOD("set_bone_global_transform", "global_transform"), &DragonBonesBone::set_bone_global_transform);

	ClassDB::bind_method(D_METHOD("get_bone_offset_position"), &DragonBonesBone::get_bone_offset_position);
	ClassDB::bind_method(D_METHOD("get_bone_animation_position"), &DragonBonesBone::get_bone_animation_position);
	ClassDB::bind_method(D_METHOD("get_bone_origin_position"), &DragonBonesBone::get_bone_origin_position);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rotation"), "set_bone_rotation", "get_bone_rotation");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "global_rotation"), "set_bone_global_rotation", "get_bone_global_rotation");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position"), "set_bone_position", "get_bone_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "global_position"), "set_bone_global_position", "get_bone_global_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "scale"), "set_bone_scale", "get_bone_scale");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "global_scale"), "set_bone_global_scale", "get_bone_global_scale");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "transform"), "set_bone_transform", "get_bone_transform");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "global_transform"), "set_bone_global_transform", "get_bone_global_transform");
}

Transform2D DragonBonesBone::get_bone_transform() const {
	return Transform2D{ get_bone_rotation(), get_bone_position() }.scaled(get_bone_scale());
}

void DragonBonesBone::set_bone_transform(const Transform2D &p_transform) {
	set_bone_scale(p_transform.get_scale());
	set_bone_position(p_transform.get_origin());
	set_bone_rotation(p_transform.get_rotation());
}

Transform2D DragonBonesBone::get_bone_global_transform() const {
	return Transform2D{ get_bone_global_rotation(), get_bone_global_position() }.scaled(get_bone_global_scale());
}
void DragonBonesBone::set_bone_global_transform(const Transform2D &p_global_transform) {
	set_bone_global_scale(p_global_transform.get_scale());
	set_bone_global_position(p_global_transform.get_origin());
	set_bone_global_rotation(p_global_transform.get_rotation());
}

Vector2 DragonBonesBone::get_bone_global_position() const {
	ERR_FAIL_NULL_V(armature, {});
	return get_bone_position() +
			armature->get_global_transform().get_origin() +
			get_bone_origin_position();
}

Vector2 DragonBonesBone::get_bone_global_scale() const {
	ERR_FAIL_NULL_V(armature, {});
	return armature->get_global_transform().get_scale();
}

float DragonBonesBone::get_bone_global_rotation() const {
	ERR_FAIL_NULL_V(boneData, {});
	return boneData->global.rotation;
}

void DragonBonesBone::set_bone_global_position(Vector2 new_pos) {
	ERR_FAIL_NULL(armature);
	set_bone_position(
			new_pos -
			armature->get_global_transform().get_origin() -
			get_bone_origin_position());
}

void DragonBonesBone::set_bone_global_scale(Vector2 scale) {
	ERR_FAIL_NULL(boneData);
	boneData->global.scaleX = scale.x;
	boneData->global.scaleY = scale.y;

	boneData->invalidUpdate();
}

void DragonBonesBone::set_bone_global_rotation(float rotation) {
	ERR_FAIL_NULL(boneData);
	boneData->global.rotation = rotation;
	boneData->update(0);
	boneData->invalidUpdate();
}

Vector2 DragonBonesBone::get_bone_origin_position() const {
	ERR_FAIL_COND_V(!boneData || !armature, {});
	float _x = boneData->origin->x * armature->get_global_transform().get_scale().x;
	float _y = boneData->origin->y * armature->get_global_transform().get_scale().y;

	return Vector2(_x, _y);
}

Vector2 DragonBonesBone::get_bone_animation_position() const {
	ERR_FAIL_NULL_V(boneData, {});
	float _x = boneData->animationPose.x;
	float _y = boneData->animationPose.y;

	return Vector2(_x, _y);
}

Vector2 DragonBonesBone::get_bone_offset_position() const {
	ERR_FAIL_NULL_V(boneData, {});

	float _x = boneData->offset.x;
	float _y = boneData->offset.y;

	return Vector2(_x, _y);
}

String DragonBonesBone::get_name() const {
	ERR_FAIL_NULL_V(boneData, {});
	return String(boneData->getName().c_str());
}

Vector2 DragonBonesBone::get_bone_position() const {
	ERR_FAIL_COND_V(!boneData || !armature, {});
	return Vector2(boneData->animationPose.x * armature->get_global_transform().get_scale().x,
			boneData->animationPose.y * armature->get_global_transform().get_scale().y);
}

void DragonBonesBone::set_bone_position(Vector2 new_pos) {
	ERR_FAIL_COND(!boneData || !armature);
	boneData->animationPose.x = new_pos.x / armature->get_global_transform().get_scale().x;
	boneData->animationPose.y = new_pos.y / armature->get_global_transform().get_scale().y;

	boneData->invalidUpdate();
}

Vector2 DragonBonesBone::get_bone_scale() const {
	ERR_FAIL_NULL_V(boneData, {});
	return Vector2(boneData->animationPose.scaleX, boneData->animationPose.scaleY);
}

void DragonBonesBone::set_bone_scale(Vector2 scale) {
	ERR_FAIL_NULL(boneData);
	boneData->animationPose.scaleX = scale.x;
	boneData->animationPose.scaleY = scale.y;

	boneData->invalidUpdate();
}

float DragonBonesBone::get_bone_rotation() const {
	ERR_FAIL_NULL_V(boneData, {});
	return boneData->animationPose.rotation;
}

void DragonBonesBone::set_bone_rotation(float rotation) {
	ERR_FAIL_NULL(boneData);

	boneData->animationPose.rotation = rotation;
	boneData->update(0);
	boneData->invalidUpdate();
}

bool DragonBonesBone::has_data() const {
	return boneData && armature;
}
