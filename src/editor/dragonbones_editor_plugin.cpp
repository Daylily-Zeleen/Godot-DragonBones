#include "dragonbones_editor_plugin.h"

#include "godot_cpp/classes/config_file.hpp"
#include "godot_cpp/classes/dir_access.hpp"
#include "godot_cpp/classes/editor_file_system.hpp"
#include "godot_cpp/classes/editor_file_system_directory.hpp"
#include "godot_cpp/classes/editor_interface.hpp"
#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/classes/resource_loader.hpp"
#include "godot_cpp/classes/resource_saver.hpp"
#include "godot_cpp/variant/utility_functions.hpp"

using namespace godot;

// ===========================================
void DragonBonesExportPlugin::_bind_methods() {
}

String DragonBonesExportPlugin::_get_name() const {
	return "Godot-DragonBones";
}

void DragonBonesExportPlugin::_export_file(const String &path, const String &type, const PackedStringArray &features) {
	if (type != DragonBonesFactory::get_class_static()) {
		return;
	}

	if (path.is_empty()) {
		return;
	}

	Ref<DragonBonesFactory> factory = ResourceLoader::get_singleton()->load(path, DragonBonesFactory::get_class_static(), ResourceLoader::CACHE_MODE_IGNORE);
	if (factory.is_null()) {
		return;
	}

	PackedStringArray files_to_add = factory->get_dragon_bones_ske_file_list().duplicate();
	files_to_add.append_array(factory->get_texture_atlas_json_file_list());

	for (const String &fp : files_to_add) {
		if (added_files.has(fp)) {
			continue;
		}

		ERR_CONTINUE(!FileAccess::file_exists(fp));
		auto fa = FileAccess::open(fp, FileAccess::READ);
		add_file(DragonBonesFactory::convert_to_imported_path(fp), fa->get_buffer(fa->get_length()), false);
		added_files.append(fp);
	}
}
// ===========================================
void DragonBonesImportPlugin::_bind_methods() {
}

String DragonBonesImportPlugin::_get_importer_name() const {
	return DragonBonesFactory::get_class_static().capitalize().replace(" ", "_").to_lower();
}

String DragonBonesImportPlugin::_get_visible_name() const {
	return DragonBonesFactory::get_class_static();
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
	if (!p_path.get_basename().ends_with("_ske")) {
		return {};
	}
	TypedArray<Dictionary> ret;
	Dictionary option;
	option["name"] = "compress";
	option["default_value"] = true;
	ret.push_back(option);
	return ret;
}

String DragonBonesImportPlugin::_get_save_extension() const {
	return DragonBonesFactory::SAVED_EXT;
}

String DragonBonesImportPlugin::_get_resource_type() const {
	return DragonBonesFactory::get_class_static();
}

double DragonBonesImportPlugin::_get_priority() const {
	return 2; // 提高优先级
}

int32_t DragonBonesImportPlugin::_get_import_order() const {
	return 0;
}

bool DragonBonesImportPlugin::_get_option_visibility(const String &path, const StringName &option_name, const Dictionary &options) const {
	return true;
}

static Error set_import_config_to_json_type(const String &p_src_file, const String &p_save_path) {
	ERR_FAIL_COND_V(p_src_file.get_extension().to_lower() != "json", ERR_FILE_UNRECOGNIZED);

	Ref<ConfigFile> cfg;
	cfg.instantiate();

	auto cfg_file = p_src_file + String(".import");

	cfg->set_value("remap", "importer", "keep");

	Error err = cfg->save(cfg_file);
	if (err != OK) {
		return err;
	}

	//
	const auto save_res_file = p_save_path + String(".") + DragonBonesFactory::SAVED_EXT;

	if (FileAccess::file_exists(save_res_file)) {
		err = DirAccess::remove_absolute(save_res_file);
	}

	if (err != OK) {
		return err;
	}

	const auto md5_file = p_save_path + String(".md5");
	if (FileAccess::file_exists(md5_file)) {
		return DirAccess::remove_absolute(md5_file);
	}

	return OK;
}

Error DragonBonesImportPlugin::_import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options,
		const TypedArray<String> &r_platform_variants, const TypedArray<String> &r_gen_files) const {
	auto factory = try_import(p_source_file);

	if (factory.is_null()) {
		return FAILED;
	}

	auto ext = p_source_file.get_extension();
	auto save_path = p_source_file.trim_suffix(ext) + _get_save_extension();
	return ResourceSaver::get_singleton()->save(factory, save_path);
}

Ref<DragonBonesFactory> DragonBonesImportPlugin::try_import(const String &p_ske_file) const {
	const String ext_low = p_ske_file.get_extension().to_lower();

	String base_path = p_ske_file.get_basename();
	if (!base_path.ends_with("_ske")) {
		return {};
	}

	base_path = base_path.trim_suffix("_ske");

	const String ske_file = p_ske_file;
	const String tex_atlas_file = base_path + "_tex.json";

	if (!FileAccess::file_exists(tex_atlas_file)) {
		return {};
	}

	Ref<DragonBonesFactory> res;
	res.instantiate();
	res->imported = true;

	Error err = res->load_texture_atlas_json_file_list(Array::make(tex_atlas_file));
	ERR_FAIL_COND_V(err != OK, {});

	err = res->load_dragon_bones_ske_file_list(Array::make(ske_file));
	ERR_FAIL_COND_V(err != OK, {});

	return res;
}

///////////////////////////////

void DragonBonesEditorPlugin::_reimport_dbfacroty_recursively(EditorFileSystemDirectory *p_dir, HashMap<String, Ref<DragonBonesFactory>> &r_factories) const {
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
		if (FileAccess::file_exists(fp.substr(0, fp.length() - json_extension_length) + DragonBonesFactory::SAVED_EXT)) {
			// 已存在，不重复导入，避免因为龙骨文件过多导致编辑器卡顿
			continue;
		}

		Ref<DragonBonesFactory> factory = import_plugin->try_import(fp);
		if (factory.is_valid()) {
			auto ext = fp.get_extension();
			auto save_path = fp.trim_suffix(ext) + DragonBonesFactory::SAVED_EXT;
			r_factories.insert(save_path, factory);
		}
	}

	for (int32_t i = 0; i < p_dir->get_subdir_count(); ++i) {
		_reimport_dbfacroty_recursively(p_dir->get_subdir(i), r_factories);
	}
}

void DragonBonesEditorPlugin::_on_filesystem_changed() {
	if (reimporting) {
		return;
	}

	reimporting = true;
	HashMap<String, Ref<DragonBonesFactory>> factories;
	_reimport_dbfacroty_recursively(EditorInterface::get_singleton()->get_resource_filesystem()->get_filesystem(), factories);

	if (!factories.is_empty()) {
		// 保存龙骨工厂
		for (auto &kv : factories) {
			const String path = kv.key;
			const Ref<DragonBonesFactory> factory = kv.value;
			Error err = ResourceSaver::get_singleton()->save(factory, path);

			if (err != OK) {
				ERR_PRINT(vformat("Save DragonBones factory failed: %s", UtilityFunctions::error_string(err)));
			}
		}
	}

	callable_mp(this, &DragonBonesEditorPlugin::clear_reimporting_flag).call_deferred();
}

void DragonBonesEditorPlugin::_enter_tree() {
	import_plugin.instantiate();
	add_import_plugin(import_plugin);

	export_plugin.instantiate();
	add_export_plugin(export_plugin);

	EditorInterface::get_singleton()->get_resource_filesystem()->connect("filesystem_changed", callable_mp(this, &DragonBonesEditorPlugin::_on_filesystem_changed));
}

void DragonBonesEditorPlugin::_exit_tree() {
	remove_import_plugin(import_plugin);
	import_plugin.unref();

	remove_export_plugin(export_plugin);
	export_plugin.unref();

	EditorInterface::get_singleton()->get_resource_filesystem()->disconnect("filesystem_changed", callable_mp(this, &DragonBonesEditorPlugin::_on_filesystem_changed));
}