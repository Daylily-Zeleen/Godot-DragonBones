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
		DRAW_MESH = 1 << 0,
		DRAW_BONE = 1 << 1,
		DRAW_BONE_NAME = 1 << 2,
	};

	// 三种颜色和枢轴半径所有实例共用
	inline static Color color_bone{ 0.8f, 0.8f, 0.8f, 0.8f };
	inline static Color color_ik_target{ 1.0f, 0.2f, 0.1f, 0.9f };
	static void set_color_ik_bone_outline(const Color &p_color);
	static Color get_color_ik_bone_outline();
	_FORCE_INLINE_ static void set_bone_pivot_radius(float p_radius) { bone_pivot_radius = Math::max(3.0f, p_radius); }
	_FORCE_INLINE_ static float get_bone_pivot_radius() { return bone_pivot_radius; }

	DebugDraw(CanvasItem *p_owner) : owner(p_owner) {}
	~DebugDraw();

private:
	inline static float bone_pivot_radius = 5.0f;

public:
	_FORCE_INLINE_ bool is_enabled() const { return mesh_bones.is_valid(); }
	void set_enabled(bool p_enabled);

	_FORCE_INLINE_ void set_flag(Flag p_flag, bool p_enable) { p_enable ? (draw_flags |= p_flag) : (draw_flags &= ~p_flag); }
	_FORCE_INLINE_ bool has_flag(Flag p_flag) const { return draw_flags & p_flag; }

	// 只能在 CanvasItem::_draw 阶段调用。
	void draw(DragonBonesArmature *p_root_armature, const ArmatureDrawData &p_draw_data);

private:
	CanvasItem *owner;

	// 仅调试层使用的画布项。2D 网格不读 surface 材质，材质只能挂在画布项上；
	// owner 画布上还画着龙骨本体，直接挂材质会把本体一起染色，故必须单独一层。
	// 延迟到 _draw() 里创建（属性设置阶段可能尚未入树）。
	RID canvas_bones;

	// 骨骼网格：单次提交、画在 canvas_bones 上，由那里挂的材质做描边与填充。
	RID mesh_bones;

	// 线框专用网格：直接画在 owner 自身画布项上，不挂材质。
	RID mesh_wireframe;


	uint8_t draw_flags{ DRAW_MESH | DRAW_BONE | DRAW_BONE_NAME };
};

#endif // DEBUG_ENABLED

} //namespace godot
