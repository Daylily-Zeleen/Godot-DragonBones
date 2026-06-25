/**************************************************************************/
/*  godot_dragon_bones.h                                                  */
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

#include <godot_cpp/variant/string.hpp>
#include <string>

///////////////////////////////////////////////////////////////////////////////
#ifndef GODOT_DRAGONBONES
#define GODOT_DRAGONBONES

#define DRAGONBONES_MALLOC(size) memalloc(size)
#define DRAGONBONES_REALLOC(ptr, new_size) memrealloc(ptr, new_size)
#define DRAGONBONES_FREE(ptr) memfree(ptr)

#define DRAGONBONES_NEW(T) memnew(T)
#define DRAGONBONES_DELETE(ptr) godot::memdelete(ptr)

#define DRAGONBONES_NEW_ARR(T, size) godot::memnew_arr(T, size)
#define DRAGONBONES_DELETE_ARR(ptr) godot::memdelete_arr(ptr)

///////////////////////////////////////////////////////////////////////////////
#define RAPIDJSON_MALLOC(size) DRAGONBONES_MALLOC(size)
#define RAPIDJSON_REALLOC(ptr, new_size) DRAGONBONES_REALLOC(ptr, new_size)
#define RAPIDJSON_FREE(ptr) DRAGONBONES_FREE(ptr)

#define RAPIDJSON_NEW(TypeName) DRAGONBONES_NEW(TypeName())
#define RAPIDJSON_DELETE(x) DRAGONBONES_DELETE(x)

#endif // GODOT_DRAGONBONES
///////////////////////////////////////////////////////////////////////////////

namespace godot {
_FORCE_INLINE_ String to_gd_str(const std::string &p_std_str) {
	return String::utf8(p_std_str.c_str());
}

_FORCE_INLINE_ std::string to_std_str(const String &p_gd_str) {
	return p_gd_str.utf8().get_data();
}
}; //namespace godot

#define _DEFINE_TO_STRING() \
	::godot::String _to_string() const { return ::godot::vformat("<%s#%s>", get_class_static(), get_instance_id()); }

#define SNAME(sn) ([] {static const ::godot::StringName ret{sn}; return ret; }())
