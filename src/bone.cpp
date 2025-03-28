#include <armature.h>
#include <bone.h>

using namespace godot;

_FORCE_INLINE_ Transform2D to_gd_transform(const dragonBones::Transform &p_t) {
	return {
		p_t.rotation,
		Vector2(p_t.scaleX, p_t.scaleY),
		p_t.skew,
		Vector2(p_t.x, p_t.y)
	};
}

_FORCE_INLINE_ dragonBones::Transform to_db_transform(const Transform2D &p_t) {
	dragonBones::Transform ret;
	ret.x = p_t.get_origin().x;
	ret.y = p_t.get_origin().y;
	ret.rotation = p_t.get_rotation();
	ret.scaleX = p_t.get_scale().x;
	ret.scaleY = p_t.get_scale().y;
	ret.skew = p_t.get_skew();
	return ret;
}

void DragonBonesBone::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_name"), &DragonBonesBone::get_name);
	ClassDB::bind_method(D_METHOD("get_parent"), &DragonBonesBone::get_parent);
	ClassDB::bind_method(D_METHOD("is_valid"), &DragonBonesBone::is_valid);

	ClassDB::bind_method(D_METHOD("get_position"), &DragonBonesBone::get_position);
	ClassDB::bind_method(D_METHOD("set_position", "new_position"), &DragonBonesBone::set_position);

	ClassDB::bind_method(D_METHOD("get_scale"), &DragonBonesBone::get_scale);
	ClassDB::bind_method(D_METHOD("set_scale", "new_scale"), &DragonBonesBone::set_scale);

	ClassDB::bind_method(D_METHOD("get_rotation"), &DragonBonesBone::get_rotation);
	ClassDB::bind_method(D_METHOD("set_rotation", "deg_in_rad"), &DragonBonesBone::set_rotation);

	ClassDB::bind_method(D_METHOD("set_global_scale", "new_scale"), &DragonBonesBone::set_global_scale);
	ClassDB::bind_method(D_METHOD("get_global_scale"), &DragonBonesBone::get_global_scale);

	ClassDB::bind_method(D_METHOD("set_global_position", "new_position"), &DragonBonesBone::set_global_position);
	ClassDB::bind_method(D_METHOD("get_global_position"), &DragonBonesBone::get_global_position);

	ClassDB::bind_method(D_METHOD("get_global_rotation"), &DragonBonesBone::get_global_rotation);
	ClassDB::bind_method(D_METHOD("set_global_rotation", "deg_in_rad"), &DragonBonesBone::set_global_rotation);

	ClassDB::bind_method(D_METHOD("get_transform"), &DragonBonesBone::get_transform);
	ClassDB::bind_method(D_METHOD("set_transform", "transform"), &DragonBonesBone::set_transform);

	ClassDB::bind_method(D_METHOD("get_global_transform"), &DragonBonesBone::get_global_transform);
	ClassDB::bind_method(D_METHOD("set_global_transform", "global_transform"), &DragonBonesBone::set_global_transform);

	ClassDB::bind_method(D_METHOD("get_offset_mode"), &DragonBonesBone::get_offset_mode);
	ClassDB::bind_method(D_METHOD("get_offset"), &DragonBonesBone::get_offset);
	ClassDB::bind_method(D_METHOD("get_animation_pose"), &DragonBonesBone::get_animation_pose);
	ClassDB::bind_method(D_METHOD("get_origin"), &DragonBonesBone::get_origin);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rotation"), "set_rotation", "get_rotation");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "global_rotation"), "set_global_rotation", "get_global_rotation");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position"), "set_position", "get_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "global_position"), "set_global_position", "get_global_position");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "scale"), "set_scale", "get_scale");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "global_scale"), "set_global_scale", "get_global_scale");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "transform"), "set_transform", "get_transform");
	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM2D, "global_transform"), "set_global_transform", "get_global_transform");

	// 枚举
	BIND_ENUM_CONSTANT(OFFSET_MODE_NONE);
	BIND_ENUM_CONSTANT(OFFSET_MODE_ADDITIVE);
	BIND_ENUM_CONSTANT(OFFSET_MODE_OVERRIDE);
}

bool DragonBonesBone::is_valid() const {
	return boneData && armature;
}

String DragonBonesBone::get_name() const {
	ERR_FAIL_NULL_V(boneData, {});
	return String::utf8(boneData->getName().c_str());
}

Ref<DragonBonesBone> DragonBonesBone::get_parent() const {
	ERR_FAIL_COND_V(!is_valid(), {});
	if (boneData->getParent() == nullptr) {
		return {};
	}
	return armature->get_bone(to_gd_str(boneData->getParent()->getName()));
}

// Local
Vector2 DragonBonesBone::get_position() const {
	return get_transform().get_origin();
}

void DragonBonesBone::set_position(Vector2 p_new_pos) {
	auto transform = get_transform();
	transform.set_origin(p_new_pos);
	set_transform(transform);
}

float DragonBonesBone::get_rotation() const {
	return get_transform().get_rotation();
}

void DragonBonesBone::set_rotation(float p_rotation) {
	auto transform = get_transform();
	transform.set_rotation(p_rotation);
	set_transform(transform);
}

Vector2 DragonBonesBone::get_scale() const {
	return get_transform().get_scale();
}

void DragonBonesBone::set_scale(Vector2 p_scale) {
	auto transform = get_transform();
	transform.set_scale(p_scale);
	set_transform(transform);
}

Transform2D DragonBonesBone::get_transform() const {
	ERR_FAIL_NULL_V(boneData, {});
	auto transform = to_gd_transform(boneData->global);
	if (boneData->getParent()) {
		auto parent_transform = boneData->getParent()->global;

		return to_gd_transform(parent_transform).inverse() * transform;
	}

	return transform;
}

void DragonBonesBone::set_transform(const Transform2D &p_transform) {
	ERR_FAIL_NULL(boneData);
	dragonBones::Matrix global;
	to_db_transform(p_transform).toMatrix(global);

	if (boneData->getParent()) {
		dragonBones::Matrix parent_matrix;
		boneData->getParent()->global.toMatrix(parent_matrix);

		parent_matrix.concat(global);

		boneData->global.fromMatrix(parent_matrix);
	} else {
		boneData->global.fromMatrix(global);
	}
	boneData->global.toMatrix(boneData->globalTransformMatrix);

	boneData->invalidUpdate();
}

// Global
Vector2 DragonBonesBone::get_global_position() const {
	return get_global_transform().get_origin();
}

void DragonBonesBone::set_global_position(Vector2 p_new_pos) {
	auto gt = get_global_transform();
	gt.set_origin(p_new_pos);
	set_global_transform(gt);
}

float DragonBonesBone::get_global_rotation() const {
	return get_global_transform().get_rotation();
}

void DragonBonesBone::set_global_rotation(float p_rotation) {
	auto gt = get_global_transform();
	gt.set_rotation(p_rotation);
	set_global_transform(gt);
}

Vector2 DragonBonesBone::get_global_scale() const {
	return get_global_transform().get_scale();
}

void DragonBonesBone::set_global_scale(Vector2 p_scale) {
	auto gt = get_global_transform();
	gt.set_scale(p_scale);
	set_global_transform(gt);
}

Transform2D DragonBonesBone::get_global_transform() const {
	ERR_FAIL_NULL_V(boneData, {});
	return to_gd_transform(boneData->global);
}

void DragonBonesBone::set_global_transform(const Transform2D &p_global_transform) {
	ERR_FAIL_NULL(boneData);
	boneData->global = to_db_transform(p_global_transform);
	boneData->global.toMatrix(boneData->globalTransformMatrix);
}

// Others
DragonBonesBone::OffsetMode DragonBonesBone::get_offset_mode() const {
	ERR_FAIL_NULL_V(boneData, {});
	return static_cast<OffsetMode>(boneData->offsetMode);
}

Transform2D DragonBonesBone::get_offset() const {
	ERR_FAIL_NULL_V(boneData, {});
	return to_gd_transform(boneData->offset);
}

Transform2D DragonBonesBone::get_animation_pose() const {
	ERR_FAIL_NULL_V(boneData, {});
	return to_gd_transform(boneData->animationPose);
}

Transform2D DragonBonesBone::get_origin() const {
	ERR_FAIL_NULL_V(boneData, {});
	return to_gd_transform(*boneData->origin);
}
