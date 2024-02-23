#include "dragonbones_armature.h"

#include "godot_cpp/classes/global_constants.hpp"
#include "godot_cpp/classes/ref.hpp"

#include "dragonbones.h"

using namespace godot;
using namespace dragonBones;

#define SNAME(sn) ([] {static const StringName ret{sn};return ret; }())

DragonBonesArmature::DragonBonesArmature() {
	set_use_parent_material(true);
}

DragonBonesArmature::~DragonBonesArmature() {
	_bones.clear();
	_slots.clear();
	dispose(true);
}

Ref<CanvasItemMaterial> DragonBonesArmature::get_material_to_set_blend_mode(bool p_required) {
	if (get_use_parent_material()) {
		auto parent = dynamic_cast<GDOwnerNode *>(get_parent());
		if (parent) {
			return parent->get_material_to_set_blend_mode(p_required);
		}
	}

	Ref<CanvasItemMaterial> ret = get_material();
	if (ret.is_null() && p_required) {
		ret.instantiate();
		set_material(ret);
	}
	return ret;
}

void DragonBonesArmature::_bind_methods() {
	ClassDB::bind_method(D_METHOD("for_each_armature", "action"), &DragonBonesArmature::for_each_armature_);
	ClassDB::bind_method(D_METHOD("for_each_armature_recursively_", "action", "current_depth"), &DragonBonesArmature::for_each_armature_recursively_, DEFVAL(0));

	ClassDB::bind_method(D_METHOD("has_animation", "animation_name"), &DragonBonesArmature::has_animation);
	ClassDB::bind_method(D_METHOD("get_animations"), &DragonBonesArmature::get_animations);

	ClassDB::bind_method(D_METHOD("is_playing"), &DragonBonesArmature::is_playing);

	ClassDB::bind_method(D_METHOD("tell_animation", "animation_name"), &DragonBonesArmature::tell_animation);
	ClassDB::bind_method(D_METHOD("seek_animation", "animation_name", "progress"), &DragonBonesArmature::seek_animation);

	ClassDB::bind_method(D_METHOD("play", "animation_name", "loop_count"), &DragonBonesArmature::play);
	ClassDB::bind_method(D_METHOD("play_from_time", "animation_name", "f_time", "loop_count"), &DragonBonesArmature::play_from_time);
	ClassDB::bind_method(D_METHOD("play_from_progress", "animation_name", "f_progress", "loop_count"), &DragonBonesArmature::play_from_progress);
	ClassDB::bind_method(D_METHOD("stop", "animation_name", "b_reset"), &DragonBonesArmature::stop);
	ClassDB::bind_method(D_METHOD("stop_all_animations", "reset", "recursively"), &DragonBonesArmature::stop_all_animations);
	ClassDB::bind_method(D_METHOD("fade_in"), &DragonBonesArmature::fade_in);

	ClassDB::bind_method(D_METHOD("reset", "recurisively"), &DragonBonesArmature::reset);

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
	ClassDB::bind_method(D_METHOD("set_ik_constraint_bend_positive", "constraint_name", "is_positive"), &DragonBonesArmature::set_ik_constraint_bend_positive);

	ClassDB::bind_method(D_METHOD("get_bones"), &DragonBonesArmature::get_bones);
	ClassDB::bind_method(D_METHOD("get_bone", "bone_name"), &DragonBonesArmature::get_bone);

	ClassDB::bind_method(D_METHOD("advance", "delta", "recursively"), &DragonBonesArmature::advance, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("set_debug", "debug", "recursively"), &DragonBonesArmature::set_debug, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("set_active", "active", "recursively"), &DragonBonesArmature::set_active, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("set_callback_mode_process", "callback_mode_process", "recursively"), &DragonBonesArmature::set_callback_mode_process, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("set_slots_inherit_material", "slots_inherit_material", "recursively"), &DragonBonesArmature::set_slots_inherit_material);

	// Setter Getter
	ClassDB::bind_method(D_METHOD("set_current_animation", "current_animation"), &DragonBonesArmature::set_current_animation);
	ClassDB::bind_method(D_METHOD("get_current_animation"), &DragonBonesArmature::get_current_animation);

	ClassDB::bind_method(D_METHOD("set_animation_progress", "progress"), &DragonBonesArmature::set_animation_progress);
	ClassDB::bind_method(D_METHOD("get_animation_progress"), &DragonBonesArmature::get_animation_progress);

	ClassDB::bind_method(D_METHOD("set_debug_", "debug"), &DragonBonesArmature::set_debug_);
	ClassDB::bind_method(D_METHOD("is_debug"), &DragonBonesArmature::is_debug);

	ClassDB::bind_method(D_METHOD("set_active_", "active"), &DragonBonesArmature::set_active_);
	ClassDB::bind_method(D_METHOD("is_active"), &DragonBonesArmature::is_active);

	ClassDB::bind_method(D_METHOD("set_callback_mode_process_", "callback_mode_process"), &DragonBonesArmature::set_callback_mode_process_);
	ClassDB::bind_method(D_METHOD("get_callback_mode_process"), &DragonBonesArmature::get_callback_mode_process);

	ClassDB::bind_method(D_METHOD("set_flip_x_", "flip_x"), &DragonBonesArmature::set_flip_x_);
	ClassDB::bind_method(D_METHOD("is_flipped_x"), &DragonBonesArmature::is_flipped_x);

	ClassDB::bind_method(D_METHOD("set_flip_y_", "flip_y"), &DragonBonesArmature::set_flip_y_);
	ClassDB::bind_method(D_METHOD("is_flipped_y"), &DragonBonesArmature::is_flipped_y);

	ClassDB::bind_method(D_METHOD("set_texture_override", "texture_override"), &DragonBonesArmature::set_texture_override);
	ClassDB::bind_method(D_METHOD("get_texture_override"), &DragonBonesArmature::get_texture_override);

	ClassDB::bind_method(D_METHOD("set_slots_inherit_material_", "slots_inherit_material"), &DragonBonesArmature::set_slots_inherit_material_);
	ClassDB::bind_method(D_METHOD("is_slots_inherit_material"), &DragonBonesArmature::is_slots_inherit_material);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug"), "set_debug_", "is_debug");

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "current_animation", PROPERTY_HINT_ENUM, "", PROPERTY_USAGE_EDITOR), "set_current_animation", "get_current_animation");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "animation_progress", PROPERTY_HINT_RANGE, "0.0,1.0,0.0001"), "set_animation_progress", "get_animation_progress");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "active"), "set_active_", "is_active");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "slots_inherit_material"), "set_slots_inherit_material_", "is_slots_inherit_material");

	ADD_GROUP("Flip", "flip_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_x"), "set_flip_x_", "is_flipped_x");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_y"), "set_flip_y_", "is_flipped_y");

	ADD_GROUP("Callback Mode", "callback_mode_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "callback_mode_process", PROPERTY_HINT_ENUM, "Physics,Idle,Manual"), "set_callback_mode_process_", "get_callback_mode_process");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture_override", PROPERTY_HINT_RESOURCE_TYPE, Texture2D::get_class_static()), "set_texture_override", "get_texture_override");

	// Enum
	BIND_CONSTANT(ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS);
	BIND_CONSTANT(ANIMATION_CALLBACK_MODE_PROCESS_IDLE);
	BIND_CONSTANT(ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);

	BIND_CONSTANT(FADE_OUT_NONE);
	BIND_CONSTANT(FADE_OUT_SAME_LAYER);
	BIND_CONSTANT(FADE_OUT_SAME_GROUP);
	BIND_CONSTANT(FADE_OUT_SAME_LAYER_AND_GROUP);
	BIND_CONSTANT(FADE_OUT_ALL);
	BIND_CONSTANT(FADE_OUT_SINGLE);

#ifdef TOOLS_ENABLED
	auto props = ClassDB::class_get_property_list(get_class_static(), true);
	auto tmp_obj = memnew(DragonBonesArmature);
	for (size_t i = 0; i < props.size(); ++i) {
		Dictionary prop = props[i];
		if ((uint32_t)prop["usage"] & PROPERTY_USAGE_STORAGE) {
			storage_properties.emplace_back(StoragedProperty{ prop["name"], tmp_obj->get(prop["name"]) });
		}

		DragonBonesArmatureProxy::armature_property_list.emplace_back(PropertyInfo(
				(Variant::Type)((int)prop["type"]), (StringName)prop["name"], (PropertyHint)((int)prop["hint"]),
				(String)prop["hint_string"], (uint64_t)(prop["usage"]), (StringName)prop["class"]));
	}

	storage_properties.emplace_back(StoragedProperty{ "use_parent_material", false });
	DragonBonesArmatureProxy::armature_property_list.emplace_back(PropertyInfo(Variant::BOOL, "use_parent_material"));

	memdelete(tmp_obj);
#endif // TOOLS_ENABLED
}

void DragonBonesArmature::for_each_armature_(const Callable &p_action) {
	for_each_armature([&](auto p_child_armature) {
		return p_action.call(p_child_armature).booleanize();
	});
}

void DragonBonesArmature::for_each_armature_recursively_(const Callable &p_action, int p_current_depth) {
	for_each_armature_recursively(
			[&p_action](auto p_child_armature, auto depth) {
				return p_action.call(p_child_armature, depth).booleanize();
			},
			p_current_depth);
}

void DragonBonesArmature::set_debug(bool _b_debug, bool p_recursively) {
	if (!p_armature)
		return;

	b_debug = _b_debug;
	for (Slot *slot : p_armature->getSlots()) {
		if (!slot)
			continue;

		if (p_recursively) {
			for_each_armature([_b_debug](DragonBonesArmature *p_child_armature) {
				p_child_armature->set_debug(_b_debug, true);
			});
		}

		if (auto display = static_cast<GDDisplay *>(slot->getRawDisplay())) {
			display->b_debug = _b_debug;
			display->queue_redraw();
		}
	}
}

bool DragonBonesArmature::has_animation(const String &_animation_name) const {
	if (p_armature == nullptr || !getAnimation()) {
		return false;
	}

	return getArmature()->getArmatureData()->getAnimation(_animation_name.ascii().get_data()) != nullptr;
}

PackedStringArray DragonBonesArmature::get_animations() {
	PackedStringArray animations{};

	const ArmatureData *data = p_armature->getArmatureData();

	for (std::string animation_name : data->getAnimationNames()) {
		animations.push_back(String(animation_name.c_str()));
	}

	return animations;
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
		play(p_animation, static_cast<DragonBones *>(p_owner)->get_animation_loop());
	} else if (get_current_animation() != p_animation) {
		play(p_animation, static_cast<DragonBones *>(p_owner)->get_animation_loop());
	} else {
		// 相同动画，无需响应
	}
}

String DragonBonesArmature::get_current_animation() const {
	if (!p_armature || !getAnimation())
		return {};
	return getAnimation()->getLastAnimationName().c_str();
}

String DragonBonesArmature::get_current_animation_on_layer(int _layer) const {
	if (!getAnimation())
		return {};

	std::vector<AnimationState *> states = p_armature->getAnimation()->getStates();

	for (AnimationState *state : states) {
		if (state->layer == _layer) {
			return state->getName().c_str();
		}
	}

	return {};
}

String DragonBonesArmature::get_current_animation_in_group(const String &_group_name) const {
	if (!getAnimation())
		return {};

	std::vector<AnimationState *> states = getAnimation()->getStates();

	for (AnimationState *state : states) {
		if (state->group == _group_name.ascii().get_data()) {
			return state->getName().c_str();
		}
	}

	return {};
}

float DragonBonesArmature::tell_animation(const String &_animation_name) const {
	if (has_animation(_animation_name)) {
		AnimationState *animation_state = getAnimation()->getState(_animation_name.ascii().get_data());
		if (animation_state)
			return animation_state->getCurrentTime() / animation_state->getTotalTime();
	}
	return 0.0f;
}

void DragonBonesArmature::seek_animation(const String &_animation_name, float progress) {
	if (has_animation(_animation_name)) {
		stop(_animation_name, true);
		auto current_progress = Math::fmod(progress, 1.0f);
		if (current_progress == 0 && progress != 0)
			current_progress = 1.0f;
		p_armature->getAnimation()->gotoAndStopByProgress(_animation_name.ascii().get_data(), current_progress < 0 ? 1. + current_progress : current_progress);
	}
}

bool DragonBonesArmature::is_playing() const {
	return getAnimation()->isPlaying();
}

void DragonBonesArmature::play(const String &_animation_name, int loop) {
	if (has_animation(_animation_name)) {
		getAnimation()->play(_animation_name.ascii().get_data(), loop);

		_set_process(true);
	}
	// TODO: 是否需要在没有动画时停止一切动画
}

void DragonBonesArmature::play_from_time(const String &_animation_name, float _f_time, int loop) {
	if (has_animation(_animation_name)) {
		play(_animation_name, loop);
		getAnimation()->gotoAndPlayByTime(_animation_name.ascii().get_data(), _f_time);
	}
}

void DragonBonesArmature::play_from_progress(const String &_animation_name, float f_progress, int loop) {
	if (has_animation(_animation_name)) {
		play(_animation_name, loop);
		getAnimation()->gotoAndPlayByProgress(_animation_name.ascii().get_data(), f_progress);
	}
}

void DragonBonesArmature::stop(const String &_animation_name, bool b_reset, bool p_recursively) {
	if (getAnimation()) {
		getAnimation()->stop(_animation_name.ascii().get_data());

		if (b_reset) {
			reset();
		}
	}

	_set_process(false);

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

	_set_process(false);

	if (p_recursively) {
		for_each_armature([b_reset](DragonBonesArmature *p_child_armature) {
			p_child_armature->stop_all_animations(b_reset, true);
		});
	}
}

void DragonBonesArmature::fade_in(const String &_animation_name, float _time, int _loop, int _layer, const String &_group, AnimFadeOutMode _fade_out_mode) {
	if (has_animation(_animation_name)) {
		getAnimation()->fadeIn(_animation_name.ascii().get_data(), _time, _loop, _layer, _group.ascii().get_data(), (AnimationFadeOutMode)_fade_out_mode);
		_set_process(true);
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
	return getArmature()->getSlot(_slot_name.ascii().get_data()) != nullptr;
}

Dictionary DragonBonesArmature::get_slots() {
	Dictionary slots{};

	for (auto &slot : _slots) {
		slots[slot.first.c_str()] = slot.second;
	}

	return slots;
}

Ref<DragonBonesSlot> DragonBonesArmature::get_slot(const String &_slot_name) {
	return _slots[_slot_name.ascii().get_data()];
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
		const char *desired_item = _item_name.ascii().get_data();
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
		set_slot_by_item_name(String(slot->getName().c_str()), _item_name);
	}
}

int DragonBonesArmature::get_slot_display_index(const String &_slot_name) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return -1;
	}
	return p_armature->getSlot(_slot_name.ascii().get_data())->getDisplayIndex();
}

int DragonBonesArmature::get_total_items_in_slot(const String &_slot_name) {
	if (!has_slot(_slot_name)) {
		WARN_PRINT("Slot " + _slot_name + " doesn't exist");
		return -1;
	}
	return p_armature->getSlot(_slot_name.ascii().get_data())->getDisplayList().size();
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
	ColorTransform transform(p_armature->getSlot(_slot_name.ascii().get_data())->_colorTransform);

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

	p_armature->getSlot(_slot_name.ascii().get_data())->_setColor(_new_color);
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
	return texture;
}

void DragonBonesArmature::set_texture_override(const Ref<Texture2D> &p_texture_override) {
	texture = p_texture_override;
}

Dictionary DragonBonesArmature::get_ik_constraints() {
	Dictionary dict;

	for (auto &constraint : getArmature()->getArmatureData()->constraints) {
		dict[String(constraint.first.c_str())] = Vector2(constraint.second->target->transform.x, constraint.second->target->transform.y);
	}

	return dict;
}

void DragonBonesArmature::set_ik_constraint(const String &name, Vector2 position) {
	for (dragonBones::Constraint *constraint : getArmature()->_constraints) {
		if (constraint->getName() == name.ascii().get_data()) {
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
		if (constraint->getName() == name.ascii().get_data()) {
			dragonBones::BoneData *target = const_cast<BoneData *>(constraint->_constraintData->target);

			static_cast<IKConstraint *>(constraint)->_bendPositive = bend_positive;
			constraint->update();
			getArmature()->invalidUpdate(target->name, true);
		}
	}
}

Dictionary DragonBonesArmature::get_bones() {
	Dictionary bones{};

	for (auto &bone : _bones) {
		bones[bone.first.c_str()] = bone.second;
	}

	return bones;
}

Ref<DragonBonesBone> DragonBonesArmature::get_bone(const String &name) {
	return _bones[name.ascii().get_data()];
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

void DragonBonesArmature::dispose(bool _disposeProxy) {
	if (p_armature) {
		p_armature->dispose();
		p_armature = nullptr;
	}
}

void DragonBonesArmature::setup_recursively(bool _b_debug) {
	if (!p_armature)
		return;

	b_debug = _b_debug;
	for (Slot *slot : p_armature->getSlots()) {
		if (!slot)
			continue;

		for_each_armature([this](DragonBonesArmature *p_child_armature) {
			p_child_armature->p_owner = p_owner;
			p_child_armature->setup_recursively(b_debug);
		});

		if (auto display = static_cast<GDDisplay *>(slot->getRawDisplay())) {
			add_child(display);
			display->p_owner = this;
			display->b_debug = _b_debug;
		}
	}
}

void DragonBonesArmature::update_childs(bool _b_color, bool _b_blending) {
	if (!p_armature)
		return;

	for (Slot *slot : p_armature->getSlots()) {
		if (!slot)
			continue;

		if (_b_color)
			slot->_colorDirty = true;

		if (_b_blending)
			slot->invalidUpdate();

		slot->update(0);
	}
}

void DragonBonesArmature::set_slots_inherit_material(bool p_slots_inherit_material, bool p_recursively) {
	if (!p_armature)
		return;

	slots_inherit_material = p_slots_inherit_material;

	for (Slot *slot : p_armature->getSlots()) {
		if (!slot)
			continue;

		if (auto display = static_cast<GDDisplay *>(slot->getRawDisplay())) {
			display->set_use_parent_material(p_slots_inherit_material);
		}
	}

	if (p_recursively) {
		for_each_armature([p_slots_inherit_material](auto p_child_armature) {
			p_child_armature->set_slots_inherit_material(p_slots_inherit_material, true);
		});
	}
}

bool DragonBonesArmature::is_slots_inherit_material() const {
	return slots_inherit_material;
}

void DragonBonesArmature::update_texture_atlas(const Ref<Texture> &_m_texture_atlas) {
	if (!p_armature)
		return;

	for (Slot *slot : p_armature->getSlots()) {
		if (!slot)
			continue;
		if (auto display = static_cast<GDDisplay *>(slot->getRawDisplay())) {
			display->texture = _m_texture_atlas;
			display->queue_redraw();
		}
	}
}

void DragonBonesArmature::update_material_inheritance_recursively(bool p_inheritance) {
	set_use_parent_material(p_inheritance);

	for_each_armature([p_inheritance](auto p_child_armature) {
		p_child_armature->update_material_inheritance_recursively(p_inheritance);
	});
}

//
void DragonBonesArmature::set_active(bool p_active, bool p_recursively) {
	if (active != p_active) {
		active = p_active;

		_set_process(processing, true);
	}

	if (p_recursively) {
		for_each_armature([p_active](DragonBonesArmature *p_child_armature) {
			p_child_armature->set_active(p_active, true);
		});
	}
}

void DragonBonesArmature::set_callback_mode_process(AnimationCallbackModeProcess p_process_mode, bool p_recursively) {
	if (callback_mode_process != p_process_mode) {
		bool was_active = is_active();
		if (was_active) {
			set_active(false);
		}

		callback_mode_process = p_process_mode;

		if (was_active) {
			set_active(true);
		}
	}

	if (p_recursively) {
		for_each_armature([p_process_mode](DragonBonesArmature *p_child_armature) {
			p_child_armature->set_callback_mode_process(p_process_mode, true);
		});
	}
}

void DragonBonesArmature::set_animation_progress(float p_progress) {
	seek_animation(get_current_animation(), p_progress);
}

float DragonBonesArmature::get_animation_progress() const {
	return tell_animation(get_current_animation());
}

#ifdef TOOLS_ENABLED
std::vector<DragonBonesArmature::StoragedProperty> DragonBonesArmature::storage_properties{};

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

void DragonBonesArmature::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (!processing) {
				set_physics_process_internal(false);
				set_process_internal(false);
			}
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (active && callback_mode_process == DragonBonesArmature::ANIMATION_CALLBACK_MODE_PROCESS_IDLE)
				advance(get_process_delta_time());
		} break;

		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (active && callback_mode_process == DragonBonesArmature::ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS)
				advance(get_physics_process_delta_time());
		} break;
	}
}

void DragonBonesArmature::_set_process(bool p_process, bool p_force) {
	if (processing == p_process && !p_force) {
		return;
	}

	set_physics_process_internal(callback_mode_process == DragonBonesArmature::ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS && p_process && active);
	set_process_internal(callback_mode_process == DragonBonesArmature::ANIMATION_CALLBACK_MODE_PROCESS_IDLE && p_process && active);

	processing = p_process;
}

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
				auto it = _slots.find(slot_name.ascii().get_data());
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
			sub_armatures_setting[slot_name.c_str()] = sub_armature->get_settings();
		}
	}

	return ret;
}
#endif //  TOOLS_ENABLED
////////////
#ifdef TOOLS_ENABLED
std::vector<PropertyInfo> DragonBonesArmatureProxy::armature_property_list{};

bool DragonBonesArmatureProxy::_set(const StringName &p_name, const Variant &p_val) {
	if (!armature_node) {
		return false;
	}

	if (p_name == SNAME("armature_name")) {
		return true;
	}

	for (const auto &prop_info : armature_property_list) {
		if (prop_info.name == p_name) {
			armature_node->set(p_name, p_val);
			notify_property_list_changed();
			return true;
		}
	}

	return false;
}

bool DragonBonesArmatureProxy::_get(const StringName &p_name, Variant &r_val) const {
	if (!armature_node) {
		return false;
	}

	if (p_name == SNAME("armature_name")) {
		r_val = static_cast<DragonBonesArmature *>(armature_node)->getArmature()->getName().c_str();
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
	if (!armature_node) {
		return;
	}

	for (const auto &p : armature_property_list) {
		if (p.name == SNAME("current_animation") && armature_node->getArmature()) {
			PropertyInfo info = p;
			String hint = "[none]";
			for (const auto &anim : armature_node->getArmature()->getArmatureData()->getAnimationNames()) {
				hint += ",";
				hint += anim.c_str();
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