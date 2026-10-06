/**************************************************************************/
/*  initialized_buffer.h                                                  */
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

#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/memory.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>

namespace godot {

namespace internal {

// 与 LocalVector 的关键差异：**整个 [0, capacity) 缓冲区恒为已构造的有效对象**，
// count 只是逻辑用量游标。因此：
//   - resize() / clear() 只改 count，**不析构、不释放**（这就是它存在、而不用 LocalVector 的理由）；
//   - 只有 reset() 释放整块缓冲时才析构全部元素；
//   - reset() 后回到初始态：data == nullptr && capacity == 0 && count == 0；
//   - count <= capacity 恒成立；insert() 要求 p_at <= count（p_at == count 为尾插）。
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
	_FORCE_INLINE_ void resize(U p_size) {
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
		count = 0;
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
		CRASH_BAD_INDEX(p_at, count + 1); // 允许 p_at == count（尾插）
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

} //namespace godot
