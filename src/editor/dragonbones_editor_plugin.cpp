#include "dragonbones_editor_plugin.h"

#include "../dragonbones_factory.h"
#include "godot_cpp/classes/config_file.hpp"
#include "godot_cpp/classes/dir_access.hpp"
#include "godot_cpp/classes/file_access.hpp"
#include "godot_cpp/classes/resource_loader.hpp"
#include "godot_cpp/classes/resource_saver.hpp"
#include "godot_cpp/variant/callable_method_pointer.hpp"

using namespace godot;

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
	ret.push_back(DragonBonesFactory::SRC_DBJSON_EXT);
	ret.push_back(DragonBonesFactory::SRC_JSON_EXT);
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
	ERR_FAIL_COND_V(!FileAccess::file_exists(cfg_file), ERR_FILE_NOT_FOUND);

	Error err = cfg->load(cfg_file);
	if (err != OK) {
		return err;
	}
	cfg->set_value("remap", "type", "JSON");
	cfg->erase_section_key("remap", "path");
	cfg->erase_section_key("deps", "dest_files");

	err = cfg->save(cfg_file);
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
	const String ext_low = p_source_file.get_extension().to_lower();

	const auto save_path = p_save_path + String(".") + _get_save_extension();

	String base_path = p_source_file.get_basename();
	if (!base_path.ends_with("_ske")) {
		Ref<DragonBonesFactory> res;
		res.instantiate();
		ResourceSaver::get_singleton()->save(res, save_path);
		callable_mp_static(&set_import_config_to_json_type).call_deferred(p_source_file, p_save_path);
		// 非龙骨文件
		return OK;
	}

	base_path = base_path.erase(base_path.length() - strlen("_ske"), strlen("_ske"));

	const String ske_file = p_source_file;
	const String tex_atlas_file = base_path + "_tex.json";

	if (!FileAccess::file_exists(tex_atlas_file)) {
		WARN_PRINT_ED(vformat("\"%s\" may be a DragonBones file, but can't be import automatically.", p_source_file));
		return ERR_FILE_NOT_FOUND;
	}

	Ref<DragonBonesFactory> res;
	res.instantiate();

	Error err = res->load_texture_atlas_json_file_list(Array::make(tex_atlas_file));
	ERR_FAIL_COND_V(err != OK, err);
	err = res->load_dragon_bones_ske_file_list(Array::make(ske_file));
	ERR_FAIL_COND_V(err != OK, err);

	BitField<ResourceSaver::SaverFlags> flags = 0;
	if (p_options["compress"].booleanize()) {
		flags.set_flag(ResourceSaver::FLAG_COMPRESS);
	}

	return ResourceSaver::get_singleton()->save(res, save_path, flags);
}

///////////////////////////////
void DragonBonesEditorPlugin::_enter_tree() {
	import_plugin.instantiate();
	add_import_plugin(import_plugin);
}

void DragonBonesEditorPlugin::_exit_tree() {
	remove_import_plugin(import_plugin);
	import_plugin.unref();
}