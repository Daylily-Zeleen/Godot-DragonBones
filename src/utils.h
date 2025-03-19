#pragma once

#include "godot_cpp/variant/string.hpp"
#include <string>

namespace godot {

_FORCE_INLINE_ String to_gd_str(const std::string &p_std_str) {
	return String::utf8(p_std_str.c_str());
}

_FORCE_INLINE_ std::string to_std_str(const String &p_gd_str) {
	return p_gd_str.utf8().get_data();
}

#define _DEFINE_TO_STRING() \
	::godot::String _to_string() const { return vformat("<%s#%s>", get_class_static(), get_instance_id()); }

}; //namespace godot
