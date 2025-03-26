#pragma once

#include <godot_cpp/variant/string.hpp>
#include <string>

///////////////////////////////////////////////////////////////////////////////
#define DRAGONBONES_MALLOC(size) memalloc(size)
#define DRAGONBONES_REALLOC(ptr, new_size) memrealloc(ptr, new_size)
#define DRAGONBONES_FREE(ptr) memfree(ptr)

#define DRAGONBONES_NEW(T) memnew(T)
#define DRAGONBONES_DELETE(ptr) godot::memdelete(ptr)

#define DRAGONBONES_NEW_ARR(T) godot::memnew_arr(T)
#define DRAGONBONES_DELETE_ARR(ptr) godot::memdelete_arr(ptr)

///////////////////////////////////////////////////////////////////////////////
#define RAPIDJSON_MALLOC(size) DRAGONBONES_MALLOC(size)
#define RAPIDJSON_REALLOC(ptr, new_size) DRAGONBONES_REALLOC(ptr, new_size)
#define RAPIDJSON_FREE(ptr) DRAGONBONES_FREE(ptr)

#define RAPIDJSON_NEW(TypeName) DRAGONBONES_NEW(TypeName())
#define RAPIDJSON_DELETE(x) DRAGONBONES_DELETE(x)

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
