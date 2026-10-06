/**************************************************************************/
/*  armature_view.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                           Godot-DragonBones                            */
/*        https://github.com/Daylily-Zeleen/Godot-DragonBones             */
/**************************************************************************/
/* Copyright (c) 2024-present 忘忧の (Daylily-Zeleen)                      */
/*               - Contact: daylily-zeleen@foxmail.com                    */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "armature_view.h"

#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/main_loop.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/variant/array.hpp>

#include "armature.h"
#include "dragon_bones.h"

#include "event_object.h"
using namespace godot;

// ----------------
static HashMap<CanvasItemMaterial::BlendMode, Ref<CanvasItemMaterial>> blend_materials{};
static RID get_blend_material(CanvasItemMaterial::BlendMode p_blend_mode) {
	auto it = blend_materials.find(p_blend_mode);
	if (it == blend_materials.end()) {
		Ref<CanvasItemMaterial> mat;
		mat.instantiate();
		mat->set_blend_mode(p_blend_mode);
		return blend_materials.insert(p_blend_mode, mat)->value->get_rid();
	}
	return it->value->get_rid();
}
static void clear_static() {
	blend_materials.clear();
}
// ----------------

// ---------------------------------------------------------------------------
// 逐帧复用的绘制 scratch：只被 _draw() 使用，且 _draw() 由 CanvasItem 驱动、
// 同线程串行，故全进程一份足够。放在 cpp 内 static thread_local 而非每实例成员：
// 多 view 时不会各自持有一份同规模缓冲（与 debug_draw 的 bone_scratch 同一套做法）。
// 用 thread_local 而非裸 static：防止某平台在不同线程触发 _draw 时串数据。
// ---------------------------------------------------------------------------
struct DrawScratch {
	// 一个待提交的表面：合并后的顶点/索引/颜色/UV 缓冲 + 材质信息。
	// 逐帧复用：数组缓冲跨帧保留，故用量用 n_* 计数器表示 —— 不能用 size()，
	// 因为 Packed*Array 缩到 0 会释放缓冲。数组只增不减（按需 resize 变大，
	// 帧末截到 n_* 仍在容量内，不退到 0）。
	class SurfaceData {
		PackedInt32Array indices;
		PackedVector2Array vertices;
		PackedColorArray colors;
		PackedVector2Array vertices_uv;

		int64_t n_indices = 0;
		int64_t n_vertices = 0;
		int64_t n_colors = 0;
		int64_t n_uv = 0;

	public:
		RID texture;
		CanvasItemMaterial::BlendMode blend_mode;

		SurfaceData() :
				blend_mode(CanvasItemMaterial::BLEND_MODE_MIX) {}
		SurfaceData(RID p_texture, CanvasItemMaterial::BlendMode p_blend_mode) :
				texture(p_texture), blend_mode(p_blend_mode) {}

		_FORCE_INLINE_ const PackedInt32Array &get_indices() const {
			if (n_indices)
				return indices;

			const static PackedInt32Array zero_arr;
			return zero_arr;
		}

		_FORCE_INLINE_ const PackedVector2Array &get_vertices() const {
			if (n_vertices)
				return vertices;

			const static PackedVector2Array zero_arr;
			return zero_arr;
		}

		_FORCE_INLINE_ const PackedColorArray &get_colors() const {
			if (n_colors)
				return colors;

			const static PackedColorArray zero_arr;
			return zero_arr;
		}

		_FORCE_INLINE_ const PackedVector2Array &get_vertices_uv() const {
			if (n_uv)
				return vertices_uv;

			const static PackedVector2Array zero_arr;
			return zero_arr;
		}

	private:
		friend class DrawScratch;

		_FORCE_INLINE_ void begin_frame() {
			n_indices = 0;
			n_vertices = 0;
			n_colors = 0;
			n_uv = 0;
		}

		_FORCE_INLINE_ void append_vertices(const Transform2D &p_xform, const PackedVector2Array &p_src) {
			const int64_t cnt = p_src.size();
			if (cnt == 0) {
				return;
			}
			const int64_t need = n_vertices + cnt;
			if (vertices.size() < need) {
				vertices.resize(need);
			}
			Vector2 *dst = vertices.ptrw() + n_vertices;
			const Vector2 *src = p_src.ptr();
			for (int64_t i = 0; i < cnt; ++i) {
				dst[i] = p_xform.xform(src[i]);
			}
			n_vertices = need;
		}

		_FORCE_INLINE_ void append_colors(const PackedColorArray &p_src) {
			const int64_t cnt = p_src.size();
			if (cnt == 0) {
				return;
			}
			const int64_t need = n_colors + cnt;
			if (colors.size() < need) {
				colors.resize(need);
			}
			const Color *dst = colors.ptrw() + n_colors;
			memcpy((uint8_t *)dst, (uint8_t *)p_src.ptr(), sizeof(Color) * cnt);
			n_colors = need;
		}

		_FORCE_INLINE_ void append_uv(const PackedVector2Array &p_src) {
			const int64_t cnt = p_src.size();
			if (cnt == 0) {
				return;
			}
			const int64_t need = n_uv + cnt;
			if (vertices_uv.size() < need) {
				vertices_uv.resize(need);
			}
			const Vector2 *dst = vertices_uv.ptrw() + n_uv;
			memcpy((uint8_t *)dst, (uint8_t *)p_src.ptr(), sizeof(Vector2) * cnt);
			n_uv = need;
		}

		_FORCE_INLINE_ void append_indices(const PackedInt32Array &p_src, int64_t p_base_vertex) {
			const int64_t cnt = p_src.size();
			if (cnt == 0) {
				return;
			}
			const int64_t need = n_indices + cnt;
			if (indices.size() < need) {
				indices.resize(need);
			}
			int32_t *dst = indices.ptrw() + n_indices;
			const int32_t *src = p_src.ptr();
			for (int64_t i = 0; i < cnt; ++i) {
				dst[i] = src[i] + (int32_t)p_base_vertex;
			}
			n_indices = need;
		}

		_FORCE_INLINE_ void end_frame() {
			if (n_indices > 0)
				indices.resize(n_indices);
			if (n_vertices)
				vertices.resize(n_vertices);
			if (n_colors)
				colors.resize(n_colors);
			if (n_uv)
				vertices_uv.resize(n_uv);
		}

		size_t get_capacity_bytes() const {
			return indices.size() * sizeof(int32_t) +
					vertices.size() * sizeof(Vector2) +
					colors.size() * sizeof(Color) +
					vertices_uv.size() * sizeof(Vector2);
		}
	};

	using SurfaceIndices = LocalVector<uint32_t>;
	// 每帧复用的绘制数据：begin_frame() 复位游标、不释放缓冲。
	ArmatureDrawData draw_data;

private:
	// 表面缓冲池：只增不减，跨帧保留各数组已分配的缓冲。
	// 用下标引用而不是 SurfaceData*，避免池扩容时指针悬空。
	LocalVector<SurfaceData> surfaces;

	LocalVector<SurfaceIndices> mesh_surfaces;
	// 本帧使用的分组数（mesh_surfaces 的容量跨帧保留，故不能只看 size）。
	uint32_t mesh_count = 0;

public:
	_FORCE_INLINE_ uint32_t get_mesh_count() const { return mesh_count; }
	_FORCE_INLINE_ const SurfaceIndices &get_mesh_surface_indices(const uint32_t &p_mesh_idx) const {
		DEV_ASSERT(p_mesh_idx < get_mesh_count());
		return mesh_surfaces[p_mesh_idx];
	}
	_FORCE_INLINE_ const SurfaceData &get_surface(const uint32_t &p_surface_idx) const {
		DEV_ASSERT(p_surface_idx < surfaces.size());
		return surfaces[p_surface_idx];
	}

	void try_reset() {
		// 全部缓冲的容量字节数（含 draw_data 内部与各表面数组）。
		constexpr size_t RELEASE_THRESHOLD_BYTES = 8u << 17; // 1 MiB

		const size_t cur_capacity_bytes = [this]() -> size_t {
			size_t n = draw_data.get_capacity_bytes();
			for (const SurfaceData &s : surfaces) {
				n += s.get_capacity_bytes();
			}
			return n;
		}();

		if (cur_capacity_bytes <= RELEASE_THRESHOLD_BYTES) {
			return;
		}
		surfaces.reset();
		mesh_surfaces.reset();
		mesh_count = 0;
		draw_data.reset();
	}

	void build_surfaces() {
		mesh_count = 0;
		if (draw_data.is_empty()) {
			return;
		}

		// 取/建第 p_mesh 组里的第 p_surface 个表面，复位其计数。
		auto acquire_surface = [this](uint32_t p_mesh, uint32_t p_surface, RID p_texture,
									  CanvasItemMaterial::BlendMode p_blend) -> uint32_t {
			uint32_t pool_index = p_surface;
			for (uint32_t m = 0; m < p_mesh; ++m) {
				pool_index += mesh_surfaces[m].size();
			}
			if (surfaces.size() <= pool_index) {
				surfaces.push_back(DrawScratch::SurfaceData(p_texture, p_blend));
			} else {
				surfaces[pool_index].texture = p_texture;
				surfaces[pool_index].blend_mode = p_blend;
			}
			surfaces[pool_index].begin_frame();
			return pool_index;
		};

		RID cur_texture{};
		CanvasItemMaterial::BlendMode cur_blend = CanvasItemMaterial::BLEND_MODE_MIX;
		uint32_t cur_surface_idx = 0;
		bool started = false;

		for (const ArmatureDrawData::Layer &layer : draw_data) {
			for (const ArmatureDrawData::Data &data : layer.data) {
				if (data.indices.is_empty()) {
					continue;
				}

				if (data.texture != cur_texture || !started) {
					// 首个表面，或换 mesh。
					++mesh_count;
					const uint32_t cur_mesh_idx = mesh_count - 1;
					cur_surface_idx = acquire_surface(cur_mesh_idx, 0, data.texture, data.blend_mode);
					if (mesh_surfaces.size() < mesh_count) {
						mesh_surfaces.push_back(SurfaceIndices());
					}
					mesh_surfaces[cur_mesh_idx].clear();
					mesh_surfaces[cur_mesh_idx].push_back(cur_surface_idx);
					cur_texture = data.texture;
					cur_blend = data.blend_mode;
					started = true;
				} else if (data.blend_mode != cur_blend) {
					// 同 mesh 内换 surface。
					const uint32_t s = mesh_surfaces[mesh_count - 1].size();
					cur_surface_idx = acquire_surface(mesh_count - 1, s, data.texture, data.blend_mode);
					mesh_surfaces[mesh_count - 1].push_back(cur_surface_idx);
					cur_blend = data.blend_mode;
				}

				DrawScratch::SurfaceData &sd = surfaces[cur_surface_idx];
				const int64_t base_vertex = sd.n_vertices;
				sd.append_indices(data.indices, base_vertex);
				sd.append_vertices(data.transform, data.vertices);
				sd.append_colors(data.colors);
				sd.append_uv(data.vertices_uv);
			}
		}

		// 帧末：各用到的表面把数组截到逻辑用量（保留缓冲）。
		for (uint32_t i = 0; i < mesh_count; ++i) {
			for (uint32_t idx : mesh_surfaces[i]) {
				surfaces[idx].end_frame();
			}
		}
	}
};

static thread_local DrawScratch draw_scratch;

void DragonBonesArmatureView::rebuild_armature() {
	if (armature) {
		armature->release(); // 已经处理内存的释放
		armature = nullptr;
	}

	if (factory.is_valid()) {
		armature = factory->create_armature(this, instantiate_dragon_bones_data_name, instantiate_armature_name, instantiate_skin_name);
		if (is_armature_valid()) {
			armature->force_update();
		}
	}

	if (is_inside_tree()) {
		queue_redraw();
	}
}

void DragonBonesArmatureView::set_factory(const Ref<DragonBonesFactory> &p_factory) {
	using namespace dragonBones;
	if (factory == p_factory) {
		return;
	}

	factory = p_factory;

	rebuild_armature();
	notify_property_list_changed();
}

Ref<DragonBonesFactory> DragonBonesArmatureView::get_factory() const {
	return factory;
}

void DragonBonesArmatureView::set_active(bool p_active) {
	active = p_active;
	set_physics_process_internal(callback_mode_process == ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS && active);
	set_process_internal(callback_mode_process == ANIMATION_CALLBACK_MODE_PROCESS_IDLE && active);
}

bool DragonBonesArmatureView::is_active() const {
	return active;
}

#ifdef DEBUG_ENABLED
void DragonBonesArmatureView::set_debug_draw_enabled(bool p_enabled) {
	debug_draw.set_enabled(p_enabled);
	queue_redraw();
}

bool DragonBonesArmatureView::is_debug_draw_enabled() const {
	return debug_draw.is_enabled();
}

void DragonBonesArmatureView::set_debug_draw_bone_pivot_radius(float p_radius) {
	DebugDraw::set_bone_pivot_radius(p_radius);
	queue_redraw();
}

float DragonBonesArmatureView::get_debug_draw_bone_pivot_radius() const {
	return DebugDraw::get_bone_pivot_radius();
}

void DragonBonesArmatureView::set_debug_draw_visible_mesh(bool p_visible) {
	debug_draw.set_flag(DebugDraw::DRAW_MESH, p_visible);
	queue_redraw();
}

bool DragonBonesArmatureView::is_debug_draw_visible_mesh() const {
	return debug_draw.has_flag(DebugDraw::DRAW_MESH);
}

void DragonBonesArmatureView::set_debug_draw_visible_bone(bool p_visible) {
	debug_draw.set_flag(DebugDraw::DRAW_BONE, p_visible);
	queue_redraw();
}

bool DragonBonesArmatureView::is_debug_draw_visible_bone() const {
	return debug_draw.has_flag(DebugDraw::DRAW_BONE);
}

void DragonBonesArmatureView::set_debug_draw_visible_bone_name(bool p_visible) {
	debug_draw.set_flag(DebugDraw::DRAW_BONE_NAME, p_visible);
	queue_redraw();
}

bool DragonBonesArmatureView::is_debug_draw_visible_bone_name() const {
	return debug_draw.has_flag(DebugDraw::DRAW_BONE_NAME);
}

void DragonBonesArmatureView::set_debug_draw_color_bone(const Color &p_color) {
	DebugDraw::color_bone = p_color;
	queue_redraw();
}

Color DragonBonesArmatureView::get_debug_draw_color_bone() const {
	return DebugDraw::color_bone;
}

void DragonBonesArmatureView::set_debug_draw_color_ik_target(const Color &p_color) {
	DebugDraw::color_ik_target = p_color;
	queue_redraw();
}

Color DragonBonesArmatureView::get_debug_draw_color_ik_target() const {
	return DebugDraw::color_ik_target;
}

void DragonBonesArmatureView::set_debug_draw_color_ik_bone_outline(const Color &p_color) {
	DebugDraw::set_color_ik_bone_outline(p_color);
	queue_redraw();
}

Color DragonBonesArmatureView::get_debug_draw_color_ik_bone_outline() const {
	return DebugDraw::get_color_ik_bone_outline();
}
#endif // DEBUG_ENABLED

void DragonBonesArmatureView::set_time_scale(float p_time_scale) {
	time_scale = p_time_scale < 0.0 ? 0.0 : p_time_scale;
}

float DragonBonesArmatureView::get_time_scale() const {
	return time_scale;
}

void DragonBonesArmatureView::set_instantiate_dragon_bones_data_name(String p_name) {
	if (p_name == "") {
		p_name = "";
	}
	if (p_name == instantiate_dragon_bones_data_name) {
		return;
	}

	instantiate_dragon_bones_data_name = p_name;
	rebuild_armature();
#ifdef TOOLS_ENABLED
	notify_property_list_changed(); // 触发 _validate_property
#endif //TOOLS_ENABLED
}

String DragonBonesArmatureView::get_instantiate_dragon_bones_data_name() const {
	return instantiate_dragon_bones_data_name;
}

void DragonBonesArmatureView::set_instantiate_armature_name(String p_name) {
	if (p_name == "") {
		p_name = "";
	}
	if (p_name == instantiate_armature_name) {
		return;
	}

	instantiate_armature_name = p_name;
	rebuild_armature();
#ifdef TOOLS_ENABLED
	notify_property_list_changed(); // 触发 _validate_property
#endif //TOOLS_ENABLED
}

String DragonBonesArmatureView::get_instantiate_armature_name() const {
	return instantiate_armature_name;
}

void DragonBonesArmatureView::set_instantiate_skin_name(String p_name) {
	if (p_name == "") {
		p_name = "";
	}
	if (p_name == instantiate_skin_name) {
		return;
	}

	instantiate_skin_name = p_name;
	rebuild_armature();
}

String DragonBonesArmatureView::get_instantiate_skin_name() const {
	return instantiate_skin_name;
}

void DragonBonesArmatureView::set_callback_mode_process(AnimationCallbackModeProcess p_mode) {
	callback_mode_process = p_mode;
	set_active(active);
}

DragonBonesArmatureView::AnimationCallbackModeProcess DragonBonesArmatureView::get_callback_mode_process() const {
	return callback_mode_process;
}

int DragonBonesArmatureView::get_animation_loop_count() const {
	return animation_loop_count;
}

void DragonBonesArmatureView::set_animation_loop_count(int p_animation_loop) {
	animation_loop_count = p_animation_loop;
	if (is_armature_valid()) {
		reset();
		armature->play(armature->get_current_animation(), animation_loop_count);
	}
}

void DragonBonesArmatureView::reset() {
	ERR_FAIL_COND(!is_armature_valid());
	armature->reset(true);
}

DragonBonesArmature *DragonBonesArmatureView::get_armature() {
	ERR_FAIL_COND_V(!is_armature_valid(), nullptr);
	return armature;
}

void DragonBonesArmatureView::set_armature_settings(const Dictionary &p_settings) const {
	if (is_armature_valid()) {
		armature->set_settings(p_settings);
	} else {
#ifdef TOOLS_ENABLED
		if (!factory->is_imported()) {
			// 只对非导入工厂打印错误信息，导入工厂将在后续重新导入
			WARN_PRINT_ED("armature is invalid, can't set armature settings.");
		}
#else // !TOOLS_ENABLED
		WARN_PRINT_ED("armature is invalid, can't set armature settings.");
#endif // !TOOLS_ENABLED
	}
}

Dictionary DragonBonesArmatureView::get_armature_settings() const {
	if (!is_armature_valid()) {
		return {};
	}
#ifdef TOOLS_ENABLED
	return armature->get_settings();
#else //TOOLS_ENABLED
	ERR_FAIL_V_MSG({}, "DragonBonesArmatureView::get_armature_settings() can be call in editor build only.");
#endif // TOOLS_ENABLED
}

bool DragonBonesArmatureView::_set(const StringName &p_name, const Variant &p_property) {
	if (p_name == SNAME("armature_settings")) {
		set_armature_settings(p_property);
		return true;
	}
#ifdef TOOLS_ENABLED
	else if (p_name == SNAME("armature")) {
		return true; // 禁止设置
	}
#endif //  TOOLS_ENABLED

	return false;
}

bool DragonBonesArmatureView::_get(const StringName &p_name, Variant &r_property) const {
	if (p_name == SNAME("armature_settings")) {
		r_property = get_armature_settings();
		return true;
	}
#ifdef TOOLS_ENABLED
	else if (p_name == SNAME("armature")) {
		// Avoid instantiation when getting default value.
		if (is_armature_valid() && armature_ref.is_null()) {
			armature_ref.instantiate();
		}

		if (armature_ref.is_valid()) {
			armature_ref->armature = armature;
		}

		r_property = armature_ref;
		return true;
	}
#endif // TOOLS_ENABLED
	return false;
}

void DragonBonesArmatureView::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::DICTIONARY, "armature_settings", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_STORAGE));

#ifdef TOOLS_ENABLED
	if (is_armature_valid() && Engine::get_singleton()->is_editor_hint()) {
		p_list->push_back(PropertyInfo(
				Variant::OBJECT, "armature", PROPERTY_HINT_RESOURCE_TYPE, DragonBonesArmatureProxy::get_class_static(), PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT, DragonBonesArmatureProxy::get_class_static()));
	}
#endif // TOOLS_ENABLED
}

#ifdef TOOLS_ENABLED
void DragonBonesArmatureView::_validate_property(PropertyInfo &p_property) const {
	if (!Engine::get_singleton()->is_editor_hint() || factory.is_null()) {
		return;
	}
	if (p_property.name == SNAME("instantiate_dragon_bones_data_name")) {
		auto dragon_bones_data_list = factory->get_loaded_dragon_bones_data_name_list();
		p_property.hint_string = String(",").join(dragon_bones_data_list);
	} else if (p_property.name == SNAME("instantiate_armature_name")) {
		auto armatures = factory->get_loaded_dragon_bones_armature_name_list(instantiate_dragon_bones_data_name);
		p_property.hint_string = String(",").join(armatures);
	} else if (p_property.name == SNAME("instantiate_skin_name")) {
		auto skins = factory->get_loaded_dragon_bones_skin_name_list(instantiate_dragon_bones_data_name, instantiate_armature_name);
		p_property.hint_string = String(",").join(skins);
	}
}
#endif // TOOLS_ENABLED

void DragonBonesArmatureView::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			set_active(active);
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (active && callback_mode_process == ANIMATION_CALLBACK_MODE_PROCESS_IDLE) {
				advance(get_process_delta_time() * time_scale);
			}
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			if (active && callback_mode_process == ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS) {
				advance(get_physics_process_delta_time() * time_scale);
			}
		} break;
	}
}

void DragonBonesArmatureView::_draw() {
	if (!is_armature_valid()) {
		return;
	}

	// Collect draw data. 复用成员：begin_frame() 只复位游标，不释放缓冲。
	draw_scratch.draw_data.begin_frame();
	armature->append_draw_data(draw_scratch.draw_data);
	draw_scratch.draw_data.end_frame();
	const ArmatureDrawData &draw_data = draw_scratch.draw_data;

	if (draw_data.is_empty()) {
		return;
	}

	const auto RS = RenderingServer::get_singleton();

	draw_scratch.build_surfaces();

	// Clear surfaces.
	for (RID mesh : draw_meshes) {
		RS->mesh_clear(mesh);
	}

	// Add rendering commands.
	constexpr Transform2D identity{};
	for (uint32_t mesh_i = 0; mesh_i < draw_scratch.get_mesh_count(); ++mesh_i) {
		const RID mesh = get_draw_mesh(mesh_i);
		const DrawScratch::SurfaceIndices &surface_indices = draw_scratch.get_mesh_surface_indices(mesh_i);

		for (uint32_t surface_i = 0; surface_i < surface_indices.size(); ++surface_i) {
			const DrawScratch::SurfaceData &surface_data = draw_scratch.get_surface(surface_indices[surface_i]);

			Array arr;
			arr.resize(RenderingServer::ARRAY_MAX);
			arr[RenderingServer::ARRAY_INDEX] = surface_data.get_indices();
			arr[RenderingServer::ARRAY_VERTEX] = surface_data.get_vertices();
			arr[RenderingServer::ARRAY_COLOR] = surface_data.get_colors();
			arr[RenderingServer::ARRAY_TEX_UV] = surface_data.get_vertices_uv();

			RS->mesh_add_surface_from_arrays(mesh, RenderingServer::PRIMITIVE_TRIANGLES, arr);
			auto mat = get_blend_material(surface_data.blend_mode);
			RS->mesh_surface_set_material(mesh, RS->mesh_get_surface_count(mesh) - 1, mat);
		}

		RS->canvas_item_add_mesh(get_canvas_item(), mesh, identity, get_modulate(), draw_scratch.get_surface(surface_indices[0]).texture);
	}

#ifdef DEBUG_ENABLED
	if (debug_draw.is_enabled())
		debug_draw.draw(armature, draw_data);
#endif // DEBUG_ENABLED
}

void DragonBonesArmatureView::dispatch_event(const Ref<DragonBonesEventObject> &p_event_object) {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	emit_signal(SNAME("event_dispatched"), p_event_object);
}

RID DragonBonesArmatureView::get_draw_mesh(int p_index) {
	if (p_index < draw_meshes.size()) {
		return draw_meshes[p_index];
	} else {
		draw_meshes.push_back(RenderingServer::get_singleton()->mesh_create());
		return draw_meshes[draw_meshes.size() - 1];
	}
}

void DragonBonesArmatureView::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_factory", "factory"), &DragonBonesArmatureView::set_factory);
	ClassDB::bind_method(D_METHOD("get_factory"), &DragonBonesArmatureView::get_factory);

	ClassDB::bind_method(D_METHOD("advance", "delta"), &DragonBonesArmatureView::advance);

	ClassDB::bind_method(D_METHOD("set_animation_loop_count", "loop_count"), &DragonBonesArmatureView::set_animation_loop_count);
	ClassDB::bind_method(D_METHOD("get_animation_loop_count"), &DragonBonesArmatureView::get_animation_loop_count);

	ClassDB::bind_method(D_METHOD("set_time_scale", "speed_scale"), &DragonBonesArmatureView::set_time_scale);
	ClassDB::bind_method(D_METHOD("get_time_scale"), &DragonBonesArmatureView::get_time_scale);

	ClassDB::bind_method(D_METHOD("get_armature"), &DragonBonesArmatureView::get_armature);

	ClassDB::bind_method(D_METHOD("set_active", "active"), &DragonBonesArmatureView::set_active);
	ClassDB::bind_method(D_METHOD("is_active"), &DragonBonesArmatureView::is_active);

#ifdef DEBUG_ENABLED
	ClassDB::bind_method(D_METHOD("set_debug_draw_enabled", "enabled"), &DragonBonesArmatureView::set_debug_draw_enabled);
	ClassDB::bind_method(D_METHOD("is_debug_draw_enabled"), &DragonBonesArmatureView::is_debug_draw_enabled);
	ClassDB::bind_method(D_METHOD("set_debug_draw_bone_pivot_radius", "radius"), &DragonBonesArmatureView::set_debug_draw_bone_pivot_radius);
	ClassDB::bind_method(D_METHOD("get_debug_draw_bone_pivot_radius"), &DragonBonesArmatureView::get_debug_draw_bone_pivot_radius);

	ClassDB::bind_method(D_METHOD("set_debug_draw_visible_mesh", "visible"), &DragonBonesArmatureView::set_debug_draw_visible_mesh);
	ClassDB::bind_method(D_METHOD("is_debug_draw_visible_mesh"), &DragonBonesArmatureView::is_debug_draw_visible_mesh);
	ClassDB::bind_method(D_METHOD("set_debug_draw_visible_bone", "visible"), &DragonBonesArmatureView::set_debug_draw_visible_bone);
	ClassDB::bind_method(D_METHOD("is_debug_draw_visible_bone"), &DragonBonesArmatureView::is_debug_draw_visible_bone);
	ClassDB::bind_method(D_METHOD("set_debug_draw_visible_bone_name", "visible"), &DragonBonesArmatureView::set_debug_draw_visible_bone_name);
	ClassDB::bind_method(D_METHOD("is_debug_draw_visible_bone_name"), &DragonBonesArmatureView::is_debug_draw_visible_bone_name);

	ClassDB::bind_method(D_METHOD("set_debug_draw_color_bone", "color"), &DragonBonesArmatureView::set_debug_draw_color_bone);
	ClassDB::bind_method(D_METHOD("get_debug_draw_color_bone"), &DragonBonesArmatureView::get_debug_draw_color_bone);
	ClassDB::bind_method(D_METHOD("set_debug_draw_color_ik_target", "color"), &DragonBonesArmatureView::set_debug_draw_color_ik_target);
	ClassDB::bind_method(D_METHOD("get_debug_draw_color_ik_target"), &DragonBonesArmatureView::get_debug_draw_color_ik_target);
	ClassDB::bind_method(D_METHOD("set_debug_draw_color_ik_bone_outline", "color"), &DragonBonesArmatureView::set_debug_draw_color_ik_bone_outline);
	ClassDB::bind_method(D_METHOD("get_debug_draw_color_ik_bone_outline"), &DragonBonesArmatureView::get_debug_draw_color_ik_bone_outline);
#endif // DEBUG_ENABLED

	ClassDB::bind_method(D_METHOD("set_callback_mode_process", "mode"), &DragonBonesArmatureView::set_callback_mode_process);
	ClassDB::bind_method(D_METHOD("get_callback_mode_process"), &DragonBonesArmatureView::get_callback_mode_process);

	ClassDB::bind_method(D_METHOD("set_instantiate_dragon_bones_data_name", "instantiate_dragon_bones_data_name"), &DragonBonesArmatureView::set_instantiate_dragon_bones_data_name);
	ClassDB::bind_method(D_METHOD("get_instantiate_dragon_bones_data_name"), &DragonBonesArmatureView::get_instantiate_dragon_bones_data_name);

	ClassDB::bind_method(D_METHOD("set_instantiate_armature_name", "instantiate_armature_name"), &DragonBonesArmatureView::set_instantiate_armature_name);
	ClassDB::bind_method(D_METHOD("get_instantiate_armature_name"), &DragonBonesArmatureView::get_instantiate_armature_name);

	ClassDB::bind_method(D_METHOD("set_instantiate_skin_name", "instantiate_skin_name"), &DragonBonesArmatureView::set_instantiate_skin_name);
	ClassDB::bind_method(D_METHOD("get_instantiate_skin_name"), &DragonBonesArmatureView::get_instantiate_skin_name);

	// 包装 ===========
	ClassDB::bind_method(D_METHOD("has_animation", "animation_name"), &DragonBonesArmatureView::has_animation);
	ClassDB::bind_method(D_METHOD("get_animations"), &DragonBonesArmatureView::get_animations);
	ClassDB::bind_method(D_METHOD("is_playing"), &DragonBonesArmatureView::is_playing);

	ClassDB::bind_method(D_METHOD("tell_animation", "animation_name"), &DragonBonesArmatureView::tell_animation);
	ClassDB::bind_method(D_METHOD("seek_animation", "animation_name", "progress"), &DragonBonesArmatureView::seek_animation);

	ClassDB::bind_method(D_METHOD("play", "animation_name", "loop_count"), &DragonBonesArmatureView::play, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("play_from_time", "animation_name", "time", "loop_count"), &DragonBonesArmatureView::play_from_time, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("play_from_progress", "animation_name", "progress", "loop_count"), &DragonBonesArmatureView::play_from_progress, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("stop", "animation_name", "reset", "recursively"), &DragonBonesArmatureView::stop, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("stop_all_animations", "reset", "recursively"), &DragonBonesArmatureView::stop_all_animations, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("fade_in", "animation_name", "time", "loop", "layer", "group", "fade_out_mode"), &DragonBonesArmatureView::fade_in);

	ClassDB::bind_method(D_METHOD("has_slot", "slot_name"), &DragonBonesArmatureView::has_slot);
	ClassDB::bind_method(D_METHOD("get_slot", "slot_name"), &DragonBonesArmatureView::get_slot);
	ClassDB::bind_method(D_METHOD("get_slots"), &DragonBonesArmatureView::get_slots);

	ClassDB::bind_method(D_METHOD("get_ik_constraints"), &DragonBonesArmatureView::get_ik_constraints);
	ClassDB::bind_method(D_METHOD("set_ik_constraint", "constraint_name", "new_position"), &DragonBonesArmatureView::set_ik_constraint);
	ClassDB::bind_method(D_METHOD("set_ik_constraint_bend_positive", "constraint_name", "bend_positive"), &DragonBonesArmatureView::set_ik_constraint_bend_positive);

	ClassDB::bind_method(D_METHOD("get_bones"), &DragonBonesArmatureView::get_bones);
	ClassDB::bind_method(D_METHOD("get_bone", "bone_name"), &DragonBonesArmatureView::get_bone);

	ClassDB::bind_method(D_METHOD("get_rect"), &DragonBonesArmatureView::get_rect);
	ClassDB::bind_method(D_METHOD("get_global_rect"), &DragonBonesArmatureView::get_global_rect);

	// Setter Getter
	ClassDB::bind_method(D_METHOD("set_current_animation", "current_animation"), &DragonBonesArmatureView::set_current_animation);
	ClassDB::bind_method(D_METHOD("get_current_animation"), &DragonBonesArmatureView::get_current_animation);

	ClassDB::bind_method(D_METHOD("set_animation_progress", "progress"), &DragonBonesArmatureView::set_animation_progress);
	ClassDB::bind_method(D_METHOD("get_animation_progress"), &DragonBonesArmatureView::get_animation_progress);

	ClassDB::bind_method(D_METHOD("set_flip_x_", "flip_x"), &DragonBonesArmatureView::set_flip_x_);
	ClassDB::bind_method(D_METHOD("is_flipped_x"), &DragonBonesArmatureView::is_flipped_x);

	ClassDB::bind_method(D_METHOD("set_flip_y_", "flip_y"), &DragonBonesArmatureView::set_flip_y_);
	ClassDB::bind_method(D_METHOD("is_flipped_y"), &DragonBonesArmatureView::is_flipped_y);

	ClassDB::bind_method(D_METHOD("set_texture_override", "texture_override"), &DragonBonesArmatureView::set_texture_override);
	ClassDB::bind_method(D_METHOD("get_texture_override"), &DragonBonesArmatureView::get_texture_override);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "current_animation", PROPERTY_HINT_ENUM, "", PROPERTY_USAGE_NONE), "set_current_animation", "get_current_animation");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "animation_progress", PROPERTY_HINT_RANGE, "0.0,1.0,0.0001", PROPERTY_USAGE_NONE), "set_animation_progress", "get_animation_progress");

	ADD_GROUP("Flip", "flip_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_x", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_flip_x_", "is_flipped_x");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "flip_y", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_flip_y_", "is_flipped_y");

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "texture_override", PROPERTY_HINT_RESOURCE_TYPE, Texture2D::get_class_static(), PROPERTY_USAGE_NONE), "set_texture_override", "get_texture_override");
	// ================================

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "factory", PROPERTY_HINT_RESOURCE_TYPE, DragonBonesFactory::get_class_static()), "set_factory", "get_factory");

	// This is how we set top level properties
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "active"), "set_active", "is_active");

#ifdef DEBUG_ENABLED
	// The prefix is spelled out in each property name; ADD_GROUP only records the
	// section header, matching how the `Flip` group above is written.
	ADD_GROUP("DebugDraw", "debug_draw_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_draw_enabled"), "set_debug_draw_enabled", "is_debug_draw_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "debug_draw_bone_pivot_radius", PROPERTY_HINT_RANGE, "0.5,64.0,0.5,or_greater"), "set_debug_draw_bone_pivot_radius", "get_debug_draw_bone_pivot_radius");

	ADD_SUBGROUP("Visible", "debug_draw_visible_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_draw_visible_mesh"), "set_debug_draw_visible_mesh", "is_debug_draw_visible_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_draw_visible_bone"), "set_debug_draw_visible_bone", "is_debug_draw_visible_bone");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "debug_draw_visible_bone_name"), "set_debug_draw_visible_bone_name", "is_debug_draw_visible_bone_name");

	ADD_SUBGROUP("Color", "debug_draw_color_");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "debug_draw_color_bone"), "set_debug_draw_color_bone", "get_debug_draw_color_bone");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "debug_draw_color_ik_target"), "set_debug_draw_color_ik_target", "get_debug_draw_color_ik_target");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "debug_draw_color_ik_bone_outline"), "set_debug_draw_color_ik_bone_outline", "get_debug_draw_color_ik_bone_outline");
#endif // DEBUG_ENABLED

	ADD_GROUP("Animation Settings", "animation_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "animation_loop_count", PROPERTY_HINT_RANGE, "0,100,1,or_greater"), "set_animation_loop_count", "get_animation_loop_count");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "animation_time_scale", PROPERTY_HINT_RANGE, "0,10,0.01"), "set_time_scale", "get_time_scale");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "animation_callback_mode_process", PROPERTY_HINT_ENUM, "Physics,Idle,Manual"), "set_callback_mode_process", "get_callback_mode_process");

	ADD_GROUP("Instantiate Settings", "instantiate_");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "instantiate_dragon_bones_data_name", PROPERTY_HINT_ENUM_SUGGESTION, ""), "set_instantiate_dragon_bones_data_name", "get_instantiate_dragon_bones_data_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "instantiate_armature_name", PROPERTY_HINT_ENUM_SUGGESTION, ""), "set_instantiate_armature_name", "get_instantiate_armature_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "instantiate_skin_name", PROPERTY_HINT_ENUM_SUGGESTION, ""), "set_instantiate_skin_name", "get_instantiate_skin_name");

	// 信号
	ADD_SIGNAL(MethodInfo("event_dispatched", PropertyInfo(Variant::OBJECT, "event_object", PROPERTY_HINT_NONE, "", PROPERTY_HINT_NONE, DragonBonesEventObject::get_class_static())));

	// 枚举
	BIND_ENUM_CONSTANT(ANIMATION_CALLBACK_MODE_PROCESS_PHYSICS);
	BIND_ENUM_CONSTANT(ANIMATION_CALLBACK_MODE_PROCESS_IDLE);
	BIND_ENUM_CONSTANT(ANIMATION_CALLBACK_MODE_PROCESS_MANUAL);

	// Enum
	// BIND_ENUM_CONSTANT(AnimFadeOutMode::FADE_OUT_NONE);
	// BIND_ENUM_CONSTANT(AnimFadeOutMode::FADE_OUT_SAME_LAYER);
	// BIND_ENUM_CONSTANT(AnimFadeOutMode::FADE_OUT_SAME_GROUP);
	// BIND_ENUM_CONSTANT(AnimFadeOutMode::FADE_OUT_SAME_LAYER_AND_GROUP);
	// BIND_ENUM_CONSTANT(AnimFadeOutMode::FADE_OUT_ALL);
	// BIND_ENUM_CONSTANT(AnimFadeOutMode::FADE_OUT_SINGLE);

	//
	DragonBones::add_clean_static_callback(&clear_static);
}

DragonBonesArmatureView::DragonBonesArmatureView() {
}

DragonBonesArmatureView::~DragonBonesArmatureView() {
	draw_scratch.try_reset();

	if (armature) {
		armature->release(); // 已经处理内存的释放
		armature = nullptr;
	}

	DragonBones::get_singleton()->flush();

	for (auto mesh : draw_meshes) {
		RenderingServer::get_singleton()->free_rid(mesh);
	}
	draw_meshes.clear();
}

// ---------

bool DragonBonesArmatureView::has_animation(const String &p_animation_name) const {
	ERR_FAIL_NULL_V(armature, false);
	return armature->has_animation(p_animation_name);
}
PackedStringArray DragonBonesArmatureView::get_animations() {
	ERR_FAIL_NULL_V(armature, PackedStringArray());
	return armature->get_animations();
}
String DragonBonesArmatureView::get_current_animation_on_layer(int p_layer) const {
	ERR_FAIL_NULL_V(armature, String());
	return armature->get_current_animation_on_layer(p_layer);
}
String DragonBonesArmatureView::get_current_animation_in_group(const String &p_group_name) const {
	ERR_FAIL_NULL_V(armature, String());
	return armature->get_current_animation_in_group(p_group_name);
}
float DragonBonesArmatureView::tell_animation(const String &p_animation_name) const {
	ERR_FAIL_NULL_V(armature, 0.0f);
	return armature->tell_animation(p_animation_name);
}
void DragonBonesArmatureView::seek_animation(const String &p_animation_name, float p_progress) {
	ERR_FAIL_NULL(armature);
	armature->seek_animation(p_animation_name, p_progress);
}
bool DragonBonesArmatureView::is_playing() const {
	ERR_FAIL_NULL_V(armature, false);
	return armature->is_playing();
}
void DragonBonesArmatureView::play(const String &p_animation_name, int p_loop_count) {
	ERR_FAIL_NULL(armature);
	armature->play(p_animation_name, p_loop_count);
}
void DragonBonesArmatureView::play_from_time(const String &p_animation_name, float p_time, int p_loop_count) {
	ERR_FAIL_NULL(armature);
	armature->play_from_time(p_animation_name, p_time, p_loop_count);
}
void DragonBonesArmatureView::play_from_progress(const String &p_animation_name, float p_progress, int p_loop_count) {
	ERR_FAIL_NULL(armature);
	armature->play_from_progress(p_animation_name, p_progress, p_loop_count);
}
void DragonBonesArmatureView::stop(const String &p_animation_name, bool b_reset, bool p_recursively) {
	ERR_FAIL_NULL(armature);
	armature->stop(p_animation_name, b_reset, p_recursively);
}
void DragonBonesArmatureView::stop_all_animations(bool b_reset, bool p_recursively) {
	ERR_FAIL_NULL(armature);
	armature->stop_all_animations(b_reset, p_recursively);
}
void DragonBonesArmatureView::fade_in(const String &p_animation_name, float p_time,
									  int p_loop_count, int p_layer, const String &p_group, AnimFadeOutMode p_fade_out_mode) {
	ERR_FAIL_NULL(armature);
	armature->fade_in(p_animation_name, p_time, p_loop_count, p_layer, p_group, p_fade_out_mode);
}

bool DragonBonesArmatureView::has_slot(const StringName &p_slot_name) const {
	ERR_FAIL_NULL_V(armature, false);
	return armature->has_slot(p_slot_name);
}
Ref<DragonBonesSlot> DragonBonesArmatureView::get_slot(const StringName &p_slot_name) {
	ERR_FAIL_NULL_V(armature, {});
	return armature->get_slot(p_slot_name);
}
SlotsDictionary DragonBonesArmatureView::get_slots() {
	ERR_FAIL_NULL_V(armature, {});
	return armature->get_slots_();
}

ConstraintsDictionary DragonBonesArmatureView::get_ik_constraints() {
	ERR_FAIL_NULL_V(armature, ConstraintsDictionary());
	return armature->get_ik_constraints();
}
void DragonBonesArmatureView::set_ik_constraint(const String &p_name, Vector2 p_position) {
	ERR_FAIL_NULL(armature);
	armature->set_ik_constraint(p_name, p_position);
}
void DragonBonesArmatureView::set_ik_constraint_bend_positive(const String &p_name, bool p_bend_positive) {
	ERR_FAIL_NULL(armature);
	armature->set_ik_constraint_bend_positive(p_name, p_bend_positive);
}

BonesDictionary DragonBonesArmatureView::get_bones() {
	ERR_FAIL_NULL_V(armature, {});
	return armature->get_bones_();
}
Ref<DragonBonesBone> DragonBonesArmatureView::get_bone(const StringName &p_name) {
	ERR_FAIL_NULL_V(armature, {});
	return armature->get_bone(p_name);
}

Rect2 DragonBonesArmatureView::get_rect() const {
	ERR_FAIL_NULL_V(armature, {});
	Rect2 rect = armature->get_rect();
	return get_transform().inverse().xform(rect);
}

Rect2 DragonBonesArmatureView::get_global_rect() const {
	ERR_FAIL_NULL_V(armature, {});
	Rect2 rect = armature->get_rect();
	return get_global_transform().inverse().xform(rect);
}

// setget
void DragonBonesArmatureView::set_current_animation(const String &p_animation) {
	ERR_FAIL_NULL(armature);
	armature->set_current_animation(p_animation);
}
String DragonBonesArmatureView::get_current_animation() const {
	ERR_FAIL_NULL_V(armature, "");
	return armature->get_current_animation();
}

void DragonBonesArmatureView::set_animation_progress(float p_progress) {
	ERR_FAIL_NULL(armature);
	armature->set_animation_progress(p_progress);
}
float DragonBonesArmatureView::get_animation_progress() const {
	ERR_FAIL_NULL_V(armature, 0.0f);
	return armature->get_animation_progress();
}

void DragonBonesArmatureView::set_flip_x(bool p_flip_x, bool p_recursively) {
	ERR_FAIL_NULL(armature);
	armature->set_flip_x(p_flip_x, p_recursively);
}
bool DragonBonesArmatureView::is_flipped_x() const {
	ERR_FAIL_NULL_V(armature, false);
	return armature->is_flipped_x();
}

void DragonBonesArmatureView::set_flip_y(bool p_flip_y, bool p_recursively) {
	ERR_FAIL_NULL(armature);
	armature->set_flip_y(p_flip_y, p_recursively);
}
bool DragonBonesArmatureView::is_flipped_y() const {
	ERR_FAIL_NULL_V(armature, false);
	return armature->is_flipped_y();
}

Ref<Texture2D> DragonBonesArmatureView::get_texture_override() const {
	ERR_FAIL_NULL_V(armature, Ref<Texture2D>());
	return armature->get_texture_override();
}
void DragonBonesArmatureView::set_texture_override(const Ref<Texture2D> &p_texture_override) {
	ERR_FAIL_NULL(armature);
	armature->set_texture_override(p_texture_override);
}