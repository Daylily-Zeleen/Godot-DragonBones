#include "mesh_display.h"
#include <dragonbones_armature.h>

namespace godot {

void MeshDisplay::update_modulate(const Color &p_modulate) {
	modulate = p_modulate;
	verticesColor.fill(p_modulate);
}

DragonBonesArmature *MeshDisplay::get_armature() const {
	ERR_FAIL_NULL_V(slot->getArmature()->getProxy(), nullptr);
	return static_cast<DragonBonesArmature *>(slot->getArmature()->getProxy());
}

void MeshDisplay::queue_redraw() const {
	ERR_FAIL_NULL(get_armature());
	get_armature()->queue_redraw();
}

void MeshDisplay::append_draw_data(VMap<int, LocalVector<DrawData>> &r_data) const {
	if (!slot->getVisible()) {
		return;
	}

	// auto d = std::make_unique<DrawData>();
	// d->z_order = slot->_zOrder;

	// if (armature) {
	// 	d->texture = armature->get_texture_override();
	// } else {
	// 	d->texture = slot->get_texture();
	// }

	// d->vertices = verticesPos;
	// d->indices = indices;
	// d->colors = verticesColor;
	// d->uvs = verticesUV;

	// d->vertices.resize(d->vertices.size() + verticesPos.size());
	// d->indices.resize(d->indices.size() + indices.size());
	// d->colors.resize(d->colors.size() + verticesColor.size());
	// d->uvs.resize(d->uvs.size() + verticesUV.size());

	// auto vertices_ptr = d->vertices.ptr() + d->vertices.size();
	// auto indices_ptr = d->indices.ptr() + d->indices.size();
	// auto colors_ptr = d->colors.ptr() + d->colors.size();
	// auto uvs_ptr = d->uvs.ptr() + d->uvs.size();

	// memcpy(vertices_ptr, verticesPos.ptr(), sizeof(Vector2) * verticesPos.size());
	// memcpy(indices_ptr, indices.ptr(), sizeof(int32_t) * indices.size());
	// memcpy(colors_ptr, verticesColor.ptr(), sizeof(Color) * verticesColor.size());
	// memcpy(uvs_ptr, verticesUV.ptr(), sizeof(Vector2) * verticesUV.size());

	if (!r_data.has(slot->_zOrder)) {
		r_data.insert(slot->_zOrder, LocalVector<DrawData>());
	}
	auto armature = get_armature();
	r_data[slot->_zOrder].push_back({ slot->_zOrder,
			armature && armature->get_texture_override().is_valid() ? armature->get_texture_override() : slot->get_texture(),
			verticesPos,
			indices,
			verticesColor,
			verticesUV });
	// ({ .z_order = slot->_zOrder,
	// 		.texture = armature ? armature->get_texture_override() : slot->get_texture(),
	// 		.vertices = verticesPos,
	// 		.indices = indices,
	// 		.colors = verticesColor,
	// 		.uvs = verticesUV });
}
} //namespace godot