/**************************************************************************/
/*  mesh_display.h                                                        */
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

#include <dragonBones/core/BaseObject.h>
#include <godot_cpp/classes/canvas_item_material.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/templates/vmap.hpp>

namespace godot {

struct DrawData {
	Transform2D transform;
	PackedVector2Array vertices;
	PackedInt32Array indices;
	PackedColorArray colors;
	PackedVector2Array vertices_uv;
	RID texture;
	CanvasItemMaterial::BlendMode blend_mode;
	int z_order = 0;
#ifdef DEBUG_ENABLED
	Color debug_color;
#endif // DEBUG_ENABLED
};

class Display {
protected:
	class Slot_GD *slot{ nullptr };
	friend class Slot_GD;

public:
	Transform2D transform{};

	virtual void queue_redraw() const = 0;
	virtual void append_draw_data(VMap<int, LocalVector<DrawData>> &r_data, const Transform2D &p_base_transfrom = Transform2D(), const Color &p_modulate = Color(1.0f, 1.0f, 1.0f, 1.0f)) const = 0;

	virtual void release(); // NOTE: 子类要在此出处理自身的内存管理 （多继承的情况下必须用指在开头的指针才能 memdelete）
};

class DragonBonesMeshDisplay : public Display {
private:
	DragonBonesMeshDisplay(const DragonBonesMeshDisplay &);

	void fill_vertices_colors(const Color &p_color);

public:
	PackedInt32Array indices;
	PackedColorArray colors;
	PackedVector2Array vertices_uv;
	PackedVector2Array vertices;

#ifdef DEBUG_ENABLED
	Color debug_color;
#endif // DEBUG_ENABLED

public:
	DragonBonesMeshDisplay();

	void set_blend_mode(CanvasItemMaterial::BlendMode p_blend_mode) {}
	class DragonBonesArmature *get_armature() const;

	virtual void queue_redraw() const override;
	virtual void append_draw_data(VMap<int, LocalVector<DrawData>> &r_data, const Transform2D &p_base_transfrom = Transform2D(), const Color &p_modulate = Color(1.0f, 1.0f, 1.0f, 1.0f)) const override;

	virtual void release() override;

private:
	static LocalVector<DragonBonesMeshDisplay *> pool;

public:
	static DragonBonesMeshDisplay *from_pool();
	static void clear_pool();
};

} //namespace godot