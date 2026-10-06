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

namespace internal {

template <typename T, typename U = uint32_t, bool tight = false>
class InitializedBuffer {
	T *data{ nullptr };
	U capacity{ 0 };
	U count{ 0 };

public:
	InitializedBuffer() = default;
	InitializedBuffer(const InitializedBuffer &) = delete;
	InitializedBuffer(InitializedBuffer &&) = delete;

	~InitializedBuffer() { reset(); }

	_FORCE_INLINE_ T *ptr() const { return data; }
	_FORCE_INLINE_ bool is_empty() const { return count == 0; }
	_FORCE_INLINE_ U size() const { return count; }
	_FORCE_INLINE_ U get_capacity() const { return capacity; }
	_FORCE_INLINE_ size_t get_capacity_bytes() const { return capacity * sizeof(T); }
	_FORCE_INLINE_ void reserve(U p_size) {
		if (p_size <= capacity) {
			return;
		}

		U prev_capacity = capacity;
		if (tight) {
			capacity = p_size;
		} else {
			// Try 1.5x the current capacity.
			// This ratio was chosen because it is close to the ideal growth rate of the golden ratio.
			// See https://archive.ph/Z2R8w for details.
			capacity = MAX((U)2, capacity + ((1 + capacity) >> 1));
			// If 1.5x growth isn't enough, just use the needed size exactly.
			if (p_size > capacity) {
				capacity = p_size;
			}
		}
		data = (T *)memrealloc(data, capacity * sizeof(T));
		CRASH_COND_MSG(!data, "Out of memory");
		memnew_arr_placement(data + prev_capacity, capacity - prev_capacity);
	}
	_FORCE_INLINE_ U resize(U p_size) {
		if (p_size > capacity) {
			reserve(p_size);
		}
		count = p_size;
	}
	_FORCE_INLINE_ const T &operator[](U idx) const {
		CRASH_BAD_INDEX(idx, count);
		return data[idx];
	}
	_FORCE_INLINE_ T &operator[](U idx) {
		CRASH_BAD_INDEX(idx, count);
		return data[idx];
	}
	_FORCE_INLINE_ void clear() {
		count = 0;
	}
	_FORCE_INLINE_ void reset() {
		if (!std::is_trivially_destructible_v<T>) {
			for (U i = 0; i < capacity; i++) {
				data[i].~T();
			}
		}
		if (data) {
			memfree(data);
			data = nullptr;
		}
		capacity = 0;
	}
	template <typename Func, std::enable_if_t<std::is_invocable_v<Func, T &>> *_dummy = nullptr>
	_FORCE_INLINE_ void push_back(Func &&p_initializer) {
		if (count >= capacity) {
			reserve(count + 1);
		}
		count++;
		T &val = data[count - 1];
		p_initializer(val);
	}
	template <typename Func, std::enable_if_t<std::is_invocable_v<Func, T &>> *_dummy = nullptr>
	_FORCE_INLINE_ void insert(U p_at, Func &&p_initializer) {
		if (count >= capacity) {
			reserve(count + 1);
		}

		// 插入操作：
		//	1. 将要被挤掉的最后一个的内存拷贝出来
		uint8_t buff[sizeof(T)];
		memcpy(buff, data + count, sizeof(T));

		//	2. 从 at 开始整体向后移动 1 个（memcpy）
		uint8_t *dest = (uint8_t *)(data + p_at + 1);
		const uint8_t *src = (uint8_t *)(data + p_at);
		const size_t bytes = (size() - p_at) * sizeof(T);
		memmove(dest, src, bytes);

		//	3. 将 1 拷贝出来的内存拷贝回 at 处，实现交换
		memcpy((uint8_t *)(data + p_at), buff, sizeof(T));

		count++;
		p_initializer(data[p_at]);
	}
	template <typename _Placeholder = void>
	_FORCE_INLINE_ void push_back(T &&p_val) {
		push_back([&](T &v) { v = std::move(p_val); });
	}
	template <typename _Placeholder = void>
	_FORCE_INLINE_ void push_back(const T &p_val) {
		push_back([&](T &v) { v = p_val; });
	}
	template <typename _Placeholder = void>
	_FORCE_INLINE_ void insert(U p_at, T &&p_val) {
		insert(p_at, [&](T &v) { v = std::move(p_val); });
	}
	template <typename _Placeholder = void>
	_FORCE_INLINE_ void insert(U p_at, const T &p_val) {
		insert(p_at, [&](T &v) { v = p_val; });
	}

	_FORCE_INLINE_ T *begin() { return data; }
	_FORCE_INLINE_ const T *begin() const { return data; }
	_FORCE_INLINE_ const T *end() const { return data + size(); }
};

} //namespace internal

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
			n += sizeof(Data) * layer.get_capacity_bytes();
		}
		return n;
	}

	_FORCE_INLINE_ void begin_frame() { layers.clear(); }

	// 追加一条绘制命令。复用已有槽位就地覆写；仅当本帧条目多于上帧时才增长。
	_FORCE_INLINE_ void add_data(int p_z_order, const Transform2D &p_transform,
								 const LocalVector<Vector2> &p_vertices, const LocalVector<int32_t> &p_indices,
								 const LocalVector<Color> &p_colors, const LocalVector<Vector2> &p_vertices_uv,
								 int64_t p_texture_rid, CanvasItemMaterial::BlendMode p_blend_mode
#ifdef DEBUG_ENABLED
								 ,
								 const Color &p_debug_color
#endif // DEBUG_ENABLED
	) {
		get_or_create_layer(p_z_order).push_back({
				p_transform,
				&p_vertices,
				&p_indices,
				&p_colors,
				&p_vertices_uv,
				p_texture_rid,
				p_blend_mode,
				p_z_order,
#ifdef DEBUG_ENABLED
				p_debug_color,
#endif // DEBUG_ENABLED
		});
	}

	// 帧结束：把各层 data 截到实际写入的条数（resize 保留缓冲），并丢弃空层。
	_FORCE_INLINE_ void end_frame() {
	}

	_FORCE_INLINE_ bool is_empty() const { return layers.is_empty(); }

	// 释放全部容量（连同层次与各条命令的缓冲归还系统）。
	_FORCE_INLINE_ void reset() { layers.reset(); }

	_FORCE_INLINE_ const Layer *begin() const { return layers.ptr(); }
	_FORCE_INLINE_ const Layer *end() const { return layers.ptr() + layers.size(); }
};

} //namespace godot
