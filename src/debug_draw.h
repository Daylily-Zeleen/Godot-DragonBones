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
// Tunables for the debug overlay, exposed on DragonBonesArmatureView as
// `debug_draw_*` properties.
struct DebugDraw {
	bool draw_mesh = true;
	bool draw_bone = true;
	bool draw_bone_name = true;

	// Radius of the pivot symbol drawn at every bone's start, in SCREEN pixels. The view
	// converts it to world units by the current canvas scale, so the glyph keeps a
	// constant apparent size instead of shrinking to nothing when the node is zoomed out
	// (or swelling across the whole armature when it is zoomed in).
	float bone_pivot_radius = 5.0f;

	// Generic bone colour. IK targets and IK-driven bones override it.
	Color color_bone{ 0.8f, 0.8f, 0.8f, 0.8f };
	Color color_ik_target{ 1.0f, 0.55f, 0.2f, 0.9f };
	Color color_ik_bone_outline{ 1.0f, 0.6f, 0.1f, 1.0f };

	DebugDraw(CanvasItem *p_owner) : owner(p_owner) {}

public:
	// 只能在 CanvasItem::_draw 阶段调用。
	void draw(DragonBonesArmature *p_root_armature, const DrawData &p_draw_data, const RID &p_debug_mesh);

private:
	CanvasItem *owner;
};

#endif // DEBUG_ENABLED

} //namespace godot
