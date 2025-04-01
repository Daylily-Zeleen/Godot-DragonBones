#include "armature_display.h"

#include <dragonBones/event/EventObject.h>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/transform2d.hpp>

#include "dragonbones.h"
#include "event_object.h"
#include "mesh_display.h"

using namespace godot;
using namespace dragonBones;

DragonBonesArmature::~DragonBonesArmature() {} // 不需要额外清理

void DragonBonesArmature::_bind_methods() {
	ClassDB::bind_method(D_METHOD("for_each_armature", "action"), &DragonBonesArmature::for_each_armature_);
	ClassDB::bind_method(D_METHOD("for_each_armature_recursively", "action", "current_depth"), &DragonBonesArmature::for_each_armature_recursively_, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("has_animation", "animation_name"), &DragonBonesArmature::has_animation);
	ClassDB::bind_method(D_METHOD("get_animations"), &DragonBonesArmature::get_animations);
	ClassDB::bind_method(D_METHOD("is_playing"), &DragonBonesArmature::is_playing);

	ClassDB::bind_method(D_METHOD("tell_animation", "animation_name"), &DragonBonesArmature::tell_animation);
	ClassDB::bind_method(D_METHOD("seek_animation", "animation_name", "progress"), &DragonBonesArmature::seek_animation);

	ClassDB::bind_method(D_METHOD("play", "animation_name", "loop_count"), &DragonBonesArmature::play, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("play_from_time", "animation_name", "time", "loop_count"), &DragonBonesArmature::play_from_time, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("play_from_progress", "animation_name", "progress", "loop_count"), &DragonBonesArmature::play_from_progress, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("stop", "animation_name", "reset", "recursively"), &DragonBonesArmature::stop, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("stop_all_animations", "reset", "recursively"), &DragonBonesArmature::stop_all_animations, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("fade_in", "animation_name", "time", "loop", "layer", "group", "fade_out_mode"), &DragonBonesArmature::fade_in);

	ClassDB::bind_method(D_METHOD("reset", "recursively"), &DragonBonesArmature::reset, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("has_slot", "slot_name"), &DragonBonesArmature::has_slot);
	ClassDB::bind_method(D_METHOD("get_slot", "slot_name"), &DragonBonesArmature::get_slot);
	ClassDB::bind_method(D_METHOD("get_slots"), &DragonBonesArmature::get_slots);

	ClassDB::bind_method(D_METHOD("get_ik_constraints"), &DragonBonesArmature::get_ik_constraints);
	ClassDB::bind_method(D_METHOD("set_ik_constraint", "constraint_name", "new_position"), &DragonBonesArmature::set_ik_constraint);
	ClassDB::bind_method(D_METHOD("set_ik_constraint_bend_positive", "constraint_name", "bend_positive"), &DragonBonesArmature::set_ik_constraint_bend_positive);

	ClassDB::bind_method(D_METHOD("get_bones"), &DragonBonesArmature::get_bones);
	ClassDB::bind_method(D_METHOD("get_bone", "bone_name"), &DragonBonesArmature::get_bone);

	ClassDB::bind_method(D_METHOD("advance", "delta", "recursively"), &DragonBonesArmature::advance, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_rect"), &DragonBonesArmature::get_rect);

	// Setter Getter
	ClassDB::bind_method(D_METHOD("set_current_animation", "current_animation"), &DragonBonesArmature::set_current_animation);
	ClassDB::bind_method(D_METHOD("get_current_animation"), &DragonBonesArmature::get_current_animation);

	ClassDB::bind_method(D_METHOD("set_animation_progress", "progress"), &DragonBonesArmature::set_animation_progress);
	ClassDB::bind_method(D_METHOD("get_animation_progress"), &DragonBonesArmature::get_animation_progress);

	ClassDB::bind_method(D_METHOD("set_flip_x_", "flip_x"), &DragonBonesArmature::set_flip_x_);
	ClassDB::bind_method(D_METHOD("is_flipped_x"), &DragonBonesArmature::is_flipped_x);

	ClassDB::bind_method(D_METHOD("set_flip_y_", "flip_y"), &DragonBonesArmature::set_flip_y_);
	ClassDB::bind_method(D_METHOD("is_flipped_y"), &DragonBonesArmature::is_flipped_y);

	ClassDB::bind_method(D_METHOD("set_texture_override", "texture_override"), &DragonBonesArmature::set_texture_override);
	ClassDB::bind_method(D_METHOD("get_texture_override"), &DragonBonesArmature::get_texture_override);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "current_animation", PROPERTY_HINT_ENUM, "", PROPERTY_USAGE_EDITOR), "set_current_animation", "get_current_animation");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "animation_progress", PROPERTY_HINT_RANGE, "0.0,1.0,0.0001", PROPERTY_USAGE_EDITOR), "set_animation_progress", "get_animation_progress");

	ADD_GROUP("Flip", "flip_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_x"), "set_flip_x_", "is_flipped_x");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_y"), "set_flip_y_", "is_flipped_y");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture_override", PROPERTY_HINT_RESOURCE_TYPE, Texture2D::get_class_static()), "set_texture_override", "get_texture_override");

	ADD_SIGNAL(MethodInfo("event_dispatched", PropertyInfo(Variant::OBJECT, "event_object", PROPERTY_HINT_NONE, "", PROPERTY_HINT_NONE, DragonBonesEventObject::get_class_static())));

	// Enum
	BIND_ENUM_CONSTANT(FADE_OUT_NONE);
	BIND_ENUM_CONSTANT(FADE_OUT_SAME_LAYER);
	BIND_ENUM_CONSTANT(FADE_OUT_SAME_GROUP);
	BIND_ENUM_CONSTANT(FADE_OUT_SAME_LAYER_AND_GROUP);
	BIND_ENUM_CONSTANT(FADE_OUT_ALL);
	BIND_ENUM_CONSTANT(FADE_OUT_SINGLE);

#ifdef TOOLS_ENABLED
	auto props = ClassDB::class_get_property_list(get_class_static(), true);
	auto tmp_obj = memnew(DragonBonesArmature);
	for (size_t i = 0; i < props.size(); ++i) {
		Dictionary prop = props[i];
		if ((((uint32_t)prop["usage"]) & PROPERTY_USAGE_STORAGE) != 0) {
			storage_properties.emplace_back(StoredProperty{ prop["name"], tmp_obj->get(prop["name"]) });
		}

		DragonBonesArmatureProxy::armature_property_list.emplace_back(PropertyInfo(
				(Variant::Type)((int)prop["type"]), (StringName)prop["name"], (PropertyHint)((int)prop["hint"]),
				(String)prop["hint_string"], (uint64_t)(prop["usage"]), (StringName)prop["class"]));
	}

	storage_properties.emplace_back(StoredProperty{ "current_animation", String() });

	memdelete(tmp_obj);
#endif // TOOLS_ENABLED
}

void DragonBonesArmature::dispatchDBEvent(const std::string &p_type, dragonBones::EventObject *p_value) {
	using namespace dragonBones;
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	ERR_FAIL_NULL(p_value);
	Ref<DragonBonesEventObject> event_object{ memnew(DragonBonesEventObject(p_value)) };
	emit_signal(SNAME("event_dispatched"), event_object);

	if (armature_display) {
		armature_display->dispatch_event(event_object);
	}
}

void DragonBonesArmature::for_each_armature_(const Callable &p_action) {
	for_each_armature([&](auto p_child_armature) {
		return p_action.call(p_child_armature).booleanize();
	});
}

void DragonBonesArmature::queue_redraw() const {
	if (armature_display) {
		armature_display->queue_redraw();
	}
}

void DragonBonesArmature::append_draw_data(VMap<int, LocalVector<DrawData>> &r_data, const Transform2D &p_base_transfrom) const {
	if (slot && !slot->getVisible()) {
		return;
	}

	const Transform2D global_transform = p_base_transfrom * transform;
	for (const Slot *raw_slot : armature_instance->getSlots()) {
		const Slot_GD *slot = static_cast<const Slot_GD *>(raw_slot);
		if (auto display = slot->get_display()) {
			display->append_draw_data(r_data, global_transform);
		}
	}
}

void DragonBonesArmature::for_each_armature_recursively_(const Callable &p_action, int p_current_depth) {
	for_each_armature_recursively(
			[&p_action](auto p_child_armature, auto depth) {
				return p_action.call(p_child_armature, depth).booleanize();
			},
			p_current_depth);
}

bool DragonBonesArmature::has_animation(const String &p_animation_name) const {
	if (armature_instance == nullptr || !getAnimation()) {
		return false;
	}

	return getArmature()->getArmatureData()->getAnimation(to_std_str(p_animation_name)) != nullptr;
}

PackedStringArray DragonBonesArmature::get_animations() {
	PackedStringArray animations{};

	const ArmatureData *data = armature_instance->getArmatureData();

	for (std::string animation_name : data->getAnimationNames()) {
		animations.push_back(to_gd_str(animation_name));
	}

	return animations;
}

Rect2 DragonBonesArmature::get_rect() const {
	const auto &aabb = armature_instance->getArmatureData()->aabb;
	return {
		aabb.x,
		aabb.y,
		aabb.width,
		aabb.height,
	};
}

void DragonBonesArmature::advance(float p_delta, bool p_recursively) {
	if (armature_instance) {
		armature_instance->advanceTime(p_delta);
		DragonBones::get_singleton()->flush();
	}

	if (p_recursively) {
		for_each_armature([p_delta](DragonBonesArmature *p_child_armature) {
			p_child_armature->advance(p_delta, true);
		});
	}
}

void DragonBonesArmature::set_current_animation(const String &p_animation) {
	if (p_animation == "[none]" || p_animation.is_empty()) {
		stop(get_current_animation());
	} else if (!is_playing()) {
		play(p_animation, armature_display->get_animation_loop_count());
	} else if (get_current_animation() != p_animation) {
		play(p_animation, armature_display->get_animation_loop_count());
	} else {
		// 相同动画，无需响应
	}
}

String DragonBonesArmature::get_current_animation() const {
	if (!armature_instance || !getAnimation() || !is_playing()) {
		return "[none]";
	}
	return to_gd_str(getAnimation()->getLastAnimationName());
}

String DragonBonesArmature::get_current_animation_on_layer(int p_layer) const {
	if (!getAnimation()) {
		return {};
	}

	std::vector<AnimationState *> states = armature_instance->getAnimation()->getStates();

	for (AnimationState *state : states) {
		if (state->layer == p_layer) {
			return to_gd_str(state->getName());
		}
	}

	return {};
}

String DragonBonesArmature::get_current_animation_in_group(const String &p_group_name) const {
	if (!getAnimation()) {
		return {};
	}
	std::vector<AnimationState *> states = getAnimation()->getStates();

	for (AnimationState *state : states) {
		if (state->group == p_group_name.utf8().get_data()) {
			return to_gd_str(state->getName());
		}
	}

	return {};
}

float DragonBonesArmature::tell_animation(const String &p_animation_name) const {
	if (has_animation(p_animation_name)) {
		AnimationState *animation_state = getAnimation()->getState(to_std_str(p_animation_name));
		if (animation_state) {
			return animation_state->getCurrentTime() / animation_state->getTotalTime();
		}
	}
	return 0.0f;
}

void DragonBonesArmature::seek_animation(const String &p_animation_name, float p_progress) {
	if (has_animation(p_animation_name)) {
		stop(p_animation_name, true);
		auto current_progress = Math::fmod(p_progress, 1.0f);
		if (current_progress == 0 && p_progress != 0) {
			current_progress = 1.0f;
		}
		armature_instance->getAnimation()->gotoAndStopByProgress(to_std_str(p_animation_name), current_progress < 0 ? 1. + current_progress : current_progress);
	}
}

bool DragonBonesArmature::is_playing() const {
	return getAnimation()->isPlaying();
}

void DragonBonesArmature::play(const String &p_animation_name, int p_loop_count) {
	if (has_animation(p_animation_name)) {
		getAnimation()->play(to_std_str(p_animation_name), p_loop_count);
	}
	// TODO: 是否需要在没有动画时停止一切动画
}

void DragonBonesArmature::play_from_time(const String &p_animation_name, float p_time, int p_loop_count) {
	if (has_animation(p_animation_name)) {
		play(p_animation_name, p_loop_count);
		getAnimation()->gotoAndPlayByTime(to_std_str(p_animation_name), p_time);
	}
}

void DragonBonesArmature::play_from_progress(const String &p_animation_name, float p_progress, int p_loop_count) {
	if (has_animation(p_animation_name)) {
		play(p_animation_name, p_loop_count);
		getAnimation()->gotoAndPlayByProgress(to_std_str(p_animation_name), p_progress);
	}
}

void DragonBonesArmature::stop(const String &p_animation_name, bool b_reset, bool p_recursively) {
	if (getAnimation()) {
		getAnimation()->stop(to_std_str(p_animation_name));

		if (b_reset) {
			reset();
		}
	}

	if (p_recursively) {
		for_each_armature([&p_animation_name, b_reset](DragonBonesArmature *p_child_armature) {
			p_child_armature->stop(p_animation_name, b_reset, true);
		});
	}
}

void DragonBonesArmature::stop_all_animations(bool b_reset, bool p_recursively) {
	if (getAnimation()) {
		getAnimation()->stop("");
	}

	if (b_reset) {
		reset();
	}

	if (p_recursively) {
		for_each_armature([b_reset](DragonBonesArmature *p_child_armature) {
			p_child_armature->stop_all_animations(b_reset, true);
		});
	}
}

void DragonBonesArmature::fade_in(const String &p_animation_name, float p_time, int p_loop_count, int p_layer, const String &p_group, AnimFadeOutMode p_fade_out_mode) {
	if (has_animation(p_animation_name)) {
		getAnimation()->fadeIn(to_std_str(p_animation_name), p_time, p_loop_count, p_layer, to_std_str(p_group), (AnimationFadeOutMode)p_fade_out_mode);
	}
}

void DragonBonesArmature::reset(bool p_recursively) {
	if (getAnimation()) {
		getAnimation()->reset();
	}

	if (p_recursively) {
		for_each_armature([](DragonBonesArmature *p_child_armature) {
			p_child_armature->reset(true);
		});
	}
}

bool DragonBonesArmature::has_slot(const String &p_slot_name) const {
	return getArmature()->getSlot(to_std_str(p_slot_name)) != nullptr;
}

SlotsDictionary DragonBonesArmature::get_slots() {
	SlotsDictionary ret{};

	for (auto &slot : slots) {
		ret[to_gd_str(slot.first)] = slot.second;
	}

	return ret;
}

Ref<DragonBonesSlot> DragonBonesArmature::get_slot(const String &p_slot_name) {
	auto it = slots.find(to_std_str(p_slot_name));
	return it == slots.end() ? Ref<DragonBonesSlot>{} : it->second;
}

void DragonBonesArmature::set_flip_x(bool p_flip_x, bool p_recursively) {
	getArmature()->setFlipX(p_flip_x);
	getArmature()->advanceTime(0);
	if (p_recursively) {
		for_each_armature([p_flip_x](DragonBonesArmature *p_child_armature) {
			p_child_armature->set_flip_x(p_flip_x, true);
		});
	}
}

bool DragonBonesArmature::is_flipped_x() const {
	if (!armature_instance) {
		return false;
	}
	return getArmature()->getFlipX();
}

void DragonBonesArmature::set_flip_y(bool p_flip_y, bool p_recursively) {
	getArmature()->setFlipY(p_flip_y);
	getArmature()->advanceTime(0);
	if (p_recursively) {
		for_each_armature([p_flip_y](DragonBonesArmature *p_child_armature) {
			p_child_armature->set_flip_y(p_flip_y, true);
		});
	}
}

bool DragonBonesArmature::is_flipped_y() const {
	if (!armature_instance) {
		return false;
	}
	return getArmature()->getFlipY();
}

Ref<Texture2D> DragonBonesArmature::get_texture_override() const {
	return texture_override;
}

void DragonBonesArmature::set_texture_override(const Ref<Texture2D> &p_texture_override) {
	texture_override = p_texture_override;
}

ConstraintsDictionary DragonBonesArmature::get_ik_constraints() {
	ConstraintsDictionary dict;

	for (auto &constraint : getArmature()->getArmatureData()->constraints) {
		dict[to_gd_str(constraint.first)] = Vector2(constraint.second->target->transform.x, constraint.second->target->transform.y);
	}

	return dict;
}

void DragonBonesArmature::set_ik_constraint(const String &p_name, Vector2 p_position) {
	for (dragonBones::Constraint *constraint : getArmature()->_constraints) {
		if (constraint->getName() == p_name.utf8().get_data()) {
			dragonBones::BoneData *target = const_cast<BoneData *>(constraint->_constraintData->target);
			target->transform.x = p_position.x;
			target->transform.y = p_position.y;

			constraint->_constraintData->setTarget(target);
			constraint->update();
			getArmature()->invalidUpdate(target->name, true);
		}
	}
}

void DragonBonesArmature::set_ik_constraint_bend_positive(const String &name, bool p_bend_positive) {
	for (dragonBones::Constraint *constraint : getArmature()->_constraints) {
		if (constraint->getName() == name.utf8().get_data()) {
			dragonBones::BoneData *target = const_cast<BoneData *>(constraint->_constraintData->target);

			static_cast<IKConstraint *>(constraint)->_bendPositive = p_bend_positive;
			constraint->update();
			getArmature()->invalidUpdate(target->name, true);
		}
	}
}

BonesDictionary DragonBonesArmature::get_bones() {
	BonesDictionary ret{};

	for (auto &bone : bones) {
		ret[to_gd_str(bone.first)] = bone.second;
	}

	return ret;
}

Ref<DragonBonesBone> DragonBonesArmature::get_bone(const String &p_name) {
	auto it = bones.find(to_std_str(p_name));
	return it == bones.end() ? Ref<DragonBonesBone>{} : it->second;
}

void DragonBonesArmature::add_bone(std::string p_name, const Ref<DragonBonesBone> &p_new_bone) {
	bones.insert(std::make_pair(p_name, p_new_bone));
}

void DragonBonesArmature::add_slot(std::string p_name, const Ref<DragonBonesSlot> &p_new_slot) {
	slots.insert(std::make_pair(p_name, p_new_slot));
}

void DragonBonesArmature::dbInit(Armature *p_armature) {
	armature_instance = p_armature;
}

void DragonBonesArmature::dbClear() {
	armature_instance = nullptr;
	// 不能回池重复利用，因为要直接暴露给用户，用户可以直接比较指针导致非预期情形。
	memdelete(this);
}

void DragonBonesArmature::dbUpdate() {
}

void DragonBonesArmature::release() {
	Display::release();

	bones.clear();
	slots.clear();

	slot = nullptr;
	texture_override.unref();

	if (armature_instance) {
		armature_instance->dispose();
		armature_instance = nullptr;
		// 通过 dbClear() 回调推迟销毁释放
	} else {
		dbClear();
	}

	if (armature_display) {
		armature_display = nullptr;
	}
}

void DragonBonesArmature::force_update() {
	if (!armature_instance) {
		return;
	}

	for (Slot *slot : armature_instance->getSlots()) {
		if (!slot) {
			continue;
		}

		slot->invalidUpdate();
		slot->update(0);
	}

	advance(0.0);
}

//
void DragonBonesArmature::set_animation_progress(float p_progress) {
	seek_animation(get_current_animation(), p_progress);
}

float DragonBonesArmature::get_animation_progress() const {
	return tell_animation(get_current_animation());
}

#ifdef TOOLS_ENABLED
std::vector<DragonBonesArmature::StoredProperty> DragonBonesArmature::storage_properties{};

bool DragonBonesArmature::_set(const StringName &p_name, const Variant &p_val) {
	if (p_name == SNAME("sub_armatures")) {
		return true;
	}
	return false;
}

bool DragonBonesArmature::_get(const StringName &p_name, Variant &r_val) const {
	if (p_name == SNAME("sub_armatures")) {
		TypedArray<DragonBonesArmatureProxy> ret;

		for (auto it : slots) {
			if (it.second.is_null()) {
				continue;
			}

			auto sub_armature = it.second->get_child_armature();
			if (!sub_armature) {
				continue;
			}

			Ref<DragonBonesArmatureProxy> proxy{ memnew(DragonBonesArmatureProxy(sub_armature)) };
			ret.push_back(proxy);
		}

		r_val = ret;
		return true;
	}
	return false;
}

void DragonBonesArmature::_get_property_list(List<PropertyInfo> *p_list) const {
	// for (auto it : slots) {
	// 	if (it.second.is_null()) {
	// 		continue;
	// 	}

	// 	if (it.second->get_child_armature()) {
	// 		p_list->push_back(PropertyInfo(Variant::ARRAY, SNAME("sub_armatures"),
	// 				PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, DragonBonesArmatureProxy::get_class_static()),
	// 				PROPERTY_USAGE_EDITOR));
	// 		return;
	// 	}
	// }
}

bool DragonBonesArmature::has_sub_armature() const {
	for (auto it : slots) {
		if (it.second.is_null()) {
			continue;
		}

		if (it.second->get_child_armature()) {
			return true;
		}
	}
	return false;
}

#endif // TOOLS_ENABLED

void DragonBonesArmature::set_settings(const Dictionary &p_settings) {
	auto keys = p_settings.keys();
	auto values = p_settings.values();
	for (size_t i = 0; i < keys.size(); ++i) {
		const String key = keys[i];
		if (key != "sub_armatures") {
			set(key, values[i]);
		} else {
			Dictionary sub_armatures_setting = values[i];
			auto slot_names = sub_armatures_setting.keys();
			auto slot_settings = sub_armatures_setting.values();

			for (size_t j = 0; j < slot_names.size(); ++i) {
				const String &slot_name = slot_names[i];
				const Dictionary &armature_settings = slot_settings[i];
				auto it = slots.find(to_std_str(slot_name));
				if (it == slots.end()) {
					continue;
				}
				ERR_CONTINUE(it->second.is_null());

				auto child_armature = it->second->get_child_armature();
				if (child_armature) {
					child_armature->set_settings(armature_settings);
				}
			}
		}
	}
}

#ifdef TOOLS_ENABLED
Dictionary DragonBonesArmature::get_settings() const {
	Dictionary ret;
	for (const auto &prop_info : storage_properties) {
		Variant val = get(prop_info.name);
		if (val != prop_info.default_value) {
			ret[prop_info.name] = val;
		}
	}

	Dictionary sub_armatures_setting;
	ret["sub_armatures"] = sub_armatures_setting;

	for (auto kv : slots) {
		const auto &slot_name = kv.first;
		const auto &slot = kv.second;
		if (slot.is_null()) {
			continue;
		}
		auto sub_armature = slot->get_child_armature();
		if (sub_armature) {
			sub_armatures_setting[to_gd_str(slot_name)] = sub_armature->get_settings();
		}
	}

	return ret;
}
#endif //  TOOLS_ENABLED
////////////
#ifdef TOOLS_ENABLED
std::vector<PropertyInfo> DragonBonesArmatureProxy::armature_property_list{};

bool DragonBonesArmatureProxy::_set(const StringName &p_name, const Variant &p_val) {
	if (!armature || !armature->is_valid()) {
		return false;
	}

	if (p_name == SNAME("armature_name")) {
		return true;
	}

	for (const auto &prop_info : armature_property_list) {
		if (prop_info.name == p_name) {
			armature->set(p_name, p_val);
			if (prop_info.name.ends_with("modulate")) {
				armature->queue_redraw();
			} else {
				notify_property_list_changed();
			}
			return true;
		}
	}

	return false;
}

bool DragonBonesArmatureProxy::_get(const StringName &p_name, Variant &r_val) const {
	if (!armature || !armature->is_valid()) {
		return false;
	}

	if (p_name == SNAME("armature_name")) {
		r_val = to_gd_str(static_cast<DragonBonesArmature *>(armature)->getArmature()->getName());
		return true;
	}

	for (const auto &prop_info : armature_property_list) {
		if (prop_info.name == p_name) {
			r_val = armature->get(p_name);
			return true;
		}
	}

	return false;
}

void DragonBonesArmatureProxy::_get_property_list(List<PropertyInfo> *p_list) const {
	if (!armature || !armature->is_valid()) {
		return;
	}

	for (const auto &p : armature_property_list) {
		if (p.name == SNAME("current_animation") && armature->getArmature()) {
			PropertyInfo info = p;
			String hint = "[none]";
			for (const auto &anim : armature->getArmature()->getArmatureData()->getAnimationNames()) {
				hint += ",";
				hint += to_gd_str(anim);
			}
			info.hint_string = hint;
			p_list->push_back(info);
		} else {
			p_list->push_back(p);
		}
	}

	if (armature->has_sub_armature()) {
		p_list->push_back(PropertyInfo(Variant::ARRAY, SNAME("sub_armatures"),
				PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, DragonBonesArmatureProxy::get_class_static()),
				PROPERTY_USAGE_EDITOR));
	}
}

#endif // TOOLS_ENABLED