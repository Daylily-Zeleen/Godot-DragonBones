/**************************************************************************/
/*  factory.h                                                             */
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

#include <dragonBones/factory/BaseFactory.h>

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_format_loader.hpp>
#include <godot_cpp/classes/resource_format_saver.hpp>

#include <string>

#include "db_data.h"

namespace godot {

class DragonBonesArmature;
class DragonBonesArmatureView;

// 全局数据工厂
class DragonBonesFactory : private dragonBones::BaseFactory {
	PackedByteArray get_file_data(const String &p_file) const;

	class DragonBonesArmatureView *building_armature{ nullptr };

public:
	// ---- 单例 ----

	static DragonBonesFactory *get_singleton();

	DragonBonesFactory();
	~DragonBonesFactory();

	// ---- 导入路径工具 ----

	static String get_imported_file_name(const String &p_path) { return p_path.md5_text() + ".dbimport"; }
	static String convert_to_imported_path(const String &p_path) {
		bool use_hidden_directory = ProjectSettings::get_singleton()->get_setting_with_override("application/config/use_hidden_project_data_directory");
		return vformat("res://%s/imported/%s", use_hidden_directory ? ".godot" : "godot", get_imported_file_name(p_path));
	}

	// ---- 数据装载 / 卸载 ----
	// 确保该资源描述的那套数据已解析并注册到本工厂。幂等：已加载则直接返回 OK。
	// 接受裸指针：工厂不持有资源（持有 Ref 会让资源永不析构，而卸载由资源析构驱动）。
	Error ensure_data_loaded(const DragonBonesData *p_data); // DragonBonesData = godot 资源（本头未 using dragonBones）
	// 卸载某个缓存键对应的全部运行时数据（骨架数据 + 图集 + 纹理引用）。
	void release_data(const std::string &p_cache_name);

	// ---- 查询（供 View 构造检查器候选列表）----
	PackedStringArray get_armature_names(const std::string &p_cache_name) const;
	PackedStringArray get_skin_names(const std::string &p_cache_name, const String &p_armature_name) const;

	// ---- 构建 ----
	DragonBonesArmature *create_armature(DragonBonesArmatureView *p_owner, const std::string &p_cache_name = "", const String &p_armature_name = "", const String &p_skin_name = "");

	static constexpr char SRC_JSON_EXT[] = "json";
	static constexpr char SRC_BIN_EXT[] = "dbbin";

protected:
	dragonBones::DragonBonesData *loadDragonBonesData(const char *p_data_loaded, const std::string &p_name = "");
	dragonBones::TextureAtlasData *loadTextureAtlasData(const char *p_data_loaded, void *p_atlas_data_file_path, const std::string &p_name = "", float p_scale = 1.0f);
	class DragonBonesArmature *buildArmatureDisplay(const std::string &p_armature_name, const std::string &p_dragon_bones_name, const std::string &p_skinName = "", const std::string &p_texture_atlas_name = "") const;

	// 按某套的骨架数据收集其显示对象引用的纹理名（散图用）。
	LocalVector<std::string> collect_display_paths(dragonBones::DragonBonesData *p_data) const;

	virtual dragonBones::TextureAtlasData *_buildTextureAtlasData(dragonBones::TextureAtlasData *textureAtlasData, void *textureAtlas) const override;
	virtual dragonBones::Armature *_buildArmature(const dragonBones::BuildArmaturePackage &dataPackage) const override;
	virtual dragonBones::Slot *_buildSlot(const dragonBones::BuildArmaturePackage &dataPackage, const dragonBones::SlotData *slotData, dragonBones::Armature *armature) const override;
	virtual dragonBones::Armature *_buildChildArmature(const dragonBones::BuildArmaturePackage *dataPackage, dragonBones::Slot *slot, dragonBones::DisplayData *displayData) const override;
	virtual void _buildBones(const dragonBones::BuildArmaturePackage &dataPackage, dragonBones::Armature *armature) const override;

private:
	// 加载一套骨架数据 + 其纹理源（图集 json 或散图目录）。
	Error load_data(const DragonBonesData *p_data);

	friend class DragonBonesExportPlugin;
	friend class DragonBonesImportPlugin;
	friend class DragonBonesEditorPlugin;
	friend class DragonBonesArmatureView;
};

// ---- *.dbdata 资源格式读写 ----
//
// 为什么需要它们：引擎内置的两个 saver 的 recognize() 恒为 true，但只认
// tres/tscn（text）与 <base_extension>/res（binary）扩展名；ResourceSaver 按
// 路径扩展名选择 saver，故不注册就无法写出 .dbdata 扩展名（引擎未给
// GDExtension 暴露 RES_BASE_EXTENSION 之类的自定义 base 扩展名机制）。
//
// 格式沿用原先 .dbfactory 的 ConfigFile 文本，字段精简为：
//   [properties] ske_file / texture_source_type / texture_source
//   [other]      imported
//   顶层         VERSION / UID
// *.dbdata 的字段读写。做成基类而非自由函数，是因为要把 imported 标记的写入权
// 精确授予「序列化」这一职责（自由函数拿不到 friend 权限），与原有
// DragonBonesFactoryFileProcessor 的模式一致。
class DragonBonesDataFileProcessor {
protected:
	static Error parse_dbdata_file(const String &p_path, Ref<DragonBonesData> &r_data, int64_t &r_uid);
	static Error save_dbdata_file(const String &p_path, const Ref<DragonBonesData> &p_data, int64_t p_uid);
};

class ResourceFormatSaverDragonBonesData : public ResourceFormatSaver, protected DragonBonesDataFileProcessor {
	GDCLASS(ResourceFormatSaverDragonBonesData, ResourceFormatSaver)
protected:
	static void _bind_methods() {}

public:
	virtual bool _recognize(const Ref<Resource> &resource) const override;
	virtual PackedStringArray _get_recognized_extensions(const Ref<Resource> &resource) const override;
	virtual Error _set_uid(const String &path, int64_t uid) override;
	virtual Error _save(const Ref<Resource> &resource, const String &path, uint32_t flags) override;
};

class ResourceFormatLoaderDragonBonesData : public ResourceFormatLoader, protected DragonBonesDataFileProcessor {
	GDCLASS(ResourceFormatLoaderDragonBonesData, ResourceFormatLoader)
protected:
	static void _bind_methods() {}

public:
	virtual PackedStringArray _get_recognized_extensions() const override;
	virtual bool _handles_type(const StringName &type) const override;
	virtual String _get_resource_type(const String &path) const override;
	virtual int64_t _get_resource_uid(const String &path) const override;
	virtual Variant _load(const String &path, const String &original_path, bool use_sub_threads, int32_t cache_mode) const override;
};

} //namespace godot
