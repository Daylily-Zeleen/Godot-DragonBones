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

#include <godot_cpp/classes/canvas_item_material.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/rid.hpp>
#include <godot_cpp/variant/transform2d.hpp>

namespace godot {

// Draw commands for one frame, bucketed by z-order and kept sorted by it.
//
// 逐帧复用：容器本体与各条命令的 Packed*Array 缓冲跨帧保留，本帧只复位写入游标
// 并就地覆写。因为 Packed*Array 缩到 0 会释放缓冲，用量用 used 游标表示，
// 绝不能靠 size()。
class ArmatureDrawData {
public:
	// A single draw command: a mesh plus the transform and material to draw it with.
	struct Data {
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

	// Every draw command sharing one z-order.
	struct Layer {
		int z_order = 0;
		LocalVector<Data> data;
		// 本帧已写入的条目数（<= data.size()）。跨帧保留 data 及其缓冲，只复位游标。
		uint32_t used = 0;
	};

private:
	LocalVector<Layer> layers;

	_FORCE_INLINE_ uint32_t lower_bound(int p_z_order) const {
		uint32_t lo = 0, hi = layers.size();
		while (lo < hi) {
			const uint32_t mid = lo + ((hi - lo) >> 1);
			if (layers[mid].z_order < p_z_order) {
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
		if (at < layers.size() && layers[at].z_order == p_z_order) {
			return layers[at];
		}
		layers.insert(at, Layer{ p_z_order, LocalVector<Data>(), 0 });
		return layers[at];
	}

public:
	ArmatureDrawData() = default;
	ArmatureDrawData(const ArmatureDrawData &) = delete;
	ArmatureDrawData &operator=(const ArmatureDrawData &) = delete;

	// 帧开始：只复位写入游标。**不析构 Layer/Data、不释放缓冲**，供本帧就地覆写。
	_FORCE_INLINE_ void begin_frame() {
		for (Layer &layer : layers) {
			layer.used = 0;
		}
	}

	// 追加一条绘制命令。复用已有槽位就地覆写；仅当本帧条目多于上帧时才增长。
	_FORCE_INLINE_ void add_data(int p_z_order, const Transform2D &p_transform,
				  const PackedVector2Array &p_vertices, const PackedInt32Array &p_indices,
				  const PackedColorArray &p_colors, const PackedVector2Array &p_vertices_uv,
				  RID p_texture, CanvasItemMaterial::BlendMode p_blend_mode
#ifdef DEBUG_ENABLED
				  ,
				  const Color &p_debug_color
#endif // DEBUG_ENABLED
	) {
		Layer &layer = get_or_create_layer(p_z_order);
		if (layer.used < layer.data.size()) {
			Data &d = layer.data[layer.used];
			d.transform = p_transform;
			d.vertices = p_vertices;
			d.indices = p_indices;
			d.colors = p_colors;
			d.vertices_uv = p_vertices_uv;
			d.texture = p_texture;
			d.blend_mode = p_blend_mode;
			d.z_order = p_z_order;
#ifdef DEBUG_ENABLED
			d.debug_color = p_debug_color;
#endif // DEBUG_ENABLED
		} else {
			layer.data.push_back({
					p_transform,
					p_vertices,
					p_indices,
					p_colors,
					p_vertices_uv,
					p_texture,
					p_blend_mode,
					p_z_order,
#ifdef DEBUG_ENABLED
					p_debug_color,
#endif // DEBUG_ENABLED
			});
		}
		layer.used++;
	}

	// 帧结束：把各层 data 截到实际写入的条数（resize 保留缓冲），并丢弃空层。
	_FORCE_INLINE_ void end_frame() {
		uint32_t w = 0;
		for (uint32_t i = 0; i < layers.size(); ++i) {
			Layer &layer = layers[i];
			layer.data.resize(layer.used); // 只改 count，缓冲区保留
			if (layer.used == 0) {
				continue; // 该 z-order 本帧无内容，丢弃该层
			}
			if (w != i) {
				layers[w] = std::move(layers[i]);
			}
			++w;
		}
		layers.resize(w);
	}

	_FORCE_INLINE_ bool is_empty() const { return layers.is_empty(); }

	// 释放全部容量（连同层次与各条命令的缓冲归还系统）。只在确认长期不再需要时调用。
	_FORCE_INLINE_ void reset() { layers.reset(); }

	_FORCE_INLINE_ const Layer &operator[](uint32_t p_index) const { return layers[p_index]; }

	_FORCE_INLINE_ const Layer *begin() const { return layers.ptr(); }
	_FORCE_INLINE_ const Layer *end() const { return layers.ptr() + layers.size(); }
};

} //namespace godot
