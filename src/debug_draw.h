/**************************************************************************/
/*  debug_draw.h                                                          */
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

#include "armature.h"

#include <godot_cpp/classes/canvas_item.hpp>

namespace godot {

#ifdef DEBUG_ENABLED
struct DebugDraw {
	enum Flag : uint8_t {
		DRAW_ENABLED = 1 << 0,
		DRAW_MESH = 1 << 1,
		DRAW_BONE = 1 << 2,
		DRAW_BONE_NAME = 1 << 3,
	};

	Color color_bone{ 0.8f, 0.8f, 0.8f, 0.8f };
	Color color_ik_target{ 1.0f, 0.55f, 0.2f, 0.9f };
	Color color_ik_bone_outline{ 1.0f, 0.6f, 0.1f, 1.0f };

	DebugDraw(CanvasItem *p_owner) : owner(p_owner) {}

public:
	_FORCE_INLINE_ void set_flag(Flag p_flag, bool p_enable) { p_enable ? (draw_flags |= p_flag) : (draw_flags &= ~p_flag); }
	_FORCE_INLINE_ bool has_flag(Flag p_flag) const { return draw_flags & p_flag; }

	_FORCE_INLINE_ void set_bone_pivot_radius(float p_radius) { bone_pivot_radius = Math::max(3.0f, p_radius); }
	_FORCE_INLINE_ float get_bone_pivot_radius() const { return bone_pivot_radius; }

	// 只能在 CanvasItem::_draw 阶段调用。
	void draw(DragonBonesArmature *p_root_armature, const DrawData &p_draw_data, const RID &p_debug_mesh);

	// TODO: 是否会有在运行时替换嵌套的 Armature 的情况？有的话也需要调用清除
	void clear_cache();

private:
	LocalVector<StringName> ik_targets;
	LocalVector<StringName> ik_driven;

	CanvasItem *owner;

	float bone_pivot_radius = 5.0f;

	uint8_t draw_flags{ DRAW_MESH | DRAW_BONE | DRAW_BONE_NAME };
	bool cached = false;

	void cache_ik_bones(DragonBonesArmature *p_armature);
};

#endif // DEBUG_ENABLED

} //namespace godot
