#pragma once

#include <dragonBones/armature/Slot.h>
#include <godot_cpp/classes/canvas_item_material.hpp>
#include <godot_cpp/classes/texture2d.hpp>

#include "mesh_display.h"
#include "utils.h"

namespace godot {

class Slot_GD : public dragonBones::Slot {
	BIND_CLASS_TYPE_A(Slot_GD);

private:
	float _textureScale;

public:
	CanvasItemMaterial::BlendMode blend_mode{ CanvasItemMaterial::BLEND_MODE_MIX };

	Ref<Texture2D> get_texture() const;
	// void update_display_texture() const;

	Display *get_display() const { return static_cast<Display *>(getDisplay()); }

public:
	virtual void _updateVisible() override;
	virtual void _updateBlendMode() override;
	virtual void _updateColor() override;

protected:
	virtual void _initDisplay(void *value, bool isRetain) override;
	virtual void _disposeDisplay(void *value, bool isRelease) override;
	virtual void _onUpdateDisplay() override;
	virtual void _addDisplay() override;
	virtual void _replaceDisplay(void *value, bool isArmatureDisplay) override;
	virtual void _removeDisplay() override; // 不被调用的纯虚函数
	virtual void _updateZOrder() override;

	virtual void _updateFrame() override;
	virtual void _updateMesh() override;
	virtual void _updateTransform() override;
	virtual void _identityTransform() override;

	virtual void _onClear() override;

	void __get_uv_pt(Point2 &_pt, bool _is_rot, float _u, float _v, const dragonBones::Rectangle &_reg, const dragonBones::TextureAtlasData *_p_atlas);
};

class DragonBonesSlot : public RefCounted {
	GDCLASS(DragonBonesSlot, RefCounted);

private:
	Slot_GD *slot{ nullptr }; // 生命周期由 dragonBones::Armature 管理

	friend class DragonBonesFactory;
	friend class DragonBonesArmature;

public:
	DragonBonesSlot() = default;
	DragonBonesSlot(Slot_GD *p_slot) :
			slot(p_slot) {}

	virtual ~DragonBonesSlot() = default;

public:
	/* BIND METHODS */
	static void _bind_methods();
	_DEFINE_TO_STRING()

	Color get_display_color_multiplier();
	void set_display_color_multiplier(const Color &_color);
	void set_display_index(int index = 0);
	void set_display_by_name(const String &_name);
	int get_display_index();
	int get_display_count();
	void next_display();
	void previous_display();
	String get_slot_name();

	class DragonBonesArmature *get_child_armature();
};

} //namespace godot
