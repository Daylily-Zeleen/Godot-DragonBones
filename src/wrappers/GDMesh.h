#pragma once
#include "GDDisplay.h"
#include "dragonBones/event/EventObject.h"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/variant/utility_functions.hpp"

namespace godot {

class GDMesh : public GDDisplay {
	GDCLASS(GDMesh, Node2D)
private:
	GDMesh(const GDMesh &);

protected:
	static void _bind_methods() {}

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

public:
	GDMesh() = default;
	virtual ~GDMesh() = default;

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
		if (indices.is_empty())
			return;

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
					texture_to_draw.is_valid() ? texture_to_draw->get_rid() : RID(),
					-1);
		}

		if (b_debug || texture_to_draw.is_null()) {
			for (int idx = 0; idx < indices.size(); idx += 3) {
				RenderingServer::get_singleton()->canvas_item_add_line(get_canvas_item(), verticesPos[indices[idx]], verticesPos[indices[idx + 1]], col_debug, 1.0);
				RenderingServer::get_singleton()->canvas_item_add_line(get_canvas_item(), verticesPos[indices[idx + 1]], verticesPos[indices[idx + 2]], col_debug, 1.0);
				RenderingServer::get_singleton()->canvas_item_add_line(get_canvas_item(), verticesPos[indices[idx + 2]], verticesPos[indices[idx]], col_debug, 1.0);
			}
		}
	}

	virtual void update_modulate(const Color &p_modulate) override {
		set_modulate(p_modulate);
		verticesColor.fill(p_modulate);
	}
};

} //namespace godot