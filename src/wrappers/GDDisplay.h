#pragma once

#include "dragonBones/event/EventObject.h"
#include "godot_cpp/classes/canvas_item_material.hpp"
#include "godot_cpp/classes/node2d.hpp"

#include "dragonBones/DragonBonesHeaders.h"
DRAGONBONES_HEADERS_H // 无用代码，仅用于消除 IDE 警告

namespace godot {

class GDOwnerNode : public Node2D {
public:
	GDOwnerNode() = default;
	virtual ~GDOwnerNode() = default;

	virtual void dispatch_event(const String &_str_type, const dragonBones::EventObject *_p_value) = 0;
	virtual void dispatch_sound_event(const String &_str_type, const dragonBones::EventObject *_p_value) = 0;

	virtual Ref<CanvasItemMaterial> get_material_to_set_blend_mode(bool p_required) = 0;
};

class GDDisplay : public GDOwnerNode {
private:
	GDDisplay(const GDDisplay &);

public:
	Ref<Texture2D> texture;
	GDOwnerNode *p_owner{ nullptr };
	bool b_debug{ false };

public:
	GDDisplay() = default;
	virtual ~GDDisplay() = default;

	void set_blend_mode(CanvasItemMaterial::BlendMode p_blend_mode) {
		// 仅能对 CanvasItemMaterial 进行处理
		// TOOD: 如果以后 CanvasItem 支持实例的着色器参数，可以考虑对其进行设置，以支持 ShaderMaterial
		Ref<CanvasItemMaterial> mat = get_material_to_set_blend_mode(p_blend_mode == CanvasItemMaterial::BLEND_MODE_MIX);
		if (mat.is_valid()) {
			mat->set_blend_mode(p_blend_mode);
		}
	}

	virtual void update_modulate(const Color &p_modulate) { set_modulate(p_modulate); }
};

}; //namespace godot
