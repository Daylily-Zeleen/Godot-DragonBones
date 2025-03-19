#include "mesh_display.h"
#include "dragonbones_armature.h"

namespace godot {
Ref<CanvasItemMaterial> MeshDisplay::get_material_to_set_blend_mode(bool p_required) {
	if (auto armature = dynamic_cast<DragonBonesArmature *>(p_owner)) {
		return armature->get_material_to_set_blend_mode(p_required);
	}
	return Ref<CanvasItemMaterial>();
}

void MeshDisplay::dispatch_event(const String &_str_type, const dragonBones::EventObject *_p_value) {
	if (p_owner) {
		p_owner->dispatch_event(_str_type, _p_value);
	}
}

void MeshDisplay::dispatch_sound_event(const String &_str_type, const dragonBones::EventObject *_p_value) {
	if (p_owner) {
		p_owner->dispatch_sound_event(_str_type, _p_value);
	}
}

// #ifdef DEBUG_ENABLED
// void MeshDisplay::_draw() {
// 	if (indices.is_empty()) {
// 		return;
// 	}

// 	// auto owner = static_cast<IDragonBonesDisplay *>(p_owner);
// 	// const Ref<Texture2D> texture_to_draw = get_draw_texture();

// 	// if (texture_to_draw.is_valid()) {
// 	// 	RenderingServer::get_singleton()->canvas_item_add_triangle_array(
// 	// 			get_canvas_item(),
// 	// 			indices,
// 	// 			verticesPos,
// 	// 			verticesColor,
// 	// 			verticesUV,
// 	// 			{},
// 	// 			{},
// 	// 			texture_to_draw->get_rid(),
// 	// 			-1);
// 	// }

// 	if (b_debug) {
// 		if (!debug_mesh.is_valid()) {
// 			debug_mesh = RenderingServer::get_singleton()->mesh_create();
// 		}

// 		PackedInt32Array lines_indices;
// 		for (int i = 0; i < indices.size(); i += 3) {
// 			lines_indices.push_back(indices[i]);
// 			lines_indices.push_back(indices[i + 1]);

// 			lines_indices.push_back(indices[i + 1]);
// 			lines_indices.push_back(indices[i + 2]);

// 			lines_indices.push_back(indices[i + 2]);
// 			lines_indices.push_back(indices[i]);
// 		}

// 		Array arrays;
// 		arrays.resize(RenderingServer::ARRAY_MAX);
// 		arrays[RenderingServer::ARRAY_VERTEX] = verticesPos;
// 		arrays[RenderingServer::ARRAY_INDEX] = lines_indices;

// 		RenderingServer::get_singleton()->mesh_clear(debug_mesh);
// 		RenderingServer::get_singleton()->mesh_add_surface_from_arrays(debug_mesh, RenderingServer::PRIMITIVE_LINES, arrays);
// 		RenderingServer::get_singleton()->canvas_item_add_mesh(get_canvas_item(), debug_mesh, get_canvas_transform(), col_debug);
// 	} else {
// 		if (debug_mesh.is_valid()) {
// 			RenderingServer::get_singleton()->free_rid(debug_mesh);
// 			debug_mesh = RID();
// 		}
// 	}
// }
// #endif // DEBUG_ENABLED

void MeshDisplay::update_modulate(const Color &p_modulate) {
	IDragonBonesDisplay::update_modulate(p_modulate);
	verticesColor.fill(p_modulate);
}

void MeshDisplay::request_redraw() {
	p_owner->request_redraw();
}

Ref<Texture2D> MeshDisplay::get_draw_texture() const {
	if (auto armature = static_cast<DragonBonesArmature *>(p_owner)) {
		return armature->get_texture_override();
	}
	return slot->get_texture();
}

void MeshDisplay::append_draw_info(LocalVector<Vector2> &r_vertices, LocalVector<int32_t> &r_indices, LocalVector<Color> &r_colors, LocalVector<Vector2> &r_uvs) const {
	r_vertices.reserve(r_vertices.size() + verticesPos.size());
	r_indices.reserve(r_indices.size() + indices.size());
	r_colors.reserve(r_colors.size() + verticesColor.size());
	r_uvs.reserve(r_uvs.size() + verticesUV.size());

	auto vertices_ptr = r_vertices.ptr() + r_vertices.size();
	auto indices_ptr = r_indices.ptr() + r_indices.size();
	auto colors_ptr = r_colors.ptr() + r_colors.size();

	memcpy(vertices_ptr, verticesPos.ptr(), sizeof(Vector2) * verticesPos.size());
	memcpy(indices_ptr, indices.ptr(), sizeof(int32_t) * indices.size());
	memcpy(colors_ptr, verticesColor.ptr(), sizeof(Color) * verticesColor.size());
}
} //namespace godot