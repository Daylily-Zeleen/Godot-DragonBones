#pragma once

#include <godot_dragonbones.h>

#include <dragonBones/armature/Armature.h>
#include <dragonBones/armature/IArmatureProxy.h>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/version.hpp>

#include "bone.h"
#include "slot.h"

#if GODOT_VERSION_MAJOR > 4 || (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4)
#include "godot_cpp/variant/typed_dictionary.hpp"
using SlotsDictionary = godot::TypedDictionary<godot::String, godot::DragonBonesSlot>;
using BonesDictionary = godot::TypedDictionary<godot::String, godot::DragonBonesBone>;
using ConstraintsDictionary = godot::TypedDictionary<godot::String, godot::Vector2>;
#else // GODOT_VERSION_MAJOR > 4 || (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4)
using SlotsDictionary = godot::Dictionary;
using BonesDictionary = godot::Dictionary;
using ConstraintsDictionary = godot::Dictionary;
#endif // GODOT_VERSION_MAJOR > 4 || (GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR >= 4)

namespace godot {

class DragonBonesArmatureDisplay;

class DragonBonesArmature : public Object, public Display, public dragonBones::IArmatureProxy {
	GDCLASS(DragonBonesArmature, Object)

private:
	class Slot_GD *slot{ nullptr };
	Ref<Texture2D> texture_override;

	class DragonBonesArmatureDisplay *armature_display{ nullptr };
	friend class DragonBonesFactory;

protected:
	dragonBones::Armature *armature_instance{ nullptr };

	std::map<std::string, Ref<DragonBonesBone>> bones;
	std::map<std::string, Ref<DragonBonesSlot>> slots;

public:
	enum AnimFadeOutMode {
		FADE_OUT_NONE,
		FADE_OUT_SAME_LAYER,
		FADE_OUT_SAME_GROUP,
		FADE_OUT_SAME_LAYER_AND_GROUP,
		FADE_OUT_ALL,
		FADE_OUT_SINGLE,
	};

	DragonBonesArmature() = default;
	virtual ~DragonBonesArmature() override;

	void add_bone(std::string p_name, const Ref<DragonBonesBone> &p_new_bone);
	void add_slot(std::string p_name, const Ref<DragonBonesSlot> &p_new_slot);

	virtual bool hasDBEventListener(const std::string &p_type) const override { return true; }
	virtual void addDBEventListener(const std::string &p_type, const std::function<void(dragonBones::EventObject *)> &p_listener) override {}
	virtual void removeDBEventListener(const std::string &p_type, const std::function<void(dragonBones::EventObject *)> &p_listener) override {}

	virtual void dispatchDBEvent(const std::string &p_type, dragonBones::EventObject *p_value) override;

	void dbInit(dragonBones::Armature *p_armature) override;
	void dbClear() override;
	void dbUpdate() override;

	void dispose(bool disposeProxy) override {} // 未被使用的纯虚函数
	virtual void release() override; // Display

	virtual dragonBones::Armature *getArmature() const override { return armature_instance; }
	virtual dragonBones::Animation *getAnimation() const override { return armature_instance->getAnimation(); }

	void force_update();

	//
	dragonBones::Slot *getSlot(const String &p_name) const { return armature_instance->getSlot(to_std_str(p_name)); }

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
				dragonBones::Armature *armature_display = static_cast<dragonBones::Armature *>(display.first);
				DragonBonesArmature *convertedDisplay = static_cast<DragonBonesArmature *>(armature_display->getDisplay());
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
	bool is_valid() const { return armature_instance && armature_display; }

	/* METHOD BINDINGS */
	static void _bind_methods();
	_DEFINE_TO_STRING()

	void for_each_armature_(const Callable &p_action);
	void for_each_armature_recursively_(const Callable &p_action, int p_current_depth = 0);

	bool has_animation(const String &p_animation_name) const;
	PackedStringArray get_animations();

	String get_current_animation_on_layer(int p_layer) const;
	String get_current_animation_in_group(const String &p_group_name) const;

	float tell_animation(const String &p_animation_name) const;
	void seek_animation(const String &p_animation_name, float p_progress);

	bool is_playing() const;
	void play(const String &p_animation_name, int p_loop_count = -1);

	void play_from_time(const String &p_animation_name, float p_time, int p_loop_count = -1);
	void play_from_progress(const String &p_animation_name, float p_progress, int p_loop_count = -1);
	void stop(const String &p_animation_name, bool b_reset = false, bool p_recursively = false);
	void stop_all_animations(bool b_reset = false, bool p_recursively = false);
	void fade_in(const String &p_animation_name, float p_time,
			int p_loop_count, int p_layer, const String &p_group, AnimFadeOutMode p_fade_out_mode);

	void reset(bool p_recursively = false);

	bool has_slot(const String &p_slot_name) const;
	Ref<DragonBonesSlot> get_slot(const String &p_slot_name);
	SlotsDictionary get_slots();

	ConstraintsDictionary get_ik_constraints();
	void set_ik_constraint(const String &p_name, Vector2 p_position);
	void set_ik_constraint_bend_positive(const String &p_name, bool p_bend_positive);

	BonesDictionary get_bones();
	Ref<DragonBonesBone> get_bone(const String &p_name);

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
	DragonBonesArmatureProxy(DragonBonesArmature *p_armature) :
			armature(p_armature) {}

private:
	static std::vector<PropertyInfo> armature_property_list;
	friend class DragonBonesArmature;

	DragonBonesArmature *armature{ nullptr };
	friend class DragonBonesArmatureDisplay;
};
#endif // TOOLS_ENABLED
} //namespace godot
