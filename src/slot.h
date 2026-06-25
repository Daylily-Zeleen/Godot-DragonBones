/**************************************************************************/
/*  slot.h                                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                           Godot-DragonBones                            */
/*        https://github.com/Daylily-Zeleen/Godot-DragonBones             */
/**************************************************************************/
/* Copyright (c) 2024-present 忘忧の (Daylily-Zeleen)                      */
/*               - Contact: daylily-zeleen@foxmail.com                    */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include <godot_dragon_bones.h>

#include <dragonBones/armature/Slot.h>
#include <godot_cpp/classes/canvas_item_material.hpp>
#include <godot_cpp/classes/texture2d.hpp>

#include "mesh_display.h"

namespace godot {

class Slot_GD : public dragonBones::Slot {
	BIND_CLASS_TYPE_A(Slot_GD);

private:
	Ref<class DragonBonesSlot> wrapper;
	float _textureScale;

	friend class DragonBonesFactory;

public:
	CanvasItemMaterial::BlendMode blend_mode{ CanvasItemMaterial::BLEND_MODE_MIX };
	Color color; // 直接对该变量进行设置，跳过基类的 _setColor()/colorTransform

	Ref<Texture2D> get_texture() const;
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

	friend class Slot_GD;
	friend class DragonBonesFactory;
	friend class DragonBonesArmature;

public:
	DragonBonesSlot() = default;
	DragonBonesSlot(Slot_GD *p_slot) :
			slot(p_slot) {}

public:
	/* BIND METHODS */
	static void _bind_methods();
	_DEFINE_TO_STRING()

	Color get_display_color_multiplier();
	void set_display_color_multiplier(const Color &p_color);
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
