#pragma once

#include "dragonbones_factory.h"
#include "godot_cpp/classes/editor_export_plugin.hpp"
#include "godot_cpp/classes/editor_import_plugin.hpp"
#include "godot_cpp/classes/editor_plugin.hpp"
#include <godot_cpp/templates/hash_map.hpp>

namespace godot {
// TODO 实现运行时用的 ResourceFormatLoader

class DragonBonesExportPlugin : public EditorExportPlugin {
	GDCLASS(DragonBonesExportPlugin, EditorExportPlugin)

	PackedStringArray added_files;

protected:
	static void _bind_methods();

public:
	virtual String _get_name() const override;

	virtual void _export_begin(const PackedStringArray &features, bool is_debug, const String &path, uint32_t flags) override { added_files.clear(); }
	virtual void _export_end() override { added_files.clear(); }
	virtual void _export_file(const String &path, const String &type, const PackedStringArray &features) override;
};

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
	virtual decltype(EditorImportPlugin()._get_priority()) _get_priority() const override;
	virtual int32_t _get_import_order() const override;
	virtual bool _get_option_visibility(const String &path, const StringName &option_name, const Dictionary &options) const override;
	virtual Error _import(const String &p_source_file, const String &p_save_path, const Dictionary &p_options,
			const TypedArray<String> &r_platform_variants, const TypedArray<String> &r_gen_files) const override;

public:
	Ref<DragonBonesFactory> try_import(const String &p_ske_file, DragonBonesFactory *p_factory = nullptr) const;
};

class DragonBonesEditorPlugin : public EditorPlugin {
	GDCLASS(DragonBonesEditorPlugin, EditorPlugin)

	Ref<DragonBonesExportPlugin> export_plugin;
	Ref<DragonBonesImportPlugin> import_plugin;

	HashMap<String, String> moved_factory_files;

	void _on_file_system_dock_files_moved(const String &p_old_file, const String &p_new_file);
	void _on_filesystem_changed();
	void _reimport_dbfactory_recursively(class EditorFileSystemDirectory *p_dir, HashMap<String, Ref<DragonBonesFactory>> &r_factories) const;
	void _reimport_moved_factory_files();

	void clear_reimporting_flag();

protected:
	static void _bind_methods() {}

public:
	virtual void _enter_tree() override;
	virtual void _exit_tree() override;
};

} //namespace godot