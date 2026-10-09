/**************************************************************************/
/*  db_data.h                                                             */
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

#include <godot_dragon_bones.h>

#include <dragonBones/core/DragonBones.h>
#include <godot_cpp/classes/resource.hpp>

namespace godot {

class DragonBonesArmature;
class DragonBonesArmatureView;
class DragonBonesDataFileProcessor;

class DragonBonesData : public Resource {
	GDCLASS(DragonBonesData, Resource)

public:
	_FORCE_INLINE_ const std::string &get_cache_name() const { return cache_name; }

	// 资源文件扩展名。引擎按扩展名选择 loader/saver，故必须注册对应的
	// ResourceFormatLoader/SaverDragonBonesData（见 factory.h 的说明）。
	static constexpr char SAVED_EXT[] = "dbdata";

	enum TextureSourceType {
		// texture_source 指向 *_tex.json。
		TEXTURE_SOURCE_ATLAS = 0,
		// texture_source 指向 *_texture/ 目录（散图）。
		TEXTURE_SOURCE_SCATTERED = 1,
	};

private:
	const std::string cache_name{ vformat("dbdata:0x%x", (uint64_t)(uintptr_t)this).utf8().get_data() };

	// 骨架文件。自动导入时为文件名，手动创建时为完整路径（见 resolve_ske_path）。
	String ske_file;
	// 图集 json 或散图目录。语义同 ske_file（由 texture_source_type 决定是文件还是目录）。
	String texture_source;
	TextureSourceType texture_source_type{ TEXTURE_SOURCE_ATLAS };

	bool imported{ false };

protected:
	static void _bind_methods();
	_DEFINE_TO_STRING()

#ifdef DEBUG_ENABLED
	void _validate_property(PropertyInfo &p_property) const;
#endif // DEBUG_ENABLED

public:
	DragonBonesData();
	~DragonBonesData() override;
 
	// 用本资源构建一个骨架实例（内部先确保数据已加载）。
	DragonBonesArmature *build_armature(DragonBonesArmatureView *p_owner, const String &p_armature_name, const String &p_skin_name);

	PackedStringArray get_armature_names() const;
	PackedStringArray get_skin_names(const String &p_armature_name) const;

	String resolve_ske_path() const;
	String resolve_texture_source_path() const;
	// ---- 属性 ----

	void set_ske_file(const String &p_file) { ske_file = p_file; }
	String get_ske_file() const { return ske_file; }

	void set_texture_source_type(TextureSourceType p_type);
	TextureSourceType get_texture_source_type() const { return texture_source_type; }

	void set_texture_source(const String &p_source) { texture_source = p_source; }
	String get_texture_source() const { return texture_source; }

	bool is_imported() const { return imported; }

private:
	friend class DragonBonesDataFileProcessor;
	friend class DragonBonesImportPlugin;
};

} //namespace godot

VARIANT_ENUM_CAST(godot::DragonBonesData::TextureSourceType);

// 本插件有两套同名类型：本资源的 godot::DragonBonesData，与运行时模型的
// dragonBones::DragonBonesData。同时 using 两个 namespace 的翻译单元会歧义，
// 故在此提供两个别名明确区分（本头文件未 using 任一 namespace，别名自身不会歧义）。
using GDDragonBonesData = godot::DragonBonesData;
using DBDragonBonesData = dragonBones::DragonBonesData;
