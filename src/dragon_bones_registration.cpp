#include "dragon_bones_registration.h"

#include <godot_cpp/classes/editor_plugin_registration.hpp>

#ifdef TOOLS_ENABLED
#include <editor/dragon_bones_editor_plugin.h>
#endif //TOOLS_ENABLED

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_saver.hpp>

#include "dragon_bones.h"

#include "armature_view.h"
#include "event_object.h"

using namespace godot;

static DragonBones *dragon_bones{ nullptr };
static Ref<ResourceFormatSaverDragonBones> saver;
static Ref<ResourceFormatLoaderDragonBones> loader;

void initialize_godot_dragon_bones_module(godot::ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_INTERNAL_CLASS(DragonBonesExportPlugin);
		GDREGISTER_INTERNAL_CLASS(DragonBonesImportPlugin);
		GDREGISTER_INTERNAL_CLASS(DragonBonesEditorPlugin);
		GDREGISTER_INTERNAL_CLASS(DragonBonesArmatureProxy);

		EditorPlugins::add_by_type<DragonBonesEditorPlugin>();
	}
#endif // TOOLS_ENABLED

	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(DragonBonesFactory);
		GDREGISTER_CLASS(DragonBonesArmatureView);

		GDREGISTER_ABSTRACT_CLASS(DragonBonesBone);
		GDREGISTER_ABSTRACT_CLASS(DragonBonesSlot);
		GDREGISTER_ABSTRACT_CLASS(DragonBonesArmature);
		GDREGISTER_ABSTRACT_CLASS(DragonBonesUserData);
		GDREGISTER_ABSTRACT_CLASS(DragonBonesEventObject);

		GDREGISTER_INTERNAL_CLASS(ResourceFormatSaverDragonBones);
		GDREGISTER_INTERNAL_CLASS(ResourceFormatLoaderDragonBones);

		dragon_bones = memnew(DragonBones);

		saver.instantiate();
		ResourceSaver::get_singleton()->add_resource_format_saver(saver);

		loader.instantiate();
		ResourceLoader::get_singleton()->add_resource_format_loader(loader);
	}
}

void uninitialize_godot_dragon_bones_module(godot::ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::remove_by_type<DragonBonesEditorPlugin>();
	}
#endif // TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// 清空对象池
		dragonBones::BaseObject::clearPool();
		DragonBonesMeshDisplay::clear_pool();

		ResourceSaver::get_singleton()->remove_resource_format_saver(saver);
		saver.unref();

		ResourceLoader::get_singleton()->remove_resource_format_loader(loader);
		loader.unref();

		dragon_bones->flush();
		memdelete(dragon_bones);
	}
}
