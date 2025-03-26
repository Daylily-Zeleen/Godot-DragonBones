#include "event_object.h"

#include <dragonBones/model/UserData.h>

#include "armature.h"

namespace godot {
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

////////////////////////////////////////////////////
DragonBonesEventObject::DragonBonesEventObject(const dragonBones::EventObject *p_origin) :
		time(p_origin->time),
		type_text(to_gd_str(p_origin->type)),
		name(to_gd_str(p_origin->name)),
		data(p_origin->data ? memnew(DragonBonesUserData(p_origin->data)) : nullptr) {
	ERR_FAIL_NULL(p_origin);

	using EventObject = dragonBones::EventObject;
	if (p_origin->type == EventObject::START) {
		type = TYPE_ANIM_START;
	} else if (p_origin->type == EventObject::LOOP_COMPLETE) {
		type = TYPE_ANIM_LOOP_COMPLETE;
	} else if (p_origin->type == EventObject::COMPLETE) {
		type = TYPE_ANIM_COMPLETE;
	} else if (p_origin->type == EventObject::FADE_IN) {
		type = TYPE_ANIM_FADE_IN;
	} else if (p_origin->type == EventObject::FADE_IN_COMPLETE) {
		type = TYPE_ANIM_FADE_IN_COMPLETE;
	} else if (p_origin->type == EventObject::FADE_OUT) {
		type = TYPE_ANIM_FADE_OUT;
	} else if (p_origin->type == EventObject::FADE_OUT_COMPLETE) {
		type = TYPE_ANIM_FADE_OUT_COMPLETE;
	} else if (p_origin->type == EventObject::FRAME_EVENT) {
		type = TYPE_FRAME_EVENT;
	} else if (p_origin->type == EventObject::SOUND_EVENT) {
		type = TYPE_SOUND_EVENT;
	} else {
		type = TYPE_CUSTOM;
	}

	if (p_origin->armature) {
		armature = static_cast<DragonBonesArmature *>(p_origin->armature->getDisplay());
	}
	if (p_origin->bone) {
		bone = to_gd_str(p_origin->bone->getName());
	}
	if (p_origin->slot) {
		slot = to_gd_str(p_origin->slot->getName());
	}
}

void DragonBonesEventObject::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_time", "time"), &DragonBonesEventObject::set_time);
	ClassDB::bind_method(D_METHOD("get_time"), &DragonBonesEventObject::get_time);

	ClassDB::bind_method(D_METHOD("set_type", "type"), &DragonBonesEventObject::set_type);
	ClassDB::bind_method(D_METHOD("get_type"), &DragonBonesEventObject::get_type);

	ClassDB::bind_method(D_METHOD("set_type_text", "type_text"), &DragonBonesEventObject::set_type_text);
	ClassDB::bind_method(D_METHOD("get_type_text"), &DragonBonesEventObject::get_type_text);

	ClassDB::bind_method(D_METHOD("set_name", "name"), &DragonBonesEventObject::set_name);
	ClassDB::bind_method(D_METHOD("get_name"), &DragonBonesEventObject::get_name);

	ClassDB::bind_method(D_METHOD("set_armature", "armature"), &DragonBonesEventObject::set_armature);
	ClassDB::bind_method(D_METHOD("get_armature"), &DragonBonesEventObject::get_armature);

	ClassDB::bind_method(D_METHOD("set_bone", "bone"), &DragonBonesEventObject::set_bone);
	ClassDB::bind_method(D_METHOD("get_bone"), &DragonBonesEventObject::get_bone);

	ClassDB::bind_method(D_METHOD("set_slot", "slot"), &DragonBonesEventObject::set_slot);
	ClassDB::bind_method(D_METHOD("get_slot"), &DragonBonesEventObject::get_slot);

	ClassDB::bind_method(D_METHOD("set_data", "data"), &DragonBonesEventObject::set_data);
	ClassDB::bind_method(D_METHOD("get_data"), &DragonBonesEventObject::get_data);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "time"), "set_time", "get_time");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "type", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, "DragonBonesEventObject.Type"), "set_type", "get_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "type_text"), "set_type_text", "get_type_text");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name"), "set_name", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "armature", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, DragonBonesArmature::get_class_static()), "set_armature", "get_armature");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "bone"), "set_bone", "get_bone");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "slot"), "set_slot", "get_slot");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "data", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT, DragonBonesUserData::get_class_static()), "set_data", "get_data");

	// 枚举
	BIND_ENUM_CONSTANT(TYPE_ANIM_START);
	BIND_ENUM_CONSTANT(TYPE_ANIM_LOOP_COMPLETE);
	BIND_ENUM_CONSTANT(TYPE_ANIM_COMPLETE);
	BIND_ENUM_CONSTANT(TYPE_ANIM_FADE_IN);
	BIND_ENUM_CONSTANT(TYPE_ANIM_FADE_IN_COMPLETE);
	BIND_ENUM_CONSTANT(TYPE_ANIM_FADE_OUT);
	BIND_ENUM_CONSTANT(TYPE_ANIM_FADE_OUT_COMPLETE);
	BIND_ENUM_CONSTANT(TYPE_FRAME_EVENT);
	BIND_ENUM_CONSTANT(TYPE_SOUND_EVENT);
	BIND_ENUM_CONSTANT(TYPE_CUSTOM);
	BIND_ENUM_CONSTANT(TYPE_MAX);
}

} //namespace godot