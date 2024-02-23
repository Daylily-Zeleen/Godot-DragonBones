#pragma once

#include "godot_cpp/classes/editor_import_plugin.hpp"
#include "godot_cpp/classes/editor_plugin.hpp"

namespace godot {
// TODO 实现运行时用的 ResourceFormatLoader

class DragonBonesImportPlugin : public EditorImportPlugin {
	GDCLASS(DragonBonesImportPlugin, EditorImportPlugin)
protected:
	static void _bind_methods();

public:
	virtual String _get_importer_name() const override;
	virtual String _get_visible_name() const override;
	virtual int32_t _get_preset_count() const override;
	virtual String _get_preset_name(int32_t p_preset_index) const override;
	virtual PackedStringArray _get_recognized_extensions() const override;
	virtual TypedArray<Dictionary> _get_import_options(const String &p_path, int32_t p_preset_index) const override;
	virtual String _get_save_extension() const override;
	virtual String _get_resource_type() const override;
	virtual double _get_priority() const override;
	virtual int32_t _get_import_order() const override;
	virtual bool _get_option_visibility(const String &path, const StringName &option_name, const Dictionary &options) const override;
	virtual Error _import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options,
			const TypedArray<String> &r_platform_variants, const TypedArray<String> &r_gen_files) const override;
};

class DragonBonesEditorPlugin : public EditorPlugin {
	GDCLASS(DragonBonesEditorPlugin, EditorPlugin)

	Ref<DragonBonesImportPlugin> import_plugin;

protected:
	static void _bind_methods() {}

public:
	virtual void _enter_tree() override;
	virtual void _exit_tree() override;
	// 	func _enter_tree():
	// 	import_plugin = preload("import_plugin.gd").new()
	// 	add_import_plugin(import_plugin)

	// func _exit_tree():
	// 	remove_import_plugin(import_plugin)
	// 	import_plugin = null
};

} //namespace godot