/**************************************************************************/
/*  factory.cpp                                                           */
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

#include "factory.h"

#include "armature.h"
#include "armature_view.h"
#include "dragon_bones.h"
#include "mesh_display.h"
#include "slot.h"
#include "texture_atlas_data.h"

#include <dragonBones/animation/WorldClock.h>
#include <dragonBones/core/DragonBones.h>

#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>
#include <godot_cpp/classes/resource_uid.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#ifdef TOOLS_ENABLED
#include <editor/dragon_bones_editor_plugin.h>
#endif // TOOLS_ENABLED

#include <dragonBones/armature/Constraint.h>
#include <dragonBones/armature/Slot.h>

// 本文件同时 using 两个 namespace（与既有写法一致）；两处同名类型用别名区分，
// 见 db_data.h 的说明。
using namespace godot;
using namespace dragonBones;

using GDDragonBonesData = godot::DragonBonesData; // 资源（*.dbdata）
using DBDragonBonesData = dragonBones::DragonBonesData; // 运行时数据

namespace {
DragonBonesFactory *singleton{ nullptr };
} //namespace

//////////////////////////////////////////////////////////////////

DragonBonesFactory *DragonBonesFactory::get_singleton() {
	return singleton;
}

DragonBonesFactory::DragonBonesFactory() {
	CRASH_COND_MSG(singleton != nullptr, "只能存在一个 DragonBonesFactory 实例。");
	singleton = this;
}

DragonBonesFactory::~DragonBonesFactory() {
	singleton = nullptr;
	// 释放本工厂持有的运行时数据（含图集里挂的 Ref<Texture2D>，由 returnToPool -> _onClear 释放）。
	// 必须 disposeData=true：否则图集不回池、其纹理引用永不释放（退出时报纹理泄漏）。
	// 注意：本析构由模块卸载驱动，且必须先于 BaseObject::clearPool()，
	// 否则池被清空后再 returnToPool 会二次释放。
	clear(true);
}

PackedByteArray DragonBonesFactory::get_file_data(const String &p_file) const {
	String fp = p_file;

	if (!Engine::get_singleton()->is_editor_hint()) {
		// 编辑器中不执行回退逻辑：编辑器持有原始文件
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
		if (DragonBonesEditorPlugin::reimporting) {
			// 编辑器执行重新导入时获取不到文件属于预期，因此不打印错误
			return {};
		}
#endif // TOOLS_ENABLED
		ERR_PRINT(vformat("Open \"%s\" failed: \"%s\"", fp, UtilityFunctions::error_string(FileAccess::get_open_error())));
		return {};
	}
}

///  工厂实现  ///////////////////////////////////////////////////////////////

DBDragonBonesData *DragonBonesFactory::loadDragonBonesData(const char *p_data_loaded, const std::string &p_name) {
	return parseDragonBonesData(p_data_loaded, p_name, 1.0f);
}

TextureAtlasData *DragonBonesFactory::loadTextureAtlasData(const char *p_data_loaded, void *p_atlas_data_file_path, const std::string &p_name, float p_scale) {
	return BaseFactory::parseTextureAtlasData(p_data_loaded, p_atlas_data_file_path, p_name, p_scale);
}

DragonBonesArmature *DragonBonesFactory::buildArmatureDisplay(const std::string &p_armature_name, const std::string &p_dragon_bones_name, const std::string &p_skin_name, const std::string &p_texture_atlas_name) const {
	const auto armature = buildArmature(p_armature_name, p_dragon_bones_name, p_skin_name, p_texture_atlas_name);
	if (armature != nullptr) {
		_dragonBones->getClock()->add(armature);
		return static_cast<DragonBonesArmature *>(armature->getDisplay());
	}
	return nullptr;
}

TextureAtlasData *DragonBonesFactory::_buildTextureAtlasData(TextureAtlasData *textureAtlasData, void *textureAtlas) const {
	if (textureAtlasData == nullptr && textureAtlas == nullptr) {
		return BaseObject::borrowObject<DragonBonesTextureAtlasData>();
	}
	auto atlas_data = static_cast<DragonBonesTextureAtlasData *>(textureAtlasData);
	const String *file_path = static_cast<String *>(textureAtlas);
	auto image_path = file_path->get_base_dir().path_join(to_gd_str(atlas_data->imagePath));
	ERR_FAIL_COND_V_MSG(!ResourceLoader::get_singleton()->exists(image_path, "Texture2D"), atlas_data, vformat("Unsupport texture atlas file: \"%s\", missing atlas image.", *file_path));

	atlas_data->init(ResourceLoader::get_singleton()->load(image_path));
	return atlas_data;
}

Armature *DragonBonesFactory::_buildArmature(const BuildArmaturePackage &dataPackage) const {
	ERR_FAIL_NULL_V(_dragonBones, nullptr);
	const auto armature = BaseObject::borrowObject<Armature>();
	DragonBonesArmature *armatureDisplay{ memnew(DragonBonesArmature) };
	armatureDisplay->armature_view = building_armature; // 该插件里 _dragonBones->getEventManager() 就是 DragonBones 节点

	armature->init(dataPackage.armature, armatureDisplay, armatureDisplay, _dragonBones);
	return armature;
}

Slot *DragonBonesFactory::_buildSlot(const BuildArmaturePackage &dataPackage, const SlotData *slotData, Armature *armature) const {
	auto slot = BaseObject::borrowObject<Slot_GD>();
	auto mesh_display{ DragonBonesMeshDisplay::from_pool() };

	Ref<DragonBonesSlot> tree_slot{ memnew(DragonBonesSlot(slot)) };
	slot->wrapper = tree_slot;
	slot->init(slotData, armature, mesh_display, mesh_display);
	slot->update(0);

	const auto proxy = static_cast<DragonBonesArmature *>(slot->getArmature()->getDisplay());
	proxy->add_slot(to_gd_str(slot->getName()), tree_slot);

	return slot;
}

Armature *DragonBonesFactory::_buildChildArmature(const BuildArmaturePackage *dataPackage, Slot *slot, DisplayData *displayData) const {
	const auto proxy = static_cast<DragonBonesArmature *>(slot->getArmature()->getDisplay());

	DragonBonesArmature *childArmature = nullptr;

	if (dataPackage != nullptr) {
		childArmature = buildArmatureDisplay(displayData->path, dataPackage->dataName);
	} else {
		childArmature = buildArmatureDisplay(displayData->path, displayData->getParent()->parent->parent->name);
	}

	ERR_FAIL_NULL_V_MSG(childArmature, nullptr, "Child armature is null");
	return childArmature->getArmature();
}

void DragonBonesFactory::_buildBones(const BuildArmaturePackage &dataPackage, Armature *armature) const {
	for (const auto boneData : dataPackage.armature->sortedBones) {
		const auto bone = BaseObject::borrowObject<Bone>();
		bone->init(boneData, armature);

		DragonBonesArmature *display = static_cast<DragonBonesArmature *>(armature->getDisplay());
		Ref<DragonBonesBone> new_bone{ memnew(DragonBonesBone(bone, display)) };
		display->add_bone(to_gd_str(bone->getName()), new_bone);
	}

	for (const auto &pair : dataPackage.armature->constraints) {
		// TODO more constraint type.
		const auto constraint = BaseObject::borrowObject<IKConstraint>();
		constraint->init(pair.second, armature);
		armature->_addConstraint(constraint);
	}
}

///  对外接口成员  ///////////////////////////////////////////////////////////////

void make_dragon_bones_data_unref_texture_atlas_data(DBDragonBonesData *p_data, const dragonBones::TextureAtlasData *p_atlas) {
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
							if (image_display->texture && image_display->texture->parent == p_atlas) {
								image_display->texture = nullptr; // 清除对该图集的散图引用
							}
						}
					} else if (display->type == dragonBones::DisplayType::Mesh) {
						if (auto mesh_display = static_cast<dragonBones::MeshDisplayData *>(display)) {
							if (mesh_display->texture && mesh_display->texture->parent == p_atlas) {
								mesh_display->texture = nullptr; // 清除对该图集的散图引用
							}
						}
					}
				}
			}
		}
	}
}

// 收集该套自己的显示对象路径（DisplayData::path，缺省 = name）。
// 注意不含顶层文件夹路径。
LocalVector<std::string> DragonBonesFactory::collect_display_paths(DBDragonBonesData *p_data) const {
	LocalVector<std::string> display_path_set;
	for (const auto &armature_name : p_data->getArmatureNames()) {
		const auto armature_data = p_data->getArmature(armature_name);

		for (const auto &skin_kv : armature_data->skins) {
			for (const auto &slot_kv : skin_kv.second->getSlotDisplays()) {
				for (const auto display : slot_kv.second) {
					if (display == nullptr || display->type == dragonBones::DisplayType::Armature) {
						// 子骨架的 path 指向龙骨数据，不是纹理
						continue;
					}
					display_path_set.push_back(display->path);
				}
			}
		}
	}
	return display_path_set;
}

Error DragonBonesFactory::load_data(const GDDragonBonesData *p_data) {
	const std::string &cache_name = p_data->get_cache_name();
	const String ske_path = p_data->resolve_ske_path();
	const String texture_path = p_data->resolve_texture_source_path();

	// ---- 纹理先于骨架加载（图集模式）----
	// 图集模式必须在骨架之前解析：解析骨架时会经 _getTextureData 按缓存键找纹理，
	// 而图集以同一缓存键注册在这里。
	const bool is_scattered = p_data->get_texture_source_type() == GDDragonBonesData::TEXTURE_SOURCE_SCATTERED;
	if (!is_scattered && !texture_path.is_empty()) {
		auto raw_data = get_file_data(texture_path);
		ERR_FAIL_COND_V_MSG(raw_data.is_empty(), ERR_PARSE_ERROR, vformat("Load DragonBones tex file failed: \"%s\".", texture_path));
		String atlas_path = texture_path;
		const auto atlas = static_cast<DragonBonesTextureAtlasData *>(loadTextureAtlasData((const char *)raw_data.ptr(), &atlas_path, cache_name));
		ERR_FAIL_NULL_V_MSG(atlas, ERR_PARSE_ERROR, vformat("Parse failed: \"%s\"", texture_path));
	}

	// ---- 骨架 ----
	auto raw_data = get_file_data(ske_path);
	ERR_FAIL_COND_V_MSG(raw_data.is_empty(), ERR_PARSE_ERROR, vformat("Load DragonBones ske file failed: \"%s\".", ske_path));

	if (loadDragonBonesData((const char *)raw_data.ptr(), cache_name) == nullptr) {
		ERR_PRINT(vformat("Parse failed: \"%s\"", ske_path));
		return ERR_PARSE_ERROR;
	}

	// ---- 散图图集（必须在骨架之后）----
	// 散图要遍历该套骨架数据里的显示路径来收集纹理名，故必须在骨架加载后。
	if (is_scattered && !texture_path.is_empty()) {
		auto dragon_bones_data = getDragonBonesData(cache_name);
		ERR_FAIL_NULL_V_MSG(dragon_bones_data, ERR_DOES_NOT_EXIST, vformat("Scattered textures require skeleton data, but \"%s\" was not loaded.", ske_path));

		const auto display_path_set = collect_display_paths(dragon_bones_data);

		// 不加载整图（display_texture 为空）：每个显示对象对应一张独立纹理
		// （region = 全图，纹理与尺寸挂在 TextureData 上）。
		const auto atlas = static_cast<DragonBonesTextureAtlasData *>(_buildTextureAtlasData(nullptr, nullptr));

		std::vector<String> missing;
		for (const auto &path : display_path_set) {
			const String png_path = texture_path.path_join(to_gd_str(path) + ".png");
			const Ref<Texture2D> texture = ResourceLoader::get_singleton()->load(png_path);
			if (texture.is_null()) {
				missing.push_back(png_path);
				continue;
			}

			const Size2 size = texture->get_size();
			auto texture_data = BaseObject::borrowObject<DragonBonesTextureDataScattered>();
			texture_data->name = path;
			texture_data->rotated = false;
			texture_data->region.x = 0.0f;
			texture_data->region.y = 0.0f;
			texture_data->region.width = size.x;
			texture_data->region.height = size.y;
			texture_data->texture = texture;

			atlas->addTexture(texture_data);
		}

		// 以同一个缓存键注册，保证 _getTextureData(缓存键, 纹理名) 能命中。
		addTextureAtlasData(atlas, cache_name);

		if (!missing.empty()) {
			// 缺失 = 该套散图目录缺文件（美术漏导出/文件被删）。汇总一次报全，
			// 不影响其他套与其他部件。
			for (const auto &png_path : missing) {
				ERR_PRINT(vformat("Load scattered texture failed: \"%s\".", png_path));
			}
			return ERR_PARSE_ERROR;
		}
	}

	return OK;
}

Error DragonBonesFactory::ensure_data_loaded(const GDDragonBonesData *p_data) {
	ERR_FAIL_NULL_V(p_data, ERR_INVALID_PARAMETER);

	// 键由资源身份决定，总是非空。
	const std::string &key = p_data->get_cache_name();

	// 不维护单独的「已加载」集合：工厂自己的数据 map 就是唯一事实来源 ——
	// 查得到即已加载，查不到即未加载，不会出现两份状态不一致。
	if (getDragonBonesData(key) != nullptr) {
		return OK;
	}

	// 失败时清理半成品：散图模式可能在骨架已入 map 之后才因缺纹理失败，
	// 不清掉的话「能查到 = 已加载」会把这次失败误判成成功。
	const Error err = load_data(p_data);
	if (err != OK) {
		release_data(key);
	}
	return err;
}

void DragonBonesFactory::release_data(const std::string &p_cache_name) {
	if (p_cache_name.empty()) {
		return;
	}

	// 该套的散图图集（按套清理，别人注册的不动）
	if (auto *atlas_vec = getTextureAtlasData(p_cache_name)) {
		auto dragon_bones_data = getDragonBonesData(p_cache_name);
		for (const auto atlas : *atlas_vec) {
			if (dragon_bones_data != nullptr) {
				make_dragon_bones_data_unref_texture_atlas_data(dragon_bones_data, atlas);
			}
			if (atlas_vec != nullptr) {
				const auto pos = std::find(atlas_vec->begin(), atlas_vec->end(), atlas);
				if (pos != atlas_vec->end()) {
					atlas_vec->erase(pos);
				}
			}
			atlas->returnToPool();
		}
	}

	// 骨架数据（会连带清掉其 armature/skin 对该套图集的引用）
	removeDragonBonesData(p_cache_name, true);
	// 剩余图集（图集模式下由此清理）
	removeTextureAtlasData(p_cache_name, true);
}

PackedStringArray DragonBonesFactory::get_armature_names(const std::string &p_cache_name) const {
	PackedStringArray ret;
	DBDragonBonesData *dbdata = getDragonBonesData(p_cache_name);
	ERR_FAIL_NULL_V(dbdata, ret);

	for (const auto &name : dbdata->getArmatureNames()) {
		ret.push_back(to_gd_str(name));
	}
	return ret;
}

PackedStringArray DragonBonesFactory::get_skin_names(const std::string &p_cache_name, const String &p_armature_name) const {
	PackedStringArray ret;
	DBDragonBonesData *dbdata = getDragonBonesData(p_cache_name);
	ERR_FAIL_NULL_V(dbdata, ret);

	ArmatureData *armature = dbdata->getArmature(to_std_str(p_armature_name));
	if (armature == nullptr && !dbdata->armatureNames.empty()) {
		armature = dbdata->getArmature(dbdata->armatureNames[0]);
	}
	ERR_FAIL_NULL_V(armature, ret);

	for (const auto &kv : armature->skins) {
		if (kv.second) {
			ret.push_back(to_gd_str(kv.second->name));
		}
	}
	return ret;
}

DragonBonesArmature *DragonBonesFactory::create_armature(DragonBonesArmatureView *p_owner, const std::string &p_cache_name, const String &p_armature_name, const String &p_skin_name) {
	ERR_FAIL_NULL_V(p_owner, nullptr);
	auto dragon_bones = DragonBones::get_singleton();
	ERR_FAIL_NULL_V(dragon_bones, nullptr);

	DBDragonBonesData *dragon_bones_data = getDragonBonesData(p_cache_name);
	ERR_FAIL_NULL_V_MSG(dragon_bones_data, nullptr, vformat("DragonBones data \"%s\" is not loaded by the factory.", to_gd_str(p_cache_name)));
	ERR_FAIL_COND_V(dragon_bones_data->armatureNames.empty(), nullptr);

	std::string armature_name = to_std_str(p_armature_name);
	const auto &armature_names = dragon_bones_data->getArmatureNames();
	if (p_armature_name.is_empty() || std::find(armature_names.begin(), armature_names.end(), armature_name) == armature_names.end()) {
		armature_name = armature_names[0];
	}

	_dragonBones = dragon_bones->get_dragon_bones_instance();
	building_armature = p_owner;
	auto ret = buildArmatureDisplay(armature_name, p_cache_name, to_std_str(p_skin_name));
	building_armature = nullptr;
	_dragonBones = nullptr;

	return ret;
}

//////////////////////////////////////////////////////////////////
// *.dbdata 资源格式读写
//////////////////////////////////////////////////////////////////

namespace {
constexpr const char *SECTION_PROPERTY = "properties";
constexpr const char *SECTION_OTHER = "other";

constexpr const char *KEY_UID = "UID";
constexpr const char *KEY_VERSION = "VERSION";
} //namespace

Error DragonBonesDataFileProcessor::parse_dbdata_file(const String &p_path, Ref<GDDragonBonesData> &r_data, int64_t &r_uid) {
	Ref<ConfigFile> cfg;
	cfg.instantiate();
	const Error err = cfg->load(p_path);
	ERR_FAIL_COND_V(err != OK, err);

	if (r_data.is_null()) {
		r_data.instantiate();
	}

	r_data->set_ske_file(cfg->get_value(SECTION_PROPERTY, "ske_file", String()));
	r_data->set_texture_source_type((GDDragonBonesData::TextureSourceType)(int)cfg->get_value(SECTION_PROPERTY, "texture_source_type", 0));
	r_data->set_texture_source(cfg->get_value(SECTION_PROPERTY, "texture_source", String()));
	r_data->imported = cfg->get_value(SECTION_OTHER, "imported", false);
	r_uid = ResourceUID::get_singleton()->text_to_id(cfg->get_value("", KEY_UID, ResourceUID::get_singleton()->id_to_text(ResourceUID::INVALID_ID)));
	return OK;
}

Error DragonBonesDataFileProcessor::save_dbdata_file(const String &p_path, const Ref<GDDragonBonesData> &p_data, int64_t p_uid) {
	ERR_FAIL_NULL_V(p_data, FAILED);

	Ref<ConfigFile> cfg;
	cfg.instantiate();

	cfg->set_value("", KEY_VERSION, "1.0.0"); // 对保存格式也进行版本管理
	cfg->set_value("", KEY_UID, ResourceUID::get_singleton()->id_to_text(p_uid));

	cfg->set_value(SECTION_PROPERTY, "ske_file", p_data->get_ske_file());
	cfg->set_value(SECTION_PROPERTY, "texture_source_type", (int)p_data->get_texture_source_type());
	cfg->set_value(SECTION_PROPERTY, "texture_source", p_data->get_texture_source());
	cfg->set_value(SECTION_OTHER, "imported", p_data->imported);

	return cfg->save(p_path);
}

// ===========================================
bool ResourceFormatSaverDragonBonesData::_recognize(const Ref<Resource> &resource) const {
	return cast_to<GDDragonBonesData>(resource.ptr());
}

PackedStringArray ResourceFormatSaverDragonBonesData::_get_recognized_extensions(const Ref<Resource> &resource) const {
	if (cast_to<GDDragonBonesData>(resource.ptr())) {
		return Array::make(GDDragonBonesData::SAVED_EXT);
	}
	return {};
}

Error ResourceFormatSaverDragonBonesData::_set_uid(const String &p_path, int64_t p_uid) {
	if (p_path.get_extension().to_lower() != GDDragonBonesData::SAVED_EXT) {
		return ERR_FILE_UNRECOGNIZED;
	}

	Ref<GDDragonBonesData> data;
	int64_t _uid = ResourceUID::INVALID_ID;
	const Error err = this->parse_dbdata_file(p_path, data, _uid);
	ERR_FAIL_COND_V(err != OK, err);

	return this->save_dbdata_file(p_path, data, p_uid);
}

Error ResourceFormatSaverDragonBonesData::_save(const Ref<Resource> &resource, const String &path, uint32_t flags) {
	Ref<GDDragonBonesData> data = resource;
	ERR_FAIL_NULL_V(data, ERR_INVALID_PARAMETER);

	int64_t uid = ResourceLoader::get_singleton()->get_resource_uid(path);
	if (uid == ResourceUID::INVALID_ID) {
		uid = ResourceUID::get_singleton()->create_id();
		if (uid != ResourceUID::INVALID_ID) {
			ResourceUID::get_singleton()->set_id(uid, path);
		}
	}

	return this->save_dbdata_file(path, data, uid);
}

// ===========================================
PackedStringArray ResourceFormatLoaderDragonBonesData::_get_recognized_extensions() const {
	return Array::make(GDDragonBonesData::SAVED_EXT);
}

bool ResourceFormatLoaderDragonBonesData::_handles_type(const StringName &type) const {
	return type == GDDragonBonesData::get_class_static() || ClassDB::is_parent_class(type, GDDragonBonesData::get_class_static());
}

String ResourceFormatLoaderDragonBonesData::_get_resource_type(const String &path) const {
	if (path.get_extension().to_lower() == GDDragonBonesData::SAVED_EXT) {
		return GDDragonBonesData::get_class_static();
	}
	return "";
}

int64_t ResourceFormatLoaderDragonBonesData::_get_resource_uid(const String &path) const {
	if (path.get_extension().to_lower() != GDDragonBonesData::SAVED_EXT) {
		// 这里应该由 godot 引擎自身处理才对 （_get_recognized_extensions）。
		return ResourceUID::INVALID_ID;
	}

	Ref<GDDragonBonesData> data;
	int64_t uid = ResourceUID::INVALID_ID;
	const Error err = this->parse_dbdata_file(path, data, uid);

	if (err != OK) {
		ERR_PRINT(vformat("Get uid of '%s' failed: %s.", path, UtilityFunctions::error_string(err)));
		return ResourceUID::INVALID_ID;
	}

	return uid;
}

Variant ResourceFormatLoaderDragonBonesData::_load(const String &path, const String &original_path, bool use_sub_threads, int32_t cache_mode) const {
	Ref<GDDragonBonesData> ret;
	int64_t uid = ResourceUID::INVALID_ID;
	const Error err = this->parse_dbdata_file(path, ret, uid);

	if (err != OK) {
		return err;
	}

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		// 编辑器内补建 UID 并回写，避免每次加载都缺 UID。
		if (uid == ResourceUID::INVALID_ID) {
			if (FileAccess::file_exists(path)) {
				uid = ResourceUID::get_singleton()->create_id();
				if (this->save_dbdata_file(path, ret, uid) != OK) {
					ResourceUID::get_singleton()->remove_id(uid);
				}
			}
		}
	}
#endif // TOOLS_ENABLED

	if (uid != ResourceUID::INVALID_ID) {
		if (ResourceUID::get_singleton()->has_id(uid)) {
			ResourceUID::get_singleton()->set_id(uid, path);
		} else {
			ResourceUID::get_singleton()->add_id(uid, path);
		}
	}

	return ret;
}
