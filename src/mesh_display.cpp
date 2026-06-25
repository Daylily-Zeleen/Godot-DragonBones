/**************************************************************************/
/*  mesh_display.cpp                                                      */
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

#include "mesh_display.h"

#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include "armature.h"
#include "godot_cpp/core/memory.hpp"

namespace godot {

void Display::release() {
	transform = Transform2D();
	slot = nullptr; // 不只需要置空，需要回池 （销毁操作是由 slot 触发）
}

DragonBonesMeshDisplay::DragonBonesMeshDisplay()
#ifdef DEBUG_ENABLED
		:
		debug_color{
			static_cast<float>(UtilityFunctions::randf_range(0.5f, 1.0f)),
			static_cast<float>(UtilityFunctions::randf_range(0.3f, 1.0f)),
			static_cast<float>(UtilityFunctions::randf_range(0.3f, 1.0f)),
			1.0f,
		}
#endif // DEBUG_ENABLED
{
}

void DragonBonesMeshDisplay::fill_vertices_colors(const Color &p_color) {
	colors.fill(p_color);
}

DragonBonesArmature *DragonBonesMeshDisplay::get_armature() const {
	ERR_FAIL_NULL_V(slot->getArmature()->getProxy(), nullptr);
	return static_cast<DragonBonesArmature *>(slot->getArmature()->getProxy());
}

void DragonBonesMeshDisplay::queue_redraw() const {
	ERR_FAIL_NULL(get_armature());
	get_armature()->queue_redraw();
}

void DragonBonesMeshDisplay::append_draw_data(VMap<int, LocalVector<DrawData>> &r_data, const Transform2D &p_base_transfrom, const Color &p_modulate) const {
	if (!slot->getVisible()) {
		return;
	}

	const_cast<DragonBonesMeshDisplay *>(this)->fill_vertices_colors(slot->color * p_modulate);

	if (!r_data.has(slot->_zOrder)) {
		r_data.insert(slot->_zOrder, LocalVector<DrawData>());
	}
	auto armature = get_armature();

	RID texture;
	if (armature && armature->get_texture_override().is_valid()) {
		texture = armature->get_texture_override()->get_rid();
	} else if (slot->get_texture().is_valid()) {
		texture = slot->get_texture()->get_rid();
	}

	r_data[slot->_zOrder].push_back({
			p_base_transfrom * transform,
			vertices,
			indices,
			colors,
			vertices_uv,
			texture,
			slot->blend_mode,
			slot->_zOrder,
#ifdef DEBUG_ENABLED
			debug_color,
#endif // DEBUG_ENABLED
	});
}

void DragonBonesMeshDisplay::release() {
	Display::release();

	indices.clear();
	colors.clear();
	vertices_uv.clear();
	vertices.clear();

	// DEBUG: 清理完后回池, 待移除
	if (pool.has(this)) {
		ERR_FAIL_COND_MSG(pool.has(this), "DragonBonesMeshDisplay instance already in pool");
	}
	pool.push_back(this);
}

LocalVector<DragonBonesMeshDisplay *> DragonBonesMeshDisplay::pool{};

DragonBonesMeshDisplay *DragonBonesMeshDisplay::from_pool() {
	if (pool.is_empty()) {
		return memnew(DragonBonesMeshDisplay);
	}
	auto ret = pool[pool.size() - 1];
	pool.resize(pool.size() - 1);
	return ret;
}
void DragonBonesMeshDisplay::clear_pool() {
	for (auto obj : pool) {
		memdelete(obj);
	}
	pool.clear();
}

} //namespace godot