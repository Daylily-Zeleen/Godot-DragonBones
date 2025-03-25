#include "dragonbones_registration.h"

#include <godot_cpp/classes/editor_plugin_registration.hpp>

#ifdef TOOLS_ENABLED
#include <editor/dragonbones_editor_plugin.h>
#endif //TOOLS_ENABLED

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>

#include "dragonbones.h"

using namespace godot;

static ResourceFormatSaverDragonBones *saver;
static ResourceFormatLoaderDragonBones *loader;

void initialize_gddragonbones_module(godot::ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_INTERNAL_CLASS(DragonBonesExportPlugin);
		GDREGISTER_INTERNAL_CLASS(DragonBonesImportPlugin);
		GDREGISTER_INTERNAL_CLASS(DragonBonesEditorPlugin);
		EditorPlugins::add_by_type<DragonBonesEditorPlugin>();
		GDREGISTER_INTERNAL_CLASS(DragonBonesArmatureProxy);
	}
#endif // TOOLS_ENABLED

	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(DragonBonesFactory);
	GDREGISTER_CLASS(DragonBones);

	GDREGISTER_ABSTRACT_CLASS(DragonBonesBone);
	GDREGISTER_ABSTRACT_CLASS(DragonBonesSlot);
	GDREGISTER_ABSTRACT_CLASS(DragonBonesArmature);
	GDREGISTER_ABSTRACT_CLASS(DragonBonesUserData);

	GDREGISTER_INTERNAL_CLASS(ResourceFormatSaverDragonBones);
	GDREGISTER_INTERNAL_CLASS(ResourceFormatLoaderDragonBones);

	saver = memnew(ResourceFormatSaverDragonBones);
	saver->reference();
	ResourceSaver::get_singleton()->add_resource_format_saver(saver);

	loader = memnew(ResourceFormatLoaderDragonBones);
	loader->reference();
	ResourceLoader::get_singleton()->add_resource_format_loader(loader);
}

void uninitialize_gddragonbones_module(godot::ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::remove_by_type<DragonBonesEditorPlugin>();
	}
#endif // TOOLS_ENABLED
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	// 清除对象池
	dragonBones::BaseObject::clearPool();

	ResourceSaver::get_singleton()->remove_resource_format_saver(saver);
	saver->unreference();
	memdelete(saver);

	ResourceLoader::get_singleton()->remove_resource_format_loader(loader);
	loader->unreference();
	memdelete(loader);
}
