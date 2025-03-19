#pragma once
#include "GDDisplay.h"
#include "dragonBones/event/EventObject.h"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/variant/utility_functions.hpp"
#include <godot_cpp/classes/engine.hpp>

namespace godot {

class GDMesh : public GDDisplay {
	GDCLASS(GDMesh, Node2D)
private:
	GDMesh(const GDMesh &);

protected:
	static void _bind_methods() {}
	_DEFINE_TO_STRING()

public:
	PackedInt32Array indices;
	PackedColorArray verticesColor;
	PackedVector2Array verticesUV;
	PackedVector2Array verticesPos;

	Color col_debug{
		static_cast<float>(UtilityFunctions::randf_range(0.5f, 1.0f)),
		static_cast<float>(UtilityFunctions::randf_range(0.3f, 1.0f)),
		static_cast<float>(UtilityFunctions::randf_range(0.3f, 1.0f)),
		1
	};

	RID debug_mesh;

public:
	GDMesh() = default;
	virtual ~GDMesh() override = default;

	virtual Ref<CanvasItemMaterial> get_material_to_set_blend_mode(bool p_required) override {
		if (get_use_parent_material()) {
			auto parent = dynamic_cast<GDOwnerNode *>(get_parent());
			if (parent) {
				return parent->get_material_to_set_blend_mode(p_required);
			}
		}

		Ref<CanvasItemMaterial> ret = get_material();
		if (ret.is_null() && p_required) {
			ret.instantiate();
			set_material(ret);
		}
		return ret;
	}

	virtual void dispatch_event(const String &_str_type, const dragonBones::EventObject *_p_value) override {
		if (p_owner) {
			p_owner->dispatch_event(_str_type, _p_value);
		}
	}

	virtual void dispatch_sound_event(const String &_str_type, const dragonBones::EventObject *_p_value) override {
		if (p_owner) {
			p_owner->dispatch_sound_event(_str_type, _p_value);
		}
	}

	virtual void _draw() override {
		if (indices.is_empty()) {
			return;
		}

		auto owner = static_cast<GDDisplay *>(p_owner);
		const Ref<Texture2D> texture_to_draw = owner && owner->texture.is_valid() ? owner->texture : this->texture;

		if (texture_to_draw.is_valid()) {
			RenderingServer::get_singleton()->canvas_item_add_triangle_array(
					get_canvas_item(),
					indices,
					verticesPos,
					verticesColor,
					verticesUV,
					{},
					{},
					texture_to_draw->get_rid(),
					-1);
		}

#ifdef DEBUG_ENABLED
		if (b_debug) {
			if (!debug_mesh.is_valid()) {
				debug_mesh = RenderingServer::get_singleton()->mesh_create();
			}

			PackedInt32Array lines_indices;
			for (int i = 0; i < indices.size(); i += 3) {
				lines_indices.push_back(indices[i]);
				lines_indices.push_back(indices[i + 1]);

				lines_indices.push_back(indices[i + 1]);
				lines_indices.push_back(indices[i + 2]);

				lines_indices.push_back(indices[i + 2]);
				lines_indices.push_back(indices[i]);
			}

			Array arrays;
			arrays.resize(RenderingServer::ARRAY_MAX);
			arrays[RenderingServer::ARRAY_VERTEX] = verticesPos;
			arrays[RenderingServer::ARRAY_INDEX] = lines_indices;

			RenderingServer::get_singleton()->mesh_clear(debug_mesh);
			RenderingServer::get_singleton()->mesh_add_surface_from_arrays(debug_mesh, RenderingServer::PRIMITIVE_LINES, arrays);
			RenderingServer::get_singleton()->canvas_item_add_mesh(get_canvas_item(), debug_mesh, get_canvas_transform(), col_debug);
		} else {
			if (debug_mesh.is_valid()) {
				RenderingServer::get_singleton()->free_rid(debug_mesh);
				debug_mesh = RID();
			}
		}
#endif // DEBUG_ENABLED
	}

	virtual void update_modulate(const Color &p_modulate) override {
		GDDisplay::update_modulate(p_modulate);
		verticesColor.fill(p_modulate);
	}
};

} //namespace godot