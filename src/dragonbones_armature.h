#pragma once

#include "dragonBones/armature/Armature.h"
#include "dragonBones/armature/IArmatureProxy.h"
#include "dragonbones_bone.h"
#include "dragonbones_slot.h"

#include "godot_cpp/classes/texture2d.hpp"

#include <godot_cpp/core/version.hpp>

#if GODOT_VERSION_MAJOR > 4 || (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4)
#include "godot_cpp/variant/typed_dictionary.hpp"
using SlotsDictionary = godot::TypedDictionary<godot::String, godot::Ref<godot::DragonBonesSlot>>;
using BonesDictionary = godot::TypedDictionary<godot::String, godot::Ref<godot::DragonBonesBone>>;
#else // GODOT_VERSION_MAJOR > 4 || (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4)
using SlotsDictionary = godot::Dictionary;
using BonesDictionary = godot::Dictionary;
#endif // GODOT_VERSION_MAJOR > 4 || (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4)

namespace godot {

class DragonBonesArmature : public Object, public Display, public dragonBones::IArmatureProxy {
	GDCLASS(DragonBonesArmature, Object)
public:
	enum AnimFadeOutMode {
		FADE_OUT_NONE,
		FADE_OUT_SAME_LAYER,
		FADE_OUT_SAME_GROUP,
		FADE_OUT_SAME_LAYER_AND_GROUP,
		FADE_OUT_ALL,
		FADE_OUT_SINGLE,
	};

private:
	class Slot_GD *slot{ nullptr };
	friend class Slot_GD;

	// bool active{ true };
	// bool processing{ false };
	// float time_scale{ 1.0f };

	bool slots_inherit_material{ true };

	Ref<Texture2D> texture_override;

	class DragonBones *dragon_bones{ nullptr };
	friend class DragonBones;

protected:
	dragonBones::Armature *p_armature{ nullptr };
	std::map<std::string, Ref<DragonBonesBone>> _bones;
	std::map<std::string, Ref<DragonBonesSlot>> _slots;

public:
	DragonBonesArmature() = default;
	virtual ~DragonBonesArmature() override;

	dragonBones::Slot *getSlot(const std::string &name) const;

	void add_bone(std::string name, const Ref<DragonBonesBone> &new_bone);
	void add_slot(std::string name, const Ref<DragonBonesSlot> &new_slot);
	void addEvent(const std::string &_type, const std::function<void(dragonBones::EventObject *)> &_callback);
	void removeEvent(const std::string &_type);

	virtual bool hasDBEventListener(const std::string &_type) const override { return true; }
	virtual void addDBEventListener(const std::string &_type, const std::function<void(dragonBones::EventObject *)> &_listener) override {}
	virtual void removeDBEventListener(const std::string &_type, const std::function<void(dragonBones::EventObject *)> &_listener) override {}

	virtual void dispatchDBEvent(const std::string &_type, dragonBones::EventObject *_value) override;

	void dbInit(dragonBones::Armature *_p_armature) override;
	void dbClear() override;
	void dbUpdate() override;

	void dispose(bool disposeProxy) override;

	virtual dragonBones::Armature *getArmature() const override { return p_armature; }
	virtual dragonBones::Animation *getAnimation() const override { return p_armature->getAnimation(); }

	void setup_recursively();
	void update_childs(bool _b_color, bool _b_blending = false);

	//
	dragonBones::Slot *getSlot(const String &p_name) const { return p_armature->getSlot(to_std_str(p_name)); }

	template <typename Func, typename std::enable_if<std::is_invocable_v<Func, DragonBonesArmature *>>::type *_dummy = nullptr>
	void for_each_armature(Func &&p_action) {
		for (auto slot : getArmature()->getSlots()) {
			if (slot->getDisplayList().size() == 0) {
				continue;
			}
			if (slot->getDisplayIndex() < 0) {
				slot->setDisplayIndex(0);
			}
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

	virtual void queue_redraw() const override;
	virtual void append_draw_data(VMap<int, LocalVector<DrawData>> &r_data, const Transform2D &p_base_transfrom = Transform2D()) const override;

public:
	bool is_initialized() const { return p_armature; }

	/* METHOD BINDINGS */
	static void _bind_methods();
	_DEFINE_TO_STRING()

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
	SlotsDictionary get_slots();

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

	BonesDictionary get_bones();
	Ref<DragonBonesBone> get_bone(const String &name);

	Rect2 get_rect() const;
	void advance(float p_delta, bool p_recursively = false);

	// setget
	void set_current_animation(const String &p_animation);
	String get_current_animation() const;

	void set_animation_progress(float p_progress);
	float get_animation_progress() const;

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
#ifdef TOOLS_ENABLED
	struct StoredProperty {
		StringName name;
		Variant default_value;
	};
	static std::vector<StoredProperty> storage_properties;

	bool _set(const StringName &p_name, const Variant &p_val);
	bool _get(const StringName &p_name, Variant &r_val) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	bool has_sub_armature() const;

protected:
#endif // TOOLS_ENABLED
};

#ifdef TOOLS_ENABLED
class DragonBonesArmatureProxy : public Resource {
	GDCLASS(DragonBonesArmatureProxy, Resource)
protected:
	static void _bind_methods() {}
	_DEFINE_TO_STRING()

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

VARIANT_ENUM_CAST(godot::DragonBonesArmature::AnimFadeOutMode);