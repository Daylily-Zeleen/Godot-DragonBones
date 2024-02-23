#include "dragon_bones_registration.h"

#include "dragonbones.h"
#include "godot_cpp/classes/editor_plugin_registration.hpp"

#include "editor/dragonbones_editor_plugin.h"

#include "wrappers/GDMesh.h"

using namespace godot;

void initialize_dragonbones_module(godot::ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_INTERNAL_CLASS(DragonBonesImportPlugin);
		GDREGISTER_INTERNAL_CLASS(DragonBonesEditorPlugin);
		EditorPlugins::add_by_type<DragonBonesEditorPlugin>();
#ifdef TOOLS_ENABLED
		GDREGISTER_INTERNAL_CLASS(DragonBonesArmatureProxy);
#endif
	}

	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	GDREGISTER_CLASS(DragonBonesFactory);
	GDREGISTER_CLASS(DragonBones);

	GDREGISTER_INTERNAL_CLASS(GDMesh);
	GDREGISTER_ABSTRACT_CLASS(DragonBonesBone);
	GDREGISTER_ABSTRACT_CLASS(DragonBonesSlot);
	GDREGISTER_ABSTRACT_CLASS(DragonBonesArmature);
	GDREGISTER_ABSTRACT_CLASS(DragonBonesUserData);
}

void uninitialize_dragonbones_module(godot::ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		EditorPlugins::remove_by_type<DragonBonesEditorPlugin>();
	}
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	// 清除对象池
	dragonBones::BaseObject::clearPool();
}
