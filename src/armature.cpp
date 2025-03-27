#include "armature.h"

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

DragonBonesArmature::~DragonBonesArmature() { _onClear(); }

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

	ClassDB::bind_method(D_METHOD("set_slot_display_index", "slot_name", "index"), &DragonBonesArmature::set_slot_display_index);
	ClassDB::bind_method(D_METHOD("set_slot_by_item_name", "slot_name", "item_name"), &DragonBonesArmature::set_slot_by_item_name);
	ClassDB::bind_method(D_METHOD("set_all_slots_by_item_name", "item_name"), &DragonBonesArmature::set_all_slots_by_item_name);
	ClassDB::bind_method(D_METHOD("get_slot_display_index", "slot_name"), &DragonBonesArmature::get_slot_display_index);
	ClassDB::bind_method(D_METHOD("get_total_items_in_slot", "slot_name"), &DragonBonesArmature::get_total_items_in_slot);
	ClassDB::bind_method(D_METHOD("cycle_next_item_in_slot", "slot_name"), &DragonBonesArmature::cycle_next_item_in_slot);
	ClassDB::bind_method(D_METHOD("cycle_previous_item_in_slot", "slot_name"), &DragonBonesArmature::cycle_previous_item_in_slot);
	ClassDB::bind_method(D_METHOD("get_slot_display_color_multiplier", "slot_name"), &DragonBonesArmature::get_slot_display_color_multiplier);
	ClassDB::bind_method(D_METHOD("set_slot_display_color_multiplier", "slot_name", "color"), &DragonBonesArmature::set_slot_display_color_multiplier);

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
	emit_signal(SNAME("event_dispatched"), Ref<DragonBonesEventObject>(memnew(DragonBonesEventObject(p_value))));
}

void DragonBonesArmature::for_each_armature_(const Callable &p_action) {
	for_each_armature([&](auto p_child_armature) {
		return p_action.call(p_child_armature).booleanize();
	});
}

void DragonBonesArmature::queue_redraw() const {
	if (dragon_bones) {
		dragon_bones->queue_redraw();
	}
}

void DragonBonesArmature::append_draw_data(VMap<int, LocalVector<DrawData>> &r_data, const Transform2D &p_base_transfrom) const {
	if (slot && !slot->getVisible()) {
		return;
	}

	const Transform2D global_transform = p_base_transfrom * transform;
	for (const Slot *raw_slot : p_armature->getSlots()) {
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

bool DragonBonesArmature::has_animation(const String &_animation_name) const {
	if (p_armature == nullptr || !getAnimation()) {
		return false;
	}

	return getArmature()->getArmatureData()->getAnimation(to_std_str(_animation_name)) != nullptr;
}

PackedStringArray DragonBonesArmature::get_animations() {
	PackedStringArray animations{};

	const ArmatureData *data = p_armature->getArmatureData();

	for (std::string animation_name : data->getAnimationNames()) {
		animations.push_back(to_gd_str(animation_name));
	}

	return animations;
}

Rect2 DragonBonesArmature::get_rect() const {
	const auto &aabb = p_armature->getArmatureData()->aabb;
	return {
		aabb.x,
		aabb.y,
		aabb.width,
		aabb.height,
	};
}

void DragonBonesArmature::advance(float p_delta, bool p_recursively) {
	if (p_armature) {
		p_armature->advanceTime(p_delta);
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
		play(p_animation, dragon_bones->get_animation_loop());
	} else if (get_current_animation() != p_animation) {
		play(p_animation, dragon_bones->get_animation_loop());
	} else {
		// 相同动画，无需响应
	}
}

String DragonBonesArmature::get_current_animation() const {
	if (!p_armature || !getAnimation() || !is_playing()) {
		return "[none]";
	}
	return to_gd_str(getAnimation()->getLastAnimationName());
}

String DragonBonesArmature::get_current_animation_on_layer(int _layer) const {
	if (!getAnimation()) {
		return {};
	}

	std::vector<AnimationState *> states = p_armature->getAnimation()->getStates();

	for (AnimationState *state : states) {
		if (state->layer == _layer) {
			return to_gd_str(state->getName());
		}
	}

	return {};
}

String DragonBonesArmature::get_current_animation_in_group(const String &_group_name) const {
	if (!getAnimation()) {
		return {};
	}
	std::vector<AnimationState *> states = getAnimation()->getStates();

	for (AnimationState *state : states) {
		if (state->group == _group_name.utf8().get_data()) {
			return to_gd_str(state->getName());
		}
	}

	return {};
}

float DragonBonesArmature::tell_animation(const String &_animation_name) const {
	if (has_animation(_animation_name)) {
		AnimationState *animation_state = getAnimation()->getState(to_std_str(_animation_name));
		if (animation_state) {
			return animation_state->getCurrentTime() / animation_state->getTotalTime();
		}
	}
	return 0.0f;
}

void DragonBonesArmature::seek_animation(const String &_animation_name, float progress) {
	if (has_animation(_animation_name)) {
		stop(_animation_name, true);
		auto current_progress = Math::fmod(progress, 1.0f);
		if (current_progress == 0 && progress != 0) {
			current_progress = 1.0f;
		}
		p_armature->getAnimation()->gotoAndStopByProgress(to_std_str(_animation_name), current_progress < 0 ? 1. + current_progress : current_progress);
	}
}

bool DragonBonesArmature::is_playing() const {
	return getAnimation()->isPlaying();
}

void DragonBonesArmature::play(const String &_animation_name, int loop) {
	if (has_animation(_animation_name)) {
		getAnimation()->play(to_std_str(_animation_name), loop);
	}
	// TODO: 是否需要在没有动画时停止一切动画
}

void DragonBonesArmature::play_from_time(const String &_animation_name, float _f_time, int loop) {
	if (has_animation(_animation_name)) {
		play(_animation_name, loop);
		getAnimation()->gotoAndPlayByTime(to_std_str(_animation_name), _f_time);
	}
}

void DragonBonesArmature::play_from_progress(const String &_animation_name, float f_progress, int loop) {
	if (has_animation(_animation_name)) {
		play(_animation_name, loop);
		getAnimation()->gotoAndPlayByProgress(to_std_str(_animation_name), f_progress);
	}
}

void DragonBonesArmature::stop(const String &_animation_name, bool b_reset, bool p_recursively) {
	if (getAnimation()) {
		getAnimation()->stop(to_std_str(_animation_name));

		if (b_reset) {
			reset();
		}
	}

	if (p_recursively) {
		for_each_armature([&_animation_name, b_reset](DragonBonesArmature *p_child_armature) {
			p_child_armature->stop(_animation_name, b_reset, true);
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

void DragonBonesArmature::fade_in(const String &_animation_name, float _time, int _loop, int _layer, const String &_group, AnimFadeOutMode _fade_out_mode) {
	if (has_animation(_animation_name)) {
		getAnimation()->fadeIn(to_std_str(_animation_name), _time, _loop, _layer, to_std_str(_group), (AnimationFadeOutMode)_fade_out_mode);
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

bool DragonBonesArmature::has_slot(const String &_slot_name) const {
	return getArmature()->getSlot(to_std_str(_slot_name)) != nullptr;
}

SlotsDictionary DragonBonesArmature::get_slots() {
	SlotsDictionary slots{};

	for (auto &slot : _slots) {
		slots[to_gd_str(slot.first)] = slot.second;
	}

	return slots;
}

Ref<DragonBonesSlot> DragonBonesArmature::get_slot(const String &_slot_name) {
	auto it = _slots.find(to_std_str(_slot_name));
	return it == _slots.end() ? Ref<DragonBonesSlot>{} : it->second;
}

void DragonBonesArmature::set_slot_display_index(const String &_slot_name, int _index) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return;
	}

	getSlot(_slot_name)->setDisplayIndex(_index);
}

void DragonBonesArmature::set_slot_by_item_name(const String &_slot_name, const String &_item_name) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return;
	}

	const std::vector<DisplayData *> *rawData = getSlot(_slot_name)->getRawDisplayDatas();

	// we only want to update the slot if there's a choice
	if (rawData->size() > 1) {
		const char *desired_item = _item_name.utf8().get_data();
		std::string NONE_STRING("none");

		if (NONE_STRING.compare(desired_item) == 0) {
			getSlot(_slot_name)->setDisplayIndex(-1);
		}

		for (int i = 0; i < rawData->size(); i++) {
			DisplayData *display_data = rawData->at(i);

			if (display_data->name.compare(desired_item) == 0) {
				getSlot(_slot_name)->setDisplayIndex(i);
				return;
			}
		}
	} else {
		WARN_PRINT("Slot " + _slot_name + " has only 1 item; refusing to set slot");
		return;
	}

	WARN_PRINT("Slot " + _slot_name + " has no item called \"" + _item_name);
}

void DragonBonesArmature::set_all_slots_by_item_name(const String &_item_name) {
	for (Slot *slot : getArmature()->getSlots()) {
		set_slot_by_item_name(to_gd_str(slot->getName()), _item_name);
	}
}

int DragonBonesArmature::get_slot_display_index(const String &_slot_name) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return -1;
	}
	return p_armature->getSlot(to_std_str(_slot_name))->getDisplayIndex();
}

int DragonBonesArmature::get_total_items_in_slot(const String &_slot_name) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return -1;
	}
	return p_armature->getSlot(to_std_str(_slot_name))->getDisplayList().size();
}

void DragonBonesArmature::cycle_next_item_in_slot(const String &_slot_name) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return;
	}

	int current_slot = get_slot_display_index(_slot_name);
	current_slot++;

	set_slot_display_index(_slot_name, current_slot < get_total_items_in_slot(_slot_name) ? current_slot : -1);
}

void DragonBonesArmature::cycle_previous_item_in_slot(const String &_slot_name) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return;
	}

	int current_slot = get_slot_display_index(_slot_name);
	current_slot--;

	set_slot_display_index(_slot_name, current_slot >= -1 ? current_slot : get_total_items_in_slot(_slot_name) - 1);
}

Color DragonBonesArmature::get_slot_display_color_multiplier(const String &_slot_name) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return Color(-1, -1, -1, -1);
	}
	ColorTransform transform(p_armature->getSlot(to_std_str(_slot_name))->_colorTransform);

	Color return_color;
	return_color.r = transform.redMultiplier;
	return_color.g = transform.greenMultiplier;
	return_color.b = transform.blueMultiplier;
	return_color.a = transform.alphaMultiplier;
	return return_color;
}

void DragonBonesArmature::set_slot_display_color_multiplier(const String &_slot_name, const Color &_color) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return;
	}

	ColorTransform _new_color;
	_new_color.redMultiplier = _color.r;
	_new_color.greenMultiplier = _color.g;
	_new_color.blueMultiplier = _color.b;
	_new_color.alphaMultiplier = _color.a;

	p_armature->getSlot(to_std_str(_slot_name))->_setColor(_new_color);
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
	if (!p_armature) {
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
	if (!p_armature) {
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

Dictionary DragonBonesArmature::get_ik_constraints() {
	Dictionary dict;

	for (auto &constraint : getArmature()->getArmatureData()->constraints) {
		dict[to_gd_str(constraint.first)] = Vector2(constraint.second->target->transform.x, constraint.second->target->transform.y);
	}

	return dict;
}

void DragonBonesArmature::set_ik_constraint(const String &name, Vector2 position) {
	for (dragonBones::Constraint *constraint : getArmature()->_constraints) {
		if (constraint->getName() == name.utf8().get_data()) {
			dragonBones::BoneData *target = const_cast<BoneData *>(constraint->_constraintData->target);
			target->transform.x = position.x;
			target->transform.y = position.y;

			constraint->_constraintData->setTarget(target);
			constraint->update();
			getArmature()->invalidUpdate(target->name, true);
		}
	}
}

void DragonBonesArmature::set_ik_constraint_bend_positive(const String &name, bool bend_positive) {
	for (dragonBones::Constraint *constraint : getArmature()->_constraints) {
		if (constraint->getName() == name.utf8().get_data()) {
			dragonBones::BoneData *target = const_cast<BoneData *>(constraint->_constraintData->target);

			static_cast<IKConstraint *>(constraint)->_bendPositive = bend_positive;
			constraint->update();
			getArmature()->invalidUpdate(target->name, true);
		}
	}
}

BonesDictionary DragonBonesArmature::get_bones() {
	BonesDictionary bones{};

	for (auto &bone : _bones) {
		bones[to_gd_str(bone.first)] = bone.second;
	}

	return bones;
}

Ref<DragonBonesBone> DragonBonesArmature::get_bone(const String &name) {
	auto it = _bones.find(to_std_str(name));
	return it == _bones.end() ? Ref<DragonBonesBone>{} : it->second;
}

Slot *DragonBonesArmature::getSlot(const std::string &name) const {
	return p_armature->getSlot(name);
}

void DragonBonesArmature::add_bone(std::string name, const Ref<DragonBonesBone> &new_bone) {
	_bones.insert(std::make_pair(name, new_bone));
}

void DragonBonesArmature::add_slot(std::string name, const Ref<DragonBonesSlot> &new_slot) {
	_slots.insert(std::make_pair(name, new_slot));
}

void DragonBonesArmature::dbInit(Armature *_p_armature) {
	p_armature = _p_armature;
}

void DragonBonesArmature::dbClear() {
	p_armature = nullptr;
}

void DragonBonesArmature::dbUpdate() {
}

void DragonBonesArmature::dispose(bool disposeProxy) {
	DragonBonesArmature::_onClear();
}

void DragonBonesArmature::_onClear() {
	Display::_onClear();

	_bones.clear();
	_slots.clear();

	if (p_armature) {
		p_armature->dispose();
		if (dragon_bones) {
			dragon_bones->advance(0.0f);
		}
		p_armature = nullptr;
	}
	// TODO: 检查清理是否完全
}

void DragonBonesArmature::setup_recursively() {
	if (!p_armature) {
		return;
	}

	for (Slot *slot : p_armature->getSlots()) {
		if (!slot) {
			continue;
		}

		for_each_armature([this](DragonBonesArmature *p_child_armature) {
			p_child_armature->dragon_bones = dragon_bones;
			p_child_armature->setup_recursively();
		});

		// TODO
		// if (auto display = static_cast<DragonBonesMeshDisplay *>(slot->getRawDisplay())) {
		// 	display->p_owner = this;
		// }
	}
}

void DragonBonesArmature::update_childs(bool _b_color, bool _b_blending) {
	if (!p_armature) {
		return;
	}

	for (Slot *slot : p_armature->getSlots()) {
		if (!slot) {
			continue;
		}

		if (_b_color) {
			slot->_colorDirty = true;
		}

		if (_b_blending) {
			slot->invalidUpdate();
		}

		slot->update(0);
	}
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

		for (auto it : _slots) {
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
	// for (auto it : _slots) {
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
	for (auto it : _slots) {
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
				auto it = _slots.find(to_std_str(slot_name));
				if (it == _slots.end()) {
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

	for (auto kv : _slots) {
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
	if (!armature_node || !armature_node->is_initialized()) {
		return false;
	}

	if (p_name == SNAME("armature_name")) {
		return true;
	}

	for (const auto &prop_info : armature_property_list) {
		if (prop_info.name == p_name) {
			armature_node->set(p_name, p_val);
			if (prop_info.name.ends_with("modulate")) {
				armature_node->queue_redraw();
			} else {
				notify_property_list_changed();
			}
			return true;
		}
	}

	return false;
}

bool DragonBonesArmatureProxy::_get(const StringName &p_name, Variant &r_val) const {
	if (!armature_node || !armature_node->is_initialized()) {
		return false;
	}

	if (p_name == SNAME("armature_name")) {
		r_val = to_gd_str(static_cast<DragonBonesArmature *>(armature_node)->getArmature()->getName());
		return true;
	}

	for (const auto &prop_info : armature_property_list) {
		if (prop_info.name == p_name) {
			r_val = armature_node->get(p_name);
			return true;
		}
	}

	return false;
}

void DragonBonesArmatureProxy::_get_property_list(List<PropertyInfo> *p_list) const {
	if (!armature_node || !armature_node->is_initialized()) {
		return;
	}

	for (const auto &p : armature_property_list) {
		if (p.name == SNAME("current_animation") && armature_node->getArmature()) {
			PropertyInfo info = p;
			String hint = "[none]";
			for (const auto &anim : armature_node->getArmature()->getArmatureData()->getAnimationNames()) {
				hint += ",";
				hint += to_gd_str(anim);
			}
			info.hint_string = hint;
			p_list->push_back(info);
		} else {
			p_list->push_back(p);
		}
	}

	if (armature_node->has_sub_armature()) {
		p_list->push_back(PropertyInfo(Variant::ARRAY, SNAME("sub_armatures"),
				PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, DragonBonesArmatureProxy::get_class_static()),
				PROPERTY_USAGE_EDITOR));
	}
}

#endif // TOOLS_ENABLED