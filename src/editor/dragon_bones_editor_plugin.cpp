/**************************************************************************/
/*  dragon_bones_editor_plugin.cpp                                        */
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

#include "dragon_bones_editor_plugin.h"

#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/editor_file_system.hpp>
#include <godot_cpp/classes/editor_file_system_directory.hpp>
#include <godot_cpp/classes/editor_interface.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

bool DragonBonesEditorPlugin::reimporting{ false };

// ===========================================
void DragonBonesExportPlugin::_bind_methods() {
}

String DragonBonesExportPlugin::_get_name() const {
	return "Godot-DragonBones";
}

void DragonBonesExportPlugin::_export_file(const String &path, const String &type, const PackedStringArray &features) {
	if (type != DragonBonesData::get_class_static()) {
		return;
	}

	if (path.is_empty()) {
		return;
	}

	Ref<DragonBonesData> data = ResourceLoader::get_singleton()->load(path, DragonBonesData::get_class_static(), ResourceLoader::CACHE_MODE_IGNORE);
	if (data.is_null()) {
		return;
	}

	PackedStringArray files_to_add;

	const String ske_path = data->resolve_ske_path();
	if (!ske_path.is_empty()) {
		files_to_add.push_back(ske_path);
	}

	// 仅图集模式有需要携带的 json；散图模式的纹理源是目录（普通资源，与导出无关）。
	if (data->get_texture_source_type() == DragonBonesData::TEXTURE_SOURCE_ATLAS) {
		const String atlas_path = data->resolve_texture_source_path();
		if (!atlas_path.is_empty()) {
			files_to_add.push_back(atlas_path);
		}
	}

	for (const String &fp : files_to_add) {
		if (added_files.has(fp)) {
			continue;
		}

		ERR_CONTINUE_MSG(!FileAccess::file_exists(fp), vformat("DragonBones data file not found: \"%s\".", fp));
		auto fa = FileAccess::open(fp, FileAccess::READ);
		add_file(DragonBonesFactory::convert_to_imported_path(fp), fa->get_buffer(fa->get_length()), false);
		added_files.append(fp);
	}
}

// ===========================================
void DragonBonesImportPlugin::_bind_methods() {
}

String DragonBonesImportPlugin::_get_importer_name() const {
	return DragonBonesData::get_class_static().capitalize().replace(" ", "_").to_lower();
}

String DragonBonesImportPlugin::_get_visible_name() const {
	return DragonBonesData::get_class_static();
}

int32_t DragonBonesImportPlugin::_get_preset_count() const {
	return 1;
}

String DragonBonesImportPlugin::_get_preset_name(int32_t preset_index) const {
	return preset_index == 0 ? "Default" : "";
}

PackedStringArray DragonBonesImportPlugin::_get_recognized_extensions() const {
	PackedStringArray ret;
	// 只对二进制格式进行导入
	ret.push_back(DragonBonesFactory::SRC_BIN_EXT);
	return ret;
}

TypedArray<Dictionary> DragonBonesImportPlugin::_get_import_options(const String &p_path, int32_t p_preset_index) const {
	return {};
}

String DragonBonesImportPlugin::_get_save_extension() const {
	return DragonBonesData::SAVED_EXT;
}

String DragonBonesImportPlugin::_get_resource_type() const {
	return DragonBonesData::get_class_static();
}

decltype(EditorImportPlugin()._get_priority()) DragonBonesImportPlugin::_get_priority() const {
	return 2; // 提高优先级
}

int32_t DragonBonesImportPlugin::_get_import_order() const {
	return 0;
}

bool DragonBonesImportPlugin::_get_option_visibility(const String &path, const StringName &option_name, const Dictionary &options) const {
	return true;
}

Error DragonBonesImportPlugin::_import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options,
									   const TypedArray<String> &r_platform_variants, const TypedArray<String> &r_gen_files) const {
	auto data = try_import(p_source_file);

	if (data.is_null()) {
		return FAILED;
	}

	auto ext = p_source_file.get_extension();
	auto save_path = p_source_file.trim_suffix(ext) + _get_save_extension();
	return ResourceSaver::get_singleton()->save(data, save_path);
}

Ref<DragonBonesData> DragonBonesImportPlugin::try_import(const String &p_ske_file) const {
	String base_path = p_ske_file.get_basename();
	if (!base_path.ends_with("_ske")) {
		return {};
	}

	base_path = base_path.trim_suffix("_ske");

	// 两种导出形态：
	//   图集模式：<base>_tex.json + 整张图集；
	//   散图模式（Images 导出）：无 _tex.json，<base>_texture/ 下每个部件一张独立 PNG。
	const String tex_atlas_file = base_path + "_tex.json";
	const String scattered_texture_dir = base_path + "_texture";
	const bool has_texture_atlas = FileAccess::file_exists(tex_atlas_file);
	const bool has_scattered_textures = !has_texture_atlas && DirAccess::dir_exists_absolute(scattered_texture_dir);

	if (!has_texture_atlas && !has_scattered_textures) {
		return {};
	}

	Ref<DragonBonesData> ret;
	ret.instantiate();

	// 只存文件名/文件夹名（不存绝对路径）：加载时在资源同目录解析。
	// 这样资源与数据文件一起移动即可继续工作，无需任何跟随逻辑。
	ret->set_ske_file(p_ske_file.get_file());
	if (has_texture_atlas) {
		ret->set_texture_source_type(DragonBonesData::TEXTURE_SOURCE_ATLAS);
		ret->set_texture_source(tex_atlas_file.get_file());
	} else {
		ret->set_texture_source_type(DragonBonesData::TEXTURE_SOURCE_SCATTERED);
		ret->set_texture_source(scattered_texture_dir.get_file());
	}
	ret->imported = true;

	return ret;
}

///////////////////////////////
const auto SETTING_AUTO_GENERATE_DBDATA = "Godot_DragonBones/auto_generate_dbdata";

void DragonBonesEditorPlugin::_reimport_dbdata_recursively(EditorFileSystemDirectory *p_dir, HashMap<String, Ref<DragonBonesData>> &r_datas) const {
	if (!p_dir) {
		return;
	}

	for (int32_t i = 0; i < p_dir->get_file_count(); ++i) {
		const String fp = p_dir->get_file_path(i);
		if (fp.get_extension().to_lower() != "json") {
			// 仅对json进行处理
			continue;
		}

		constexpr decltype(fp.length()) json_extension_length = sizeof("json") - 1;
		const String save_path = fp.substr(0, fp.length() - json_extension_length) + DragonBonesData::SAVED_EXT;
		if (FileAccess::file_exists(save_path)) {
			// 已存在，避免因为龙骨文件过多导致编辑器卡顿
			continue;
		}

		Ref<DragonBonesData> data = import_plugin->try_import(fp);
		if (data.is_valid()) {
			r_datas.insert(save_path, data);
		}
	}

	for (int32_t i = 0; i < p_dir->get_subdir_count(); ++i) {
		_reimport_dbdata_recursively(p_dir->get_subdir(i), r_datas);
	}
}

void DragonBonesEditorPlugin::_on_filesystem_changed() {
	if (reimporting) {
		return;
	}
	reimporting = true;

	if (ProjectSettings::get_singleton()->get_setting(SETTING_AUTO_GENERATE_DBDATA, false)) {
		// 默认不进行导入。需要手动创建资源。
		HashMap<String, Ref<DragonBonesData>> datas;
		_reimport_dbdata_recursively(EditorInterface::get_singleton()->get_resource_filesystem()->get_filesystem(), datas);

		if (!datas.is_empty()) {
			for (auto &kv : datas) {
				const String path = kv.key;
				const Ref<DragonBonesData> data = kv.value;

				Error err = ResourceSaver::get_singleton()->save(data, path, ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS | ResourceSaver::FLAG_CHANGE_PATH);

				if (err != OK) {
					ERR_PRINT(vformat("Save DragonBones data failed: %s", UtilityFunctions::error_string(err)));
				}
			}
		}
	}

	callable_mp(this, &DragonBonesEditorPlugin::clear_reimporting_flag).call_deferred();
}

void DragonBonesEditorPlugin::clear_reimporting_flag() { reimporting = false; }

void DragonBonesEditorPlugin::_enter_tree() {
	import_plugin.instantiate();
	add_import_plugin(import_plugin);

	export_plugin.instantiate();
	add_export_plugin(export_plugin);

	EditorInterface::get_singleton()->get_resource_filesystem()->connect("filesystem_changed", callable_mp(this, &DragonBonesEditorPlugin::_on_filesystem_changed));

	if (!ProjectSettings::get_singleton()->has_setting(SETTING_AUTO_GENERATE_DBDATA)) {
		ProjectSettings::get_singleton()->set_setting(SETTING_AUTO_GENERATE_DBDATA, false);
		ProjectSettings::get_singleton()->set_initial_value(SETTING_AUTO_GENERATE_DBDATA, false);
		ProjectSettings::get_singleton()->set_as_basic(SETTING_AUTO_GENERATE_DBDATA, true);
	}
}

void DragonBonesEditorPlugin::_exit_tree() {
	remove_import_plugin(import_plugin);
	import_plugin.unref();

	remove_export_plugin(export_plugin);
	export_plugin.unref();

	EditorInterface::get_singleton()->get_resource_filesystem()->disconnect("filesystem_changed", callable_mp(this, &DragonBonesEditorPlugin::_on_filesystem_changed));
}