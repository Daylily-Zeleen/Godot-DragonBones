#pragma once

#include "wrappers/GDDisplay.h"

#include "dragonBones/armature/Armature.h"
#include "dragonBones/armature/IArmatureProxy.h"
#include "dragonbones_bone.h"
#include "dragonbones_slot.h"

#include "godot_cpp/classes/texture2d.hpp"

namespace godot {

// 同一个DragonBones中的Armature公用同一个时钟，不做特殊处理不能单独设置Armature的动画Speed
class DragonBonesArmature : public GDDisplay, public dragonBones::IArmatureProxy {
	GDCLASS(DragonBonesArmature, GDDisplay)
public:
	enum AnimationCallbackModeProcess {
		ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS = 0,
		ANIMATION_CALLBACK_MODE_PROCESS_IDLE = 1,
		ANIMATION_CALLBACK_MODE_PROCESS_MANUAL = 2,
	};

	enum AnimFadeOutMode {
		FADE_OUT_NONE,
		FADE_OUT_SAME_LAYER,
		FADE_OUT_SAME_GROUP,
		FADE_OUT_SAME_LAYER_AND_GROUP,
		FADE_OUT_ALL,
		FADE_OUT_SINGLE,
	};

private:
	AnimationCallbackModeProcess callback_mode_process{ ANIMATION_CALLBACK_MODE_PROCESS_IDLE };
	bool active{ true };
	bool processing{ false };

	bool slots_inherit_material{ true };

protected:
	dragonBones::Armature *p_armature{ nullptr };
	std::map<std::string, Ref<DragonBonesBone>> _bones;
	std::map<std::string, Ref<DragonBonesSlot>> _slots;

public:
	DragonBonesArmature();
	~DragonBonesArmature();

	virtual void update_modulate(const Color &p_modulate) override {
		set_modulate(p_modulate);
		update_childs(true);
	}

	virtual void dispatch_event(const String &_str_type, const dragonBones::EventObject *_p_value) override {
		if (p_owner) {
			p_owner->dispatch_event(_str_type, _p_value);
		}
	}

	virtual void dispatch_sound_event(const String &_str_type, const dragonBones::EventObject *_p_value) override {
		if (p_owner) {
			p_owner->dispatch_sound_event(_str_type, _p_value);
		}
	}

	virtual Ref<CanvasItemMaterial> get_material_to_set_blend_mode(bool p_required) override;

	dragonBones::Slot *getSlot(const std::string &name) const;

	void add_bone(std::string name, const Ref<DragonBonesBone> &new_bone);
	void add_slot(std::string name, const Ref<DragonBonesSlot> &new_slot);
	void addEvent(const std::string &_type, const std::function<void(dragonBones::EventObject *)> &_callback);
	void removeEvent(const std::string &_type);

	virtual bool hasDBEventListener(const std::string &_type) const override { return true; }
	virtual void addDBEventListener(const std::string &_type, const std::function<void(dragonBones::EventObject *)> &_listener) override {}
	virtual void removeDBEventListener(const std::string &_type, const std::function<void(dragonBones::EventObject *)> &_listener) override {}
	virtual void dispatchDBEvent(const std::string &_type, dragonBones::EventObject *_value) override {
		this->dispatch_event(String(_type.c_str()), _value);
	}

	void dbInit(dragonBones::Armature *_p_armature) override;
	void dbClear() override;
	void dbUpdate() override;

	void dispose(bool disposeProxy) override;

	virtual dragonBones::Armature *getArmature() const override { return p_armature; }
	virtual dragonBones::Animation *getAnimation() const override { return p_armature->getAnimation(); }

	void setup_recursively(bool _b_debug);
	void update_childs(bool _b_color, bool _b_blending = false);
	void update_texture_atlas(const Ref<Texture> &_m_texture_atlas);
	void update_material_inheritance_recursively(bool p_inheritance);

	//
	dragonBones::Slot *getSlot(const String &p_name) const { return p_armature->getSlot(p_name.ascii().get_data()); }

	template <typename Func, typename std::enable_if<std::is_invocable_v<Func, DragonBonesArmature *>>::type *_dummy = nullptr>
	void for_each_armature(Func &&p_action) {
		for (auto slot : getArmature()->getSlots()) {
			if (slot->getDisplayList().size() == 0)
				continue;
			auto display = slot->getDisplayList()[slot->getDisplayIndex()];
			if (display.second == dragonBones::DisplayType::Armature) {
				dragonBones::Armature *armature = static_cast<dragonBones::Armature *>(display.first);
				DragonBonesArmature *convertedDisplay = static_cast<DragonBonesArmature *>(armature->getDisplay());
				if constexpr (std::is_invocable_r_v<bool, Func, DragonBonesArmature *>) {
					if (p_action(convertedDisplay)) {
						break;
					}
				} else {
					p_action(convertedDisplay);
				}
			}
		}
	}

	template <typename Func, typename std::enable_if<std::is_invocable_v<Func, DragonBonesArmature *, int>>::type *_dummy = nullptr>
	void for_each_armature_recursively(Func &&p_action, int p_current_depth = 0) {
		for_each_armature([&p_action, p_current_depth](auto p_child_armature) {
			if constexpr (std::is_invocable_r_v<bool, Func, DragonBonesArmature *, int>) {
				if (p_action(p_child_armature, p_current_depth)) {
					return;
				}
			} else {
				p_action(p_child_armature, p_current_depth);
			}
			p_child_armature->for_each_armature_recursively(std::forward<Func>(p_action), p_current_depth + 1);
		});
	}

public:
	/* METHOD BINDINGS */
	static void _bind_methods();

	void for_each_armature_(const Callable &p_action);
	void for_each_armature_recursively_(const Callable &p_action, int p_current_depth = 0);

	bool has_animation(const String &_animation_name) const;
	PackedStringArray get_animations();

	String get_current_animation_on_layer(int _layer) const;
	String get_current_animation_in_group(const String &_group_name) const;

	float tell_animation(const String &_animation_name) const;
	void seek_animation(const String &_animation_name, float progress);

	bool is_playing() const;
	void play(const String &_animation_name, int loop = -1);

	void play_from_time(const String &_animation_name, float _f_time, int loop = -1);
	void play_from_progress(const String &_animation_name, float f_progress, int loop = -1);
	void stop(const String &_animation_name, bool b_reset = false, bool p_recursively = false);
	void stop_all_animations(bool b_reset = false, bool p_recursively = false);
	void fade_in(const String &_animation_name, float _time,
			int _loop, int _layer, const String &_group, AnimFadeOutMode _fade_out_mode);

	void reset(bool p_recursively = false);

	bool has_slot(const String &_slot_name) const;
	Ref<DragonBonesSlot> get_slot(const String &_slot_name);
	Dictionary get_slots();

	void set_slot_display_index(const String &_slot_name, int _index);
	void set_slot_by_item_name(const String &_slot_name, const String &_item_name);
	void set_all_slots_by_item_name(const String &_item_name);
	int get_slot_display_index(const String &_slot_name);
	int get_total_items_in_slot(const String &_slot_name);
	void cycle_next_item_in_slot(const String &_slot_name);
	void cycle_previous_item_in_slot(const String &_slot_name);
	Color get_slot_display_color_multiplier(const String &_slot_name);
	void set_slot_display_color_multiplier(const String &_slot_name, const Color &_color);

	Dictionary get_ik_constraints();
	void set_ik_constraint(const String &name, Vector2 position);
	void set_ik_constraint_bend_positive(const String &name, bool bend_positive);

	Dictionary get_bones();
	Ref<DragonBonesBone> get_bone(const String &name);

	void advance(float p_delta, bool p_recursively = false);

	// setget
	void set_current_animation(const String &p_animation);
	String get_current_animation() const;

	void set_animation_progress(float p_progress);
	float get_animation_progress() const;

	void set_debug_(bool p_debug) { set_debug(p_debug); }
	void set_debug(bool _b_debug, bool p_recursively = false);
	bool is_debug() const { return b_debug; }

	void set_active_(bool p_active) { set_active(p_active); }
	void set_active(bool p_active, bool p_recursively = false);
	bool is_active() const { return active; }

	void set_callback_mode_process_(AnimationCallbackModeProcess p_process_mode) { set_callback_mode_process(p_process_mode); }
	void set_callback_mode_process(AnimationCallbackModeProcess p_process_mode, bool p_recursively = false);
	AnimationCallbackModeProcess get_callback_mode_process() const { return callback_mode_process; }

	void set_flip_x_(bool p_flip_x) { set_flip_x(p_flip_x); }
	void set_flip_x(bool p_flip_x, bool p_recursively = false);
	bool is_flipped_x() const;

	void set_flip_y_(bool p_flip_y) { set_flip_y(p_flip_y); }
	void set_flip_y(bool p_flip_y, bool p_recursively = false);
	bool is_flipped_y() const;

	Ref<Texture2D> get_texture_override() const;
	void set_texture_override(const Ref<Texture2D> &p_texture_override);

	void set_slots_inherit_material_(bool p_slots_inherit_material) { set_slots_inherit_material(p_slots_inherit_material); }
	void set_slots_inherit_material(bool p_slots_inherit_material, bool p_recursively = false);
	bool is_slots_inherit_material() const;

public:
	void set_settings(const Dictionary &p_setting);
#ifdef TOOLS_ENABLED
	Dictionary get_settings() const;

#endif // TOOLS_ENABLED

protected:
	void _notification(int p_what);

#ifdef TOOLS_ENABLED
	struct StoragedProperty {
		StringName name;
		Variant default_value;
	};
	static std::vector<StoragedProperty> storage_properties;

	bool _set(const StringName &p_name, const Variant &p_val);
	bool _get(const StringName &p_name, Variant &r_val) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	bool has_sub_armature() const;

protected:
#endif // TOOLS_ENABLED

private:
	void _set_process(bool p_process, bool p_force = false);
};

#ifdef TOOLS_ENABLED
class DragonBonesArmatureProxy : public Resource {
	GDCLASS(DragonBonesArmatureProxy, Resource)
protected:
	static void _bind_methods() {}

	bool _set(const StringName &p_name, const Variant &p_val);
	bool _get(const StringName &p_name, Variant &r_val) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	DragonBonesArmatureProxy() = default;
	DragonBonesArmatureProxy(DragonBonesArmature *p_armature_node) :
			armature_node(p_armature_node) {}

private:
	static std::vector<PropertyInfo> armature_property_list;
	friend class DragonBonesArmature;

	DragonBonesArmature *armature_node{ nullptr };
	friend class DragonBones;
};
#endif // TOOLS_ENABLED

} //namespace godot

VARIANT_ENUM_CAST(godot::DragonBonesArmature::AnimationCallbackModeProcess);
VARIANT_ENUM_CAST(godot::DragonBonesArmature::AnimFadeOutMode);