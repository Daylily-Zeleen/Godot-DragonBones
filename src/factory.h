#pragma once

#include <godot_dragonbones.h>

#include <dragonBones/factory/BaseFactory.h>

#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_format_loader.hpp>
#include <godot_cpp/classes/resource_format_saver.hpp>
#include <godot_cpp/classes/texture2d.hpp>

#ifdef TOOLS_ENABLED
#include <godot_cpp/templates/hash_map.hpp>
#endif // TOOLS_ENABLED

namespace godot {

// TODO: 暂不支持内嵌图集数据的龙骨文件
class DragonBonesFactory : public Resource, private dragonBones::BaseFactory {
	GDCLASS(DragonBonesFactory, Resource)

	PackedByteArray get_file_data(const String &p_file) const;

public:
	static String get_imported_file_name(const String &p_path) { return p_path.md5_text() + ".dbimport"; }
	static String convert_to_imported_path(const String &p_path) {
		bool use_hidden_directory = ProjectSettings::get_singleton()->get_setting_with_override("application/config/use_hidden_project_data_directory");
		return vformat("res://%s/imported/%s", use_hidden_directory ? ".godot" : "godot", get_imported_file_name(p_path));
	}

protected:
	dragonBones::DragonBonesData *loadDragonBonesData(const char *p_data_loaded, const std::string &p_name = "");
	dragonBones::TextureAtlasData *loadTextureAtlasData(const char *p_data_loaded, String *p_atlas_data_file_path, const std::string &p_name = "", float p_scale = 1.0f);
	class DragonBonesArmature *buildArmatureDisplay(const std::string &p_armature_name, const std::string &p_dragonbones_name, const std::string &p_skinName = "", const std::string &p_texture_atlas_name = "") const;

	virtual dragonBones::TextureAtlasData *_buildTextureAtlasData(dragonBones::TextureAtlasData *textureAtlasData, void *textureAtlas) const override;
	virtual dragonBones::Armature *_buildArmature(const dragonBones::BuildArmaturePackage &dataPackage) const override;
	virtual dragonBones::Slot *_buildSlot(const dragonBones::BuildArmaturePackage &dataPackage, const dragonBones::SlotData *slotData, dragonBones::Armature *armature) const override;
	virtual dragonBones::Armature *_buildChildArmature(const dragonBones::BuildArmaturePackage *dataPackage, dragonBones::Slot *slot, dragonBones::DisplayData *displayData) const override;
	virtual void _buildBones(const dragonBones::BuildArmaturePackage &dataPackage, dragonBones::Armature *armature) const override;

public:
	// static constexpr auto SRC_DBJSON_EXT = "dbjson";
	static constexpr char SRC_JSON_EXT[] = "json";
	static constexpr char SRC_BIN_EXT[] = "dbbin";
	static constexpr char SAVED_EXT[] = "dbfactory";

protected:
	static void _bind_methods();
	String _to_string() const { return vformat("<%s#%s>", get_class_static(), get_instance_id()); }

#ifdef DEBUG_ENABLED
	void _validate_property(PropertyInfo &p_property) const;
#endif // DEBUG_ENABLED
public:
	Error load_dragon_bones_ske_file_list(PackedStringArray p_files);
	Error load_texture_atlas_json_file_list(PackedStringArray p_files);

	bool can_create_dragon_bones_instance() const;

	DragonBonesArmature *create_armature(class DragonBones *p_owner, const String &p_dragon_bones_data_name = "", const String &p_armature_name = "", const String &p_skin_name = "");

private:
	//  Binding
	PackedStringArray dragon_bones_ske_file_list;
	PackedStringArray texture_atlas_json_file_list;

public:
	PackedStringArray get_dragon_bones_ske_file_list() const { return dragon_bones_ske_file_list; }
	void set_dragon_bones_ske_file_list(PackedStringArray p_files);

	PackedStringArray get_texture_atlas_json_file_list() const { return texture_atlas_json_file_list; }
	void set_texture_atlas_json_file_list(PackedStringArray p_files);

	PackedStringArray get_loaded_dragon_bones_data_name_list() const;
	PackedStringArray get_loaded_dragon_bones_armature_name_list(const String &p_dragon_bones_data_name) const;
	PackedStringArray get_loaded_dragon_bones_main_skin_name_list(const String &p_dragon_bones_data_name, const String &p_armature_name) const;

private:
	bool imported{ false };
	friend class DragonBonesImportPlugin;
	friend class ResourceFormatSaverDragonBones;
	friend class ResourceFormatLoaderDragonBones;

#ifdef TOOLS_ENABLED
public:
	DragonBonesFactory();
	virtual ~DragonBonesFactory() override;

	static HashMap<String, DragonBonesFactory *> &get_all_imported_factories() {
		static HashMap<String, DragonBonesFactory *> ret;
		return ret;
	}

	bool is_imported() const { return imported; }

private:
	static bool editor_reimporting;
	friend class DragonBonesEditorPlugin;
#endif // TOOLS_ENABLED
};

class ResourceFormatSaverDragonBones : public ResourceFormatSaver {
	GDCLASS(ResourceFormatSaverDragonBones, ResourceFormatSaver)
protected:
	static void _bind_methods() {}

public:
	virtual bool _recognize(const Ref<Resource> &resource) const override;
	virtual PackedStringArray _get_recognized_extensions(const Ref<Resource> &resource) const override;
	virtual Error _set_uid(const String &path, int64_t uid) override;

	virtual Error _save(const Ref<Resource> &resource, const String &path, uint32_t flags) override;
};

class ResourceFormatLoaderDragonBones : public ResourceFormatLoader {
	GDCLASS(ResourceFormatLoaderDragonBones, ResourceFormatLoader)
protected:
	static void _bind_methods() {}

public:
	virtual PackedStringArray _get_recognized_extensions() const override;
	virtual bool _handles_type(const StringName &type) const override;
	virtual String _get_resource_type(const String &path) const override;
	virtual int64_t _get_resource_uid(const String &path) const override;
	virtual Variant _load(const String &path, const String &original_path, bool use_sub_threads, int32_t cache_mode) const override;

private:
	friend class DragonBonesEditorPlugin;
};
} //namespace godot