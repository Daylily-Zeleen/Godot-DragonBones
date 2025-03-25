#pragma once

#include <godot_cpp/classes/canvas_item_material.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/templates/vmap.hpp>

namespace godot {

struct DrawData {
	Transform2D transform;
	PackedVector2Array vertices;
	PackedInt32Array indices;
	PackedColorArray colors;
	PackedVector2Array vertices_uv;
	RID texture;
	CanvasItemMaterial::BlendMode blend_mode;
	int z_order = 0;
#ifdef DEBUG_ENABLED
	Color debug_color;
#endif // DEBUG_ENABLED
};

class Display {
protected:
	class Slot_GD *slot{ nullptr };
	friend class Slot_GD;

public:
	Transform2D transform{};

	virtual void queue_redraw() const = 0;
	virtual void append_draw_data(VMap<int, LocalVector<DrawData>> &r_data, const Transform2D &p_base_transfrom = Transform2D()) const = 0;
};

class DragonBonesMeshDisplay : public Display {
private:
	DragonBonesMeshDisplay(const DragonBonesMeshDisplay &);

public:
	PackedInt32Array indices;
	PackedColorArray colors;
	PackedVector2Array vertices_uv;
	PackedVector2Array vertices;
	CanvasItemMaterial::BlendMode blend_mode = CanvasItemMaterial::BLEND_MODE_ADD;

#ifdef DEBUG_ENABLED
	Color debug_color;
#endif // DEBUG_ENABLED

public:
	DragonBonesMeshDisplay();

	void set_blend_mode(CanvasItemMaterial::BlendMode p_blend_mode) {}
	void update_modulate(const Color &p_modulate);
	class DragonBonesArmature *get_armature() const;

	virtual void queue_redraw() const override;
	virtual void append_draw_data(VMap<int, LocalVector<DrawData>> &r_data, const Transform2D &p_base_transfrom = Transform2D()) const override;
};

} //namespace godot