#include "dragonbones_factory.h"
#include "dragonBones/core/DragonBones.h"
#include "godot_cpp/classes/file_access.hpp"

#include "dragonbones_armature.h"
#include "godot_cpp/variant/utility_functions.hpp"
#include "wrappers/GDMesh.h"
#include "wrappers/GDTextureAtlasData.h"

using namespace godot;
using namespace dragonBones;
//////////////////////////////////////////////////////////////////

bool DragonBonesFactory::editor_reimporting{ false };

PackedByteArray DragonBonesFactory::get_file_data(const String &p_file) const {
	String fp = p_file;

	if (!Engine::get_singleton()->is_editor_hint()) {
		// 编辑器中运行不执行回退逻辑
		if (!FileAccess::file_exists(fp)) {
			fp = convert_to_imported_path(fp);
		}
	}

	Ref<FileAccess> file = FileAccess::open(fp, FileAccess::READ);
	if (file.is_valid()) {
		PackedByteArray raw_data;
		raw_data.resize(file->get_length() + 1);
		file->get_buffer(raw_data.ptrw(), file->get_length());
		raw_data.set(file->get_length(), 0x00);
		return raw_data;
	} else {
#ifdef TOOLS_ENABLED
		if (editor_reimporting) {
			// 编辑器执行重新导入时获取不到文件属于预期，因此不打印错误
			return {};
		}
#endif // TOOLS_ENABLED
		ERR_PRINT(vformat("Open \"%s\" failed: \"%s\"", fp, UtilityFunctions::error_string(FileAccess::get_open_error())));
		return {};
	}
}

///  工厂实现  ///////////////////////////////////////////////////////////////
DragonBonesData *DragonBonesFactory::loadDragonBonesData(const char *_p_data_loaded, const std::string &name) {
	if (!name.empty()) {
		const auto existedData = getDragonBonesData(name);

		if (existedData) {
			return existedData;
		}
	}
	return parseDragonBonesData(_p_data_loaded, name, 1.0f);
}

TextureAtlasData *DragonBonesFactory::loadTextureAtlasData(const char *_p_data_loaded, Ref<Texture> *_p_atlasTexture, const std::string &name, float scale) {
	return BaseFactory::parseTextureAtlasData(_p_data_loaded, _p_atlasTexture, name, scale);
}

DragonBonesArmature *DragonBonesFactory::buildArmatureDisplay(const std::string &armatureName, const std::string &dragonBonesName, const std::string &skinName, const std::string &textureAtlasName) const {
	const auto armature = buildArmature(armatureName, dragonBonesName, skinName, textureAtlasName);

	// 初始化纹理
	for (const auto slot : armature->getSlots()) {
		if (auto slot_gd = static_cast<Slot_GD *>(slot)) {
			slot_gd->update_display_texutre();
		}
	}

	if (armature != nullptr) {
		_dragonBones->getClock()->add(armature);
		return static_cast<DragonBonesArmature *>(armature->getDisplay());
	}
	return nullptr;
}

TextureAtlasData *DragonBonesFactory::_buildTextureAtlasData(TextureAtlasData *textureAtlasData, void *textureAtlas) const {
	auto textureAtlasData_ = static_cast<GDTextureAtlasData *>(textureAtlasData);

	if (textureAtlasData != nullptr) {
		textureAtlasData_->setRenderTexture();
	} else {
		textureAtlasData_ = BaseObject::borrowObject<GDTextureAtlasData>();
	}
	return textureAtlasData_;
}

Armature *DragonBonesFactory::_buildArmature(const BuildArmaturePackage &dataPackage) const {
	const auto armature = BaseObject::borrowObject<Armature>();
	DragonBonesArmature *armatureDisplay{ nullptr };

	if (building_main_armature && !building_main_armature->is_initialized()) {
		// 主Armature作为内部节点将被重用
		armatureDisplay = building_main_armature;
	} else {
		armatureDisplay = memnew(DragonBonesArmature);
	}

	armature->init(dataPackage.armature, armatureDisplay, armatureDisplay, _dragonBones);
	armatureDisplay->set_name(to_gd_str(armature->getName()));
	return armature;
}

Slot *DragonBonesFactory::_buildSlot(const BuildArmaturePackage &dataPackage, const SlotData *slotData, Armature *armature) const {
	auto slot = BaseObject::borrowObject<Slot_GD>();
	auto wrapperDisplay = memnew(GDMesh);

	slot->init(slotData, armature, wrapperDisplay, wrapperDisplay);
	slot->update(0);

	// slot->update_display_texutre();

	Ref<DragonBonesSlot> tree_slot{ memnew(DragonBonesSlot(slot)) };

	const auto proxy = static_cast<DragonBonesArmature *>(slot->getArmature()->getDisplay());
	proxy->add_slot(slot->getName(), tree_slot);
	wrapperDisplay->set_name(to_gd_str(slot->getName()));

	return slot;
}

Armature *DragonBonesFactory::_buildChildArmature(const BuildArmaturePackage *dataPackage, Slot *slot, DisplayData *displayData) const {
	auto childDisplayName = slot->_slotData->name;

	const auto proxy = static_cast<DragonBonesArmature *>(slot->getArmature()->getDisplay());

	DragonBonesArmature *childArmature = nullptr;

	if (dataPackage != nullptr) {
		childArmature = buildArmatureDisplay(displayData->path, dataPackage->dataName);
	} else {
		childArmature = buildArmatureDisplay(displayData->path, displayData->getParent()->parent->parent->name);
	}

	if (childArmature == nullptr) {
		ERR_PRINT("Child armature is null");
		return nullptr;
	}

	childArmature->set_z_index(slot->_zOrder);
	childArmature->getArmature()->setFlipY(true);
	childArmature->hide();
	proxy->add_child(childArmature, false, Node::INTERNAL_MODE_BACK);

	return childArmature->getArmature();
}

void DragonBonesFactory::_buildBones(const BuildArmaturePackage &dataPackage, Armature *armature) const {
	for (const auto boneData : dataPackage.armature->sortedBones) {
		const auto bone = BaseObject::borrowObject<Bone>();
		bone->init(boneData, armature);

		DragonBonesArmature *display = static_cast<DragonBonesArmature *>(armature->getDisplay());
		Ref<DragonBonesBone> new_bone{ memnew(DragonBonesBone(bone, display)) };
		display->add_bone(bone->getName(), new_bone);
	}

	for (const auto &pair : dataPackage.armature->constraints) {
		// TODO more constraint type.
		const auto constraint = BaseObject::borrowObject<IKConstraint>();
		constraint->init(pair.second, armature);
		armature->_addConstraint(constraint);
	}
}

///  对外接口c成员  ///////////////////////////////////////////////////////////////
void make_dragon_bones_data_unref_texture_atlas_data(dragonBones::DragonBonesData *p_data, const dragonBones::TextureAtlasData *p_atlas) {
	if (!p_data || !p_atlas) {
		return;
	}

	for (const auto kv : p_data->armatures) {
		const auto armature = kv.second;
		if (!armature) {
			continue;
		}

		for (const auto skin_kv : armature->skins) {
			const auto skin = skin_kv.second;
			if (!skin) {
				continue;
			}

			for (const auto displays_kv : skin->displays) {
				const auto displays = displays_kv.second;
				for (const auto display : displays) {
					if (displays.empty()) {
						continue;
					}

					if (display->type == dragonBones::DisplayType::Image) {
						if (auto image_display = static_cast<dragonBones::ImageDisplayData *>(display)) {
							if (image_display->texture->parent == p_atlas) {
								image_display->texture = nullptr; // 清除对该图集的散图引用
							}
						}
					} else if (display->type == dragonBones::DisplayType::Mesh) {
						if (auto mesh_display = static_cast<dragonBones::MeshDisplayData *>(display)) {
							if (mesh_display->texture->parent == p_atlas) {
								mesh_display->texture = nullptr; // 清除对该图集的散图引用
							}
						}
					}
				}
			}
		}
	}
}

Error DragonBonesFactory::load_dragon_bones_ske_file_list(PackedStringArray p_files) {
	Error err = OK;
	// 去重
	for (int64_t i = p_files.size() - 1; i >= 0; --i) {
		auto file = p_files[i];
		if (p_files.count(file) > 1) {
			p_files.remove_at(i);
			++i;
		}
	}

	std::vector<std::string> data_names;
	for (const auto &kv : getAllDragonBonesData()) {
		data_names.emplace_back(kv.first);
	}

	for (const auto &name : data_names) {
		removeDragonBonesData(name, true);
	}

	// 赋值
	dragon_bones_ske_file_list = p_files;

	// 加载
	for (const auto &file_path : p_files) {
		if (file_path.is_empty()) {
			continue;
		}

		auto raw_data = get_file_data(file_path);
		ERR_CONTINUE_MSG(raw_data.is_empty(), (err = ERR_PARSE_ERROR, vformat("Load DragonBones ske file failed: \"%s\".", file_path)));

		if (!loadDragonBonesData((const char *)raw_data.ptr())) {
			err = ERR_PARSE_ERROR;
			ERR_PRINT(vformat("Parse failed: \"%s\"", file_path));
		}
	}

	return err;
}

Error DragonBonesFactory::load_texture_atlas_json_file_list(PackedStringArray p_files) {
	Error err = OK;
	// 去重
	for (int64_t i = p_files.size() - 1; i >= 0; --i) {
		auto file = p_files[i];
		if (p_files.count(file) > 1) {
			p_files.remove_at(i);
			++i;
		}
	}

	std::vector<std::string> data_names;

	for (const auto &atlas_kv : getAllTextureAtlasData()) {
		// 去除占用
		for (const auto &db_kv : getAllDragonBonesData()) {
			for (const auto &atlas : atlas_kv.second) {
				make_dragon_bones_data_unref_texture_atlas_data(db_kv.second, atlas);
			}
		}

		data_names.emplace_back(atlas_kv.first);
	}

	for (const auto &name : data_names) {
		removeTextureAtlasData(name, true);
	}

	// 赋值
	texture_atlas_json_file_list = p_files;

	// 加载
	for (const auto &file_path : p_files) {
		if (file_path.is_empty()) {
			continue;
		}

		auto raw_data = get_file_data(file_path);
		ERR_CONTINUE_MSG(raw_data.is_empty(), (err = ERR_PARSE_ERROR, vformat("Load DragonBones tex file failed: \"%s\".", file_path)));

		const auto data = static_cast<GDTextureAtlasData *>(loadTextureAtlasData((const char *)raw_data.ptr(), nullptr));
		ERR_CONTINUE_MSG(!data, (err = ERR_PARSE_ERROR, vformat("Parse failed: \"%s\"", file_path)));

		if (data->imagePath.empty()) {
			auto datas = getTextureAtlasData(data->name);
			if (datas) {
				for (auto it = datas->begin(); it != datas->end(); ++it) {
					if (*it != data) {
						continue;
					}
					datas->erase(it);
				}
			}

			err = ERR_PARSE_ERROR;
			ERR_CONTINUE_MSG(false, vformat("Unsupport texture atlas file: \"%s\", need \"imagePath\" field.", file_path));
		}

		data->init(file_path.get_base_dir().path_join(to_gd_str(data->imagePath)));
	}
	return err;
}

void DragonBonesFactory::set_dragon_bones_ske_file_list(PackedStringArray p_files) {
	load_dragon_bones_ske_file_list(std::move(p_files));
	emit_changed();
}

void DragonBonesFactory::set_texture_atlas_json_file_list(PackedStringArray p_files) {
	load_texture_atlas_json_file_list(std::move(p_files));
	emit_changed();
}

PackedStringArray DragonBonesFactory::get_loaded_dragon_bones_data_name_list() const {
	PackedStringArray ret;
	for (auto kv : getAllDragonBonesData()) {
		ret.push_back(to_gd_str(kv.first));
	}
	return ret;
}

PackedStringArray DragonBonesFactory::get_loaded_dragon_bones_main_skin_name_list(const String &p_daragon_bones_data_name) const {
	PackedStringArray ret;

	DragonBonesData *dbdata = nullptr;
	if (!p_daragon_bones_data_name.is_empty()) {
		dbdata = getDragonBonesData(to_std_str(p_daragon_bones_data_name));
	} else {
		if (getAllDragonBonesData().size() > 0) {
			dbdata = getAllDragonBonesData().begin()->second;
		}
	}

	if (p_daragon_bones_data_name.is_empty()) {
		return ret;
	}

	ERR_FAIL_NULL_V(dbdata, ret);

	auto armature_data = dbdata->getArmature(dbdata->armatureNames[0]);
	ERR_FAIL_NULL_V(armature_data, ret);

	for (const auto &kv : armature_data->skins) {
		if (kv.second) {
			ret.push_back(to_gd_str(kv.second->name));
		}
	}
	return ret;
}

bool DragonBonesFactory::can_create_dragon_bones_instance() const {
	return _dragonBonesDataMap.size() > 0 && _textureAtlasDataMap.size() > 0;
}

dragonBones::DragonBones *DragonBonesFactory::create_dragon_bones(
		dragonBones::IEventDispatcher *p_event_manager, DragonBonesArmature *p_main_armature, const String &p_armature_data_name, const String &p_skin_name) {
	const auto &dragon_bones_data_list = getAllDragonBonesData();
	ERR_FAIL_COND_V(dragon_bones_data_list.size() <= 0, nullptr);
	dragonBones::DragonBonesData *dragon_bones_data{ nullptr };

	if (p_armature_data_name.is_empty()) {
		dragon_bones_data = dragon_bones_data_list.begin()->second;
	} else {
		dragon_bones_data = getDragonBonesData(to_std_str(p_armature_data_name));
	}

	ERR_FAIL_NULL_V(dragon_bones_data, nullptr);
	ERR_FAIL_COND_V(dragon_bones_data->armatureNames.size() <= 0, nullptr);

	auto *ret{ memnew(dragonBones::DragonBones(p_event_manager)) };
	set_building_dragon_bones(ret);

	const auto armature_name = dragon_bones_data->getArmatureNames()[0];

	building_main_armature = p_main_armature;
	p_main_armature = buildArmatureDisplay(armature_name, dragon_bones_data->name, to_std_str(p_skin_name));
	building_main_armature = nullptr;

	return ret;
}

void DragonBonesFactory::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_loaded_dragon_bones_main_skin_name_list", "dragon_bones_data_name"), &DragonBonesFactory::get_loaded_dragon_bones_main_skin_name_list);
	ClassDB::bind_method(D_METHOD("get_loaded_dragon_bones_data_name_list"), &DragonBonesFactory::get_loaded_dragon_bones_data_name_list);

	ClassDB::bind_method(D_METHOD("set_dragon_bones_ske_file_list", "dragon_bones_ske_file_list"), &DragonBonesFactory::set_dragon_bones_ske_file_list);
	ClassDB::bind_method(D_METHOD("get_dragon_bones_ske_file_list"), &DragonBonesFactory::get_dragon_bones_ske_file_list);

	ClassDB::bind_method(D_METHOD("set_texture_atlas_json_file_list", "texture_atlas_json_file_list"), &DragonBonesFactory::set_texture_atlas_json_file_list);
	ClassDB::bind_method(D_METHOD("get_texture_atlas_json_file_list"), &DragonBonesFactory::get_texture_atlas_json_file_list);

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "dragon_bones_ske_file_list", PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:%s", Variant::STRING, PROPERTY_HINT_FILE, "*.dbjson,*.json,*.dbbin")), "set_dragon_bones_ske_file_list", "get_dragon_bones_ske_file_list");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "texture_atlas_json_file_list", PROPERTY_HINT_TYPE_STRING, vformat("%d/%d:%s", Variant::STRING, PROPERTY_HINT_FILE, "*.json")), "set_texture_atlas_json_file_list", "get_texture_atlas_json_file_list");
}

#ifdef DEBUG_ENABLED
void DragonBonesFactory::_validate_property(PropertyInfo &p_property) const {
	if (imported) {
		if (p_property.name == StringName("dragon_bones_ske_file_list") || p_property.name == StringName("texture_atlas_json_file_list")) {
			p_property.usage |= PROPERTY_USAGE_READ_ONLY;
		}
	}
}
#endif // DEBUG_ENABLED

#ifdef TOOLS_ENABLED

DragonBonesFactory::DragonBonesFactory() {
}

DragonBonesFactory::~DragonBonesFactory() {
	if (!imported) {
		return;
	}

	auto &all = get_all_imported_factories();
	for (auto it = all.begin(); it != all.end(); ++it) {
		if (it->value == this) {
			all.remove(it);
			return;
		}
	}
}
#endif // TOOLS_ENABLED

// ===========================================
bool ResourceFormatSaverDragonBones::_recognize(const Ref<Resource> &resource) const {
	return cast_to<DragonBonesFactory>(resource.ptr());
}

PackedStringArray ResourceFormatSaverDragonBones::_get_recognized_extensions(const Ref<Resource> &resource) const {
	if (cast_to<DragonBonesFactory>(resource.ptr())) {
		return Array::make(DragonBonesFactory::SAVED_EXT);
	}
	return {};
}

Error ResourceFormatSaverDragonBones::_save(const Ref<Resource> &resource, const String &path, uint32_t flags) {
	Ref<DragonBonesFactory> factory = resource;
	ERR_FAIL_NULL_V(factory, ERR_INVALID_PARAMETER);

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	ERR_FAIL_NULL_V_MSG(file, FileAccess::get_open_error(), vformat("Cannot save DragonBonesFactory '%s': %s.", path, UtilityFunctions::error_string(FileAccess::get_open_error())));

	file->store_var(factory->get_dragon_bones_ske_file_list());
	file->store_var(factory->get_texture_atlas_json_file_list());
	file->store_var(factory->imported);

	return OK;
}

// ===========================================

PackedStringArray ResourceFormatLoaderDragonBones::_get_recognized_extensions() const {
	return Array::make(DragonBonesFactory::SAVED_EXT);
}

bool ResourceFormatLoaderDragonBones::_handles_type(const StringName &type) const {
	return type == DragonBonesFactory::get_class_static() || ClassDB::is_parent_class(type, DragonBonesFactory::get_class_static());
}

String ResourceFormatLoaderDragonBones::_get_resource_type(const String &path) const {
	if (path.get_extension().to_lower() == DragonBonesFactory::SAVED_EXT) {
		return DragonBonesFactory::get_class_static();
	}
	return "";
}

Variant ResourceFormatLoaderDragonBones::_load(const String &path, const String &original_path, bool use_sub_threads, int32_t cache_mode) const {
	PackedStringArray ske_files;
	PackedStringArray atlas_files;
	bool imported;

	Error err = parse_dbfactory_file(path, ske_files, atlas_files, imported);
	if (err != OK) {
		return err;
	}

	Ref<DragonBonesFactory> ret;
	ret.instantiate();

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		if (!imported) {
			ret->set_dragon_bones_ske_file_list(ske_files);
			ret->set_texture_atlas_json_file_list(atlas_files);
		} else {
			bool valid = true;
			for (const auto &f : ske_files) {
				if (!FileAccess::file_exists(f)) {
					valid = false;
				}
			}
			for (const auto &f : atlas_files) {
				if (!FileAccess::file_exists(f)) {
					valid = false;
				}
			}

			// 仅在所有依赖文件存在时赋值，否则等待编辑器重新导入
			if (valid) {
				ret->set_dragon_bones_ske_file_list(ske_files);
				ret->set_texture_atlas_json_file_list(atlas_files);
			}
		}
		ret->imported = imported;
	} else {
		ret->set_dragon_bones_ske_file_list(ske_files);
		ret->set_texture_atlas_json_file_list(atlas_files);
		ret->imported = imported;
	}
#else // ! TOOLS_ENABLED
	ret->set_dragon_bones_ske_file_list(ske_files);
	ret->set_texture_atlas_json_file_list(atlas_files);
	ret->imported = imported;
#endif // TOOLS_ENABLED

#ifdef TOOLS_ENABLED
	if (imported) {
		DragonBonesFactory::get_all_imported_factories().insert(path, ret.ptr());
	}
#endif // TOOLS_ENABLED

	return ret;
}

Error ResourceFormatLoaderDragonBones::parse_dbfactory_file(const String &p_path, PackedStringArray &r_ske_files, PackedStringArray &r_atlas_files, bool &r_imported) {
	ERR_FAIL_COND_V(p_path.get_extension().to_lower() != DragonBonesFactory::SAVED_EXT, ERR_FILE_UNRECOGNIZED);
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::ModeFlags::READ);
	ERR_FAIL_NULL_V(f, FileAccess::get_open_error());

	r_ske_files.append_array(f->get_var());
	r_atlas_files.append_array(f->get_var());
	r_imported = f->get_var();

	return OK;
}