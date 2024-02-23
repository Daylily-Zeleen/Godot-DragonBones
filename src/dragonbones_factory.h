#pragma once

#include "dragonBones/factory/BaseFactory.h"

#include "godot_cpp/classes/resource.hpp"
#include "godot_cpp/classes/texture2d.hpp"

namespace godot {

// TODO: 暂不支持内嵌图集数据的龙骨文件
class DragonBonesFactory : public Resource, private dragonBones::BaseFactory {
	GDCLASS(DragonBonesFactory, Resource)

protected:
	// dragonBones::BaseFactory 成员
	void set_building_dragon_bones(dragonBones::DragonBones *p_building_instance) { _dragonBones = p_building_instance; }

	dragonBones::DragonBonesData *loadDragonBonesData(const char *_p_data_loaded, const std::string &name = "");
	dragonBones::TextureAtlasData *loadTextureAtlasData(const char *_p_data_loaded, Ref<Texture> *_p_atlasTexture, const std::string &name = "", float scale = 1.0f);
	class DragonBonesArmature *buildArmatureDisplay(const std::string &armatureName, const std::string &dragonBonesName, const std::string &skinName = "", const std::string &textureAtlasName = "") const;

	virtual dragonBones::TextureAtlasData *_buildTextureAtlasData(dragonBones::TextureAtlasData *textureAtlasData, void *textureAtlas) const override;
	virtual dragonBones::Armature *_buildArmature(const dragonBones::BuildArmaturePackage &dataPackage) const override;
	virtual dragonBones::Slot *_buildSlot(const dragonBones::BuildArmaturePackage &dataPackage, const dragonBones::SlotData *slotData, dragonBones::Armature *armature) const override;
	virtual dragonBones::Armature *_buildChildArmature(const dragonBones::BuildArmaturePackage *dataPackage, dragonBones::Slot *slot, dragonBones::DisplayData *displayData) const override;
	virtual void _buildBones(const dragonBones::BuildArmaturePackage &dataPackage, dragonBones::Armature *armature) const override;

public:
	static constexpr auto SRC_DBJSON_EXT = "dbjson";
	static constexpr auto SRC_JSON_EXT = "json";
	static constexpr auto SRC_BIN_EXT = "dbbin";
	static constexpr auto SAVED_EXT = "res";

protected:
	static void _bind_methods();

public:
	DragonBonesFactory() = default;

	Error load_dragon_bones_ske_file_list(PackedStringArray p_files);
	Error load_texture_atlas_json_file_list(PackedStringArray p_files);

	bool can_create_dragon_bones_instance() const;

	dragonBones::DragonBones *create_dragon_bones(dragonBones::IEventDispatcher *p_event_manager, DragonBonesArmature *&r_main_armature, const String &p_armature_data_name = "", const String &p_skin_name = "");

private:
	//  Binding
	PackedStringArray dragon_bones_ske_file_list;
	PackedStringArray texture_atlas_json_file_list;

public:
	PackedStringArray get_dragon_bones_ske_file_list() const { return dragon_bones_ske_file_list; }
	void set_dragon_bones_ske_file_list(PackedStringArray p_files);

	PackedStringArray get_get_dragon_bones_ske_file_list() const { return texture_atlas_json_file_list; }
	void set_texture_atlas_json_file_list(PackedStringArray p_files);

	PackedStringArray get_loaded_dragon_bones_data_name_list() const;
	PackedStringArray get_loaded_dragon_bones_main_skin_name_list(const String &p_daragon_bones_data_name) const;
};

} //namespace godot