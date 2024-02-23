#pragma once

#include "dragonBones/armature/Slot.h"
#include "wrappers/GDDisplay.h"


namespace godot {

class Slot_GD : public dragonBones::Slot {
	BIND_CLASS_TYPE_A(Slot_GD);

private:
	float _textureScale;
	GDDisplay *_renderDisplay;

	Ref<Texture2D> get_display_texture() const;

public:
	void update_display_texutre() const;

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
	virtual void _removeDisplay() override;
	virtual void _updateZOrder() override;

	virtual void _updateFrame() override;
	virtual void _updateMesh() override;
	virtual void _updateTransform() override;
	virtual void _identityTransform() override;

	virtual void _onClear() override;

	void __get_uv_pt(Point2 &_pt, bool _is_rot, float _u, float _v, const dragonBones::Rectangle &_reg, const dragonBones::TextureAtlasData *_p_atlas);
};

// TODO: 是否需要对每个Slot单独设置继承父级材质？
class DragonBonesSlot : public RefCounted {
	GDCLASS(DragonBonesSlot, RefCounted);

private:
	Slot_GD *slot{ nullptr }; // 生命周期由 dragonBones::ArmatureData 管理

	friend class DragonBonesFactory;

public:
	DragonBonesSlot() = default;
	DragonBonesSlot(Slot_GD *p_slot) :
			slot(p_slot) {}

	virtual ~DragonBonesSlot() = default;

public:
	/* BIND METHODS */
	static void _bind_methods();

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
