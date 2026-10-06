/**************************************************************************/
/*  armature_draw_data.h                                                  */
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

#include "initialized_buffer.h"

#include <godot_cpp/classes/canvas_item_material.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/transform2d.hpp>

namespace godot {

class ArmatureDrawData {
public:
	/**
	 * @brief 单个绘制数据
	 * @note: 由于绘制数据数组是从 DragonBonesMeshDisplay 上取的，并且 DragonBonesMeshDisplay 是池化的
	 * 		所以 vertices, indices. colors, vertices_uv 这几个指针是安全的，不会变为悬垂指针。
	 */
	struct Data {
		Transform2D transform;
		const LocalVector<Vector2> *vertices;
		const LocalVector<int32_t> *indices;
		const LocalVector<Color> *colors;
		const LocalVector<Vector2> *vertices_uv;
		int64_t texture_rid;
		CanvasItemMaterial::BlendMode blend_mode;
		int z_order = 0;
#ifdef DEBUG_ENABLED
		Color debug_color;
#endif // DEBUG_ENABLED
	};

	// Every draw command sharing one z-order.
	class Layer {
		internal::InitializedBuffer<Data> data;
		int z_order = 0;

	public:
		_FORCE_INLINE_ void push_back(Data &&p_data) {
			data.push_back(std::move(p_data));
		}

		_FORCE_INLINE_ const internal::InitializedBuffer<Data> &get_data() const { return data; }

		_FORCE_INLINE_ void reinit(int p_z_order) {
			z_order = p_z_order;
			data.clear();
		}

		_FORCE_INLINE_ int get_z_order() const { return z_order; }

		_FORCE_INLINE_ size_t get_capacity_bytes() const { return data.get_capacity_bytes() + sizeof(z_order); }
	};

private:
	internal::InitializedBuffer<Layer> layers;

	_FORCE_INLINE_ uint32_t lower_bound(int p_z_order) const {
		uint32_t lo = 0, hi = layers.size();
		while (lo < hi) {
			const uint32_t mid = lo + ((hi - lo) >> 1);
			if (layers[mid].get_z_order() < p_z_order) {
				lo = mid + 1;
			} else {
				hi = mid;
			}
		}
		return lo;
	}

	// 取该 z-order 的层，不存在则按序插入。
	_FORCE_INLINE_ Layer &get_or_create_layer(int p_z_order) {
		const uint32_t at = lower_bound(p_z_order);
		if (at < layers.size() && layers[at].get_z_order() == p_z_order) {
			return layers[at];
		}

		layers.insert(at, [=](Layer &p_layer) {
			p_layer.reinit(p_z_order);
		});

		return layers[at];
	}

public:
	ArmatureDrawData() = default;
	ArmatureDrawData(const ArmatureDrawData &) = delete;
	ArmatureDrawData &operator=(const ArmatureDrawData &) = delete;

	size_t get_capacity_bytes() const {
		size_t n = layers.get_capacity_bytes();
		for (uint32_t i = 0; i < layers.get_capacity(); i++) {
			const Layer &layer = *(layers.ptr() + i);
			n += layer.get_capacity_bytes();
		}
		return n;
	}

	_FORCE_INLINE_ void begin_frame() { layers.clear(); }

	/** NOTE: 这里假定传入的 LocalVector 指针在这次绘制过程中不会变为悬垂指针  */
	_FORCE_INLINE_ void add_data(int p_z_order, const Transform2D &p_transform,
								 const LocalVector<Vector2> *p_vertices, const LocalVector<int32_t> *p_indices,
								 const LocalVector<Color> *p_colors, const LocalVector<Vector2> *p_vertices_uv,
								 int64_t p_texture_rid, CanvasItemMaterial::BlendMode p_blend_mode
#ifdef DEBUG_ENABLED
								 ,
								 const Color &p_debug_color
#endif // DEBUG_ENABLED
	) {
		DEV_ASSERT(p_vertices && p_indices && p_colors && p_vertices_uv);
		get_or_create_layer(p_z_order).push_back({
				p_transform,
				p_vertices,
				p_indices,
				p_colors,
				p_vertices_uv,
				p_texture_rid,
				p_blend_mode,
				p_z_order,
#ifdef DEBUG_ENABLED
				p_debug_color,
#endif // DEBUG_ENABLED
		});
	}

	_FORCE_INLINE_ void end_frame() {
	}

	_FORCE_INLINE_ bool is_empty() const { return layers.is_empty(); }

	// 释放全部容量（连同层次与各条命令的缓冲归还系统）。
	_FORCE_INLINE_ void reset() { layers.reset(); }

	_FORCE_INLINE_ const Layer *begin() const { return layers.ptr(); }
	_FORCE_INLINE_ const Layer *end() const { return layers.ptr() + layers.size(); }
};

} //namespace godot
