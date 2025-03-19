#pragma once

#include "dragonBones/event/EventObject.h"
#include "godot_cpp/classes/canvas_item_material.hpp"
#include "godot_cpp/classes/texture2d.hpp"
#include "godot_cpp/templates/local_vector.hpp"

#include "dragonBones/DragonBonesHeaders.h"
DRAGONBONES_HEADERS_H; // 无用代码，仅用于消除 IDE 警告

namespace godot {

_FORCE_INLINE_ String to_gd_str(const std::string &p_std_str) {
	return String::utf8(p_std_str.c_str());
}

_FORCE_INLINE_ std::string to_std_str(const String &p_gd_str) {
	return p_gd_str.utf8().get_data();
}

#define _DEFINE_TO_STRING() \
	::godot::String _to_string() const { return vformat("<%s#%s>", get_class_static(), get_instance_id()); }

class IDragonBonesOwner {
public:
	IDragonBonesOwner() = default;
	virtual ~IDragonBonesOwner() = default;

	virtual void dispatch_event(const String &_str_type, const dragonBones::EventObject *_p_value) = 0;
	virtual void dispatch_sound_event(const String &_str_type, const dragonBones::EventObject *_p_value) = 0;
	virtual Ref<CanvasItemMaterial> get_material_to_set_blend_mode(bool p_required) = 0;

	virtual void request_redraw() = 0;
};

class IDragonBonesDisplay {
private:
	IDragonBonesDisplay(const IDragonBonesDisplay &);
	friend class Slot_GD;

protected:
	class Slot_GD *slot{ nullptr };

public:
	IDragonBonesOwner *p_owner{ nullptr };
	bool b_debug{ false };

	int z_index{ 0 };
	bool visible{ true };

public:
	IDragonBonesDisplay() = default;
	virtual ~IDragonBonesDisplay();

	virtual void set_blend_mode(CanvasItemMaterial::BlendMode p_blend_mode) = 0;
	// {
	// 	// 仅能对 CanvasItemMaterial 进行处理
	// 	// TODO: 如果以后 CanvasItem 支持实例的着色器参数，可以考虑对其进行设置，以支持 ShaderMaterial
	// 	Ref<CanvasItemMaterial> mat = get_material_to_set_blend_mode(p_blend_mode == CanvasItemMaterial::BLEND_MODE_MIX);
	// 	if (mat.is_valid()) {
	// 		mat->set_blend_mode(p_blend_mode);
	// 	}
	// }

	virtual void update_modulate(const Color &p_modulate) = 0; //{ set_self_modulate(p_modulate); }

	virtual Ref<Texture2D> get_draw_texture() const = 0;
	virtual void append_draw_info(LocalVector<Vector2> &r_vertices, LocalVector<int32_t> &r_indices, LocalVector<Color> &r_colors, LocalVector<Vector2> &r_uvs) const = 0;
};

}; //namespace godot
