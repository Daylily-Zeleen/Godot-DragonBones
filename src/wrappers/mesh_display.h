#pragma once
#include "dragonBones/event/EventObject.h"
#include "godot_cpp/classes/rendering_server.hpp"
#include "godot_cpp/variant/utility_functions.hpp"
#include "i_dragonbones_display.h"
#include <godot_cpp/classes/engine.hpp>

namespace godot {

class MeshDisplay : public IDragonBonesDisplay, public IDragonBonesOwner {
private:
	MeshDisplay(const MeshDisplay &);

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
		1.0f,
	};

	RID debug_mesh;

public:
	MeshDisplay() = default;

	virtual Ref<CanvasItemMaterial> get_material_to_set_blend_mode(bool p_required) override;
	virtual void dispatch_event(const String &_str_type, const dragonBones::EventObject *_p_value) override;
	virtual void dispatch_sound_event(const String &_str_type, const dragonBones::EventObject *_p_value) override;

#ifdef DEBUG_ENABLED
	// virtual void _draw() override;
#endif // DEBUG_ENABLED

	virtual void set_blend_mode(CanvasItemMaterial::BlendMode p_blend_mode) override {}
	virtual void update_modulate(const Color &p_modulate) override;
	virtual void request_redraw() override;

	virtual Ref<Texture2D> get_draw_texture() const override;
	virtual void append_draw_info(LocalVector<Vector2> &r_vertices, LocalVector<int32_t> &r_indices, LocalVector<Color> &r_colors, LocalVector<Vector2> &r_uvs) const override;
};

} //namespace godot