/**************************************************************************/
/*  dragon_bones_registration.cpp                                         */
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
