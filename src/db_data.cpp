/**************************************************************************/
/*  db_data.cpp                                                           */
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

#include "db_data.h"

#include "armature.h"
#include "armature_view.h"
#include "factory.h"

#include <godot_cpp/classes/project_settings.hpp>

namespace godot {

DragonBonesData::DragonBonesData() {
}

DragonBonesData::~DragonBonesData() {
	// 可能的潜在风险：若工厂先于本资源析构，这里会访问已销毁的单例。
	// 待实测出现再处理。
	DragonBonesFactory::get_singleton()->release_data(cache_name);
}

DragonBonesArmature *DragonBonesData::build_armature(DragonBonesArmatureView *p_owner, const String &p_armature_name, const String &p_skin_name) {
	auto factory = DragonBonesFactory::get_singleton();
	ERR_FAIL_NULL_V(factory, nullptr);

	if (factory->ensure_data_loaded(this) != OK) {
		return nullptr;
	}
	return factory->create_armature(p_owner, cache_name, p_armature_name, p_skin_name);
}

PackedStringArray DragonBonesData::get_armature_names() const {
	auto factory = DragonBonesFactory::get_singleton();
	// 检查器要在资源尚未被任何 View 使用时就列出候选，故这里按需加载。
	// 这是唯一的「查询也会加载」的入口；构建路径由 build_armature 自己保证加载。
	if (factory->ensure_data_loaded(this) != OK) {
		return {};
	}
	return factory->get_armature_names(cache_name);
}

PackedStringArray DragonBonesData::get_skin_names(const String &p_armature_name) const {
	auto factory = DragonBonesFactory::get_singleton();
	if (factory->ensure_data_loaded(this) != OK) {
		return {};
	}
	return factory->get_skin_names(cache_name, p_armature_name);
}

String DragonBonesData::resolve_ske_path() const {
	if (imported) {
		return get_path().get_base_dir().path_join(ske_file);
	}
	return ske_file;
}

String DragonBonesData::resolve_texture_source_path() const {
	if (imported) {
		return get_path().get_base_dir().path_join(texture_source);
	}
	return texture_source;
}

void DragonBonesData::set_texture_source_type(TextureSourceType p_type) {
	if (texture_source_type == p_type) {
		return;
	}
	texture_source_type = p_type;
#ifdef TOOLS_ENABLED
	notify_property_list_changed();
#endif // TOOLS_ENABLED
}

void DragonBonesData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("build_armature", "owner", "armature_name", "skin_name"), &DragonBonesData::build_armature);
	ClassDB::bind_method(D_METHOD("get_armature_names"), &DragonBonesData::get_armature_names);
	ClassDB::bind_method(D_METHOD("get_skin_names", "armature_name"), &DragonBonesData::get_skin_names);

	ClassDB::bind_method(D_METHOD("resolve_ske_path"), &DragonBonesData::resolve_ske_path);
	ClassDB::bind_method(D_METHOD("resolve_texture_source_path"), &DragonBonesData::resolve_texture_source_path);

	ClassDB::bind_method(D_METHOD("set_ske_file", "ske_file"), &DragonBonesData::set_ske_file);
	ClassDB::bind_method(D_METHOD("get_ske_file"), &DragonBonesData::get_ske_file);
	ClassDB::bind_method(D_METHOD("set_texture_source_type", "texture_source_type"), &DragonBonesData::set_texture_source_type);
	ClassDB::bind_method(D_METHOD("get_texture_source_type"), &DragonBonesData::get_texture_source_type);
	ClassDB::bind_method(D_METHOD("set_texture_source", "texture_source"), &DragonBonesData::set_texture_source);
	ClassDB::bind_method(D_METHOD("get_texture_source"), &DragonBonesData::get_texture_source);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "ske_file", PROPERTY_HINT_FILE, "*.json,*.dbbin"), "set_ske_file", "get_ske_file");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "texture_source_type", PROPERTY_HINT_ENUM, "Texture Atlas,Scattered Images"), "set_texture_source_type", "get_texture_source_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "texture_source", PROPERTY_HINT_FILE, "*.json"), "set_texture_source", "get_texture_source");

	BIND_ENUM_CONSTANT(TEXTURE_SOURCE_ATLAS);
	BIND_ENUM_CONSTANT(TEXTURE_SOURCE_SCATTERED);
}

#ifdef DEBUG_ENABLED
void DragonBonesData::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == SNAME("texture_source")) {
		if (texture_source_type == TEXTURE_SOURCE_SCATTERED) {
			p_property.hint = PROPERTY_HINT_DIR;
			p_property.hint_string = "";
		} else {
			p_property.hint = PROPERTY_HINT_FILE;
			p_property.hint_string = "*.json";
		}
	}

	if (imported) {
		if (p_property.name == SNAME("ske_file") ||
			p_property.name == SNAME("texture_source_type") ||
			p_property.name == SNAME("texture_source")) {
			p_property.usage |= PROPERTY_USAGE_READ_ONLY;
		}
	}
}
#endif // DEBUG_ENABLED

} //namespace godot
