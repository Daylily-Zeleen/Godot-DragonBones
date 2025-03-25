#pragma once

#include <rapidjson/allocators.h>
#include <godot_cpp/core/memory.hpp>

class GodotAllocator {
public:
	static const bool kNeedFree = true;
	void *Malloc(size_t size) {
		if (size) //  behavior of malloc(0) is implementation defined.
			return memalloc(size);
		else
			return NULL; // standardize to returning NULL.
	}
	void *Realloc(void *originalPtr, size_t originalSize, size_t newSize) {
		(void)originalSize;
		if (newSize == 0) {
			memfree(originalPtr);
			return NULL;
		}
		return memrealloc(originalPtr, newSize);
	}
	static void Free(void *ptr) { memfree(ptr); }
};

using GodotMemoryPoolAllocator = rapidjson::MemoryPoolAllocator<GodotAllocator>;
