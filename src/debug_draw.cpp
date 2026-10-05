/**************************************************************************/
/*  debug_draw.cpp                                                        */
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

#include "debug_draw.h"

#include <dragonBones/armature/Constraint.h>
#include <dragonBones/armature/Slot.h>

#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/theme_db.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/templates/local_vector.hpp>

#include <type_traits>

namespace godot {

#ifdef DEBUG_ENABLED

namespace {

// ---------------------------------------------------------------------------
// 枢轴符号
//
// 骨骼起点处半径 r 的圆。IK 目标骨与普通骨的差别只在于圆上和圆外画了什么，
// 圆本身大小相同，由同一个属性控制。
//
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// 骨骼材质：一份网格、一个材质、一次绘制。
// 每个顶点在 UV 里携带「到形状中线的横向偏移, 该处半宽」，
// 片元里 d = |UV.y| - |UV.x| 就是到轮廓的有符号距离（轮廓处 0，内侧为正），
// 再用 fwidth(d) 换算成像素，落在描边宽度内就着描边色。
// 顶点、三角形都不多加。
// UV.y 的符号兼作标志位：负 = 该骨受 IK 约束，描边用橙色。
// 线框顶点把 |UV.y| 设成极大 → d 极大 → 永远走填充分支，原样输出顶点色，不被染色。
// ---------------------------------------------------------------------------
static Ref<ShaderMaterial> &get_bone_material() {
	static Ref<ShaderMaterial> material;
	if (material.is_null()) {
		Ref<Shader> shader;
		shader.instantiate();
		shader->set_code(R"(
shader_type canvas_item;

uniform vec4 outline_color : source_color = vec4(0.0, 0.0, 0.0, 1.0);
uniform vec4 outline_color_ik : source_color = vec4(1.0, 0.6, 0.1, 1.0);
uniform float outline_px = 1.0;
// 筝形小端半宽（局部单位）。筝形小端是垂直于轴的平头，用 UV.y 到该值的差
// 就能表示它到平头边的距离（与真实距离成正比，fwidth 归一化会消掉比例）。
uniform float tip_half = 0.0;

void fragment() {
	// d1：到两侧斜边的距离（x=±y 处为 0）。
	float d1 = abs(UV.y) - abs(UV.x);
	// d2：到筝形小端平头边的距离。只有筝形区域的 |UV.y| 才 >= tip_half；
	// 圆环/连线/端帽的 |UV.y| 都小于 tip_half，若直接相减会得到负值，
	// 经 min 后会把整个图元误判成在轮廓上。故仅在 >= tip_half 时启用，否则置极大。
	float d2 = abs(UV.y) >= tip_half ? (abs(UV.y) - tip_half) : 1e6;
	float d = min(d1, d2);
	float per_px = fwidth(d);
	float dist_px = d / max(per_px, 1e-6);
	float edge = max(fwidth(dist_px), 1e-3);
	float m = smoothstep(outline_px - edge, outline_px + edge, dist_px);
	vec4 oc = UV.y < 0.0 ? outline_color_ik : outline_color;
	COLOR = mix(vec4(oc.rgb, oc.a * COLOR.a), COLOR, m);
}
)");
		material.instantiate();
		material->set_shader(shader);
	}
	return material;
}

// 普通骨骼：圆环 + 圆心到圆上的连线（指示旋转）。线比 IK 目标的粗环细。
constexpr float PLAIN_RING_W = 0.20f; // 环厚，单位：半径
constexpr float PLAIN_SPOKE_W = 0.16f; // 连线粗细，单位：半径

// IK 目标骨：低不透明度深色圆盘 + 高不透明度粗环 + 四条十字准星短线（与粗环同宽），
// 其中一条延伸到圆心。
constexpr float IK_DISC_ALPHA = 0.35f; // 圆盘不透明度（叠加在主体色上）
constexpr float IK_RING_W = 0.34f; // 环厚，单位：半径
constexpr float IK_ARM_OUT = 1.30f; // 短线伸出环外的长度
constexpr float IK_ARM_W = 0.34f; // 短线粗细，按要求与环一致

// ---------------------------------------------------------------------------
// 筝形
//
// 圆心到筝形小端的距离就是骨骼长度，不做缩放或内缩。
// 大端在圆周上（与圆相接、无缝隙），但明显比圆窄；等宽会变成大三角而不是筝形。
// 小端在骨骼末端截平，宽度取大端的一半，保证一路收窄而不是收成一点。
constexpr float KITE_HALF_WIDTH = 1.0f; // 大端半宽，单位：半径
constexpr float KITE_SPRINT_AT = 0.75f; // 大端对角线距离大端顶点距离，单位：半宽
constexpr float KITE_TIP_HALF = 0.1f; // 骨骼末端截平边的半宽

constexpr int DISC_SEGMENTS = 32;
constexpr int CAP_SEGMENTS = 8;
constexpr int ARC_SEGMENTS = 32;

constexpr float TAU_F = Math::TAU;
constexpr float PI_F = Math::PI;

// 骨骼名称标签的字号。
constexpr int DEBUG_BONE_NAME_FONT_SIZE = 14;

// 描边线宽（屏幕像素）：固定 1~2 像素，任何缩放都不变粗。
// 直接作为 uniform 交给着色器，由 fwidth 换算，不经 CPU 几何。
static constexpr float DEBUG_OUTLINE_PX = 1.2f;

// 一根待绘制的骨骼。起点与朝向都取自合成矩阵（get_global_transform），已含父级旋转。
struct DebugBone {
	enum Kind {
		KIND_PLAIN, // 普通骨骼
		KIND_IK_TARGET, // IK 约束的 target
		KIND_IK_DRIVEN, // IK 约束作用的 bone
	};

	Vector2 start;
	Vector2 dir{ 1.0f, 0.0f }; // 单位向量，视图空间
	float length = 0.0f;
	StringName name;
	Kind kind = KIND_PLAIN;
};

Color faded(const Color &p_color, float p_factor) {
	return Color(p_color.r, p_color.g, p_color.b, p_color.a * p_factor);
}

// 描边颜色：黑色，不透明度跟随主体。
Color rim_color(const Color &p_body) {
	return Color(0.0f, 0.0f, 0.0f, p_body.a);
}

// 骨骼字形几何：**只有填充三角形**，一个三角形都不多加。
// 每个顶点带一个轮廓编码 UV=(x, y)，含义见 get_bone_material 的说明。
struct DebugDrawGeometry {
	// 顶点。
	LocalVector<Vector2> vertices;
	LocalVector<Color> colors;
	LocalVector<int32_t> indices;

	// 逐顶点轮廓编码，提交时写进 ARRAY_TEX_UV。
	LocalVector<Vector2> vertex_uv;

	// UV.y 的符号：-1 表示该骨受 IK 约束，描边改用橙色。
	float uv_sign = 1.0f;

	_FORCE_INLINE_ bool is_empty() const { return indices.is_empty(); }

	// ---- 写入底座 ----

	_FORCE_INLINE_ void body_tri(const Vector2 &a, const Vector2 &b, const Vector2 &c,
								 const Vector2 &p_uv_a, const Vector2 &p_uv_b, const Vector2 &p_uv_c, const Color &p_color) {
		const int32_t base = vertices.size();
		// 统一绕序。
		const bool flip = (b - a).cross(c - a) < 0.0f;
		vertices.push_back(a);
		vertex_uv.push_back(Vector2(p_uv_a.x, p_uv_a.y * uv_sign));
		vertices.push_back(flip ? c : b);
		vertex_uv.push_back(flip ? Vector2(p_uv_c.x, p_uv_c.y * uv_sign) : Vector2(p_uv_b.x, p_uv_b.y * uv_sign));
		vertices.push_back(flip ? b : c);
		vertex_uv.push_back(flip ? Vector2(p_uv_b.x, p_uv_b.y * uv_sign) : Vector2(p_uv_c.x, p_uv_c.y * uv_sign));
		colors.push_back(p_color);
		colors.push_back(p_color);
		colors.push_back(p_color);
		indices.push_back(base);
		indices.push_back(base + 1);
		indices.push_back(base + 2);
	}

	_FORCE_INLINE_ void body_quad(const Vector2 &a, const Vector2 &b, const Vector2 &c, const Vector2 &d,
								  const Vector2 &p_uv_a, const Vector2 &p_uv_b, const Vector2 &p_uv_c, const Vector2 &p_uv_d, const Color &p_color) {
		body_tri(a, b, c, p_uv_a, p_uv_b, p_uv_c, p_color);
		body_tri(a, c, d, p_uv_a, p_uv_c, p_uv_d, p_color);
	}

public:
	// ---- 填充 ----

	void body_circle(const Vector2 &p_c, float p_r, const Color &p_color) {
		// 实心圆盘不自带描边：所有顶点 UV 相同 → d 恒定 → 片元判定为纯填充。
		const Vector2 uv(0.0f, p_r);
		for (int i = 0; i < DISC_SEGMENTS; ++i) {
			const float a0 = TAU_F * float(i) / float(DISC_SEGMENTS);
			const float a1 = TAU_F * float(i + 1) / float(DISC_SEGMENTS);
			const Vector2 p0 = p_c + Vector2(cos(a0), sin(a0)) * p_r;
			const Vector2 p1 = p_c + Vector2(cos(a1), sin(a1)) * p_r;
			body_tri(p_c, p0, p1, uv, uv, uv, p_color);
		}
	}

	void body_annulus(const Vector2 &p_c, float p_outer, float p_inner, const Color &p_color) {
		// 内半径塌缩时不能构造：负半径会把点镜像到对侧，四边形退化成贯穿整个圆的长刺。
		if (p_inner <= 0.0f) {
			body_circle(p_c, p_outer, p_color);
			return;
		}

#ifdef DEV_ENABLED
		CRASH_COND(p_inner >= p_outer);
#endif

		const float hw = (p_outer - p_inner) * 0.5f;
		for (int i = 0; i < DISC_SEGMENTS; ++i) {
			const float a0 = TAU_F * float(i) / float(DISC_SEGMENTS);
			const float a1 = TAU_F * float(i + 1) / float(DISC_SEGMENTS);
			const Vector2 d0(cos(a0), sin(a0));
			const Vector2 d1(cos(a1), sin(a1));
			// 只有外沿是轮廓：内沿顶点的 UV.x 取 0，使 d1 = hw - |UV.x| 在内沿远离 0，不描边。
			body_quad(p_c + d0 * p_inner, p_c + d0 * p_outer, p_c + d1 * p_outer, p_c + d1 * p_inner,
					  Vector2(0.0f, hw), Vector2(hw, hw), Vector2(hw, hw), Vector2(0.0f, hw), p_color);
		}
	}

	void body_segment(const Vector2 &p_a, const Vector2 &p_b, float p_width, const Color &p_color) {
		const Vector2 axis = p_b - p_a;
		const float len = axis.length();
		if (len <= 0.0f) {
			return;
		}
		const Vector2 dir = axis / len;
		Vector2 n;
		n.x = -dir.y;
		n.y = dir.x;
		n *= p_width * 0.5f;
		const float hw = p_width * 0.5f;
		// 连线整体不描边（与 HEAD 相同）：UV.x 取 0，d1 = hw 恒远离 0。
		const Vector2 uv0(0.0f, hw);
		body_quad(p_a + n, p_b + n, p_b - n, p_a - n,
				  uv0, uv0, uv0, uv0, p_color);
		body_cap(p_a, n, p_color);
		body_cap(p_b, -n, p_color);
	}

	// 线段端, 以 p_end 为心、半径 = |n|、按 from 扫半圈。
	void body_cap(const Vector2 &p_end, const Vector2 &p_n, const Color &p_color) {
		constexpr float rad_per_seg = PI_F / float(CAP_SEGMENTS);

		const float radius = p_n.length();
		const float begin_angle = p_n.angle();

		// TODO: UV 怎么计算？
		const Vector2 uv_c(0.0f, radius);
		const Vector2 uv_arc(0.0f, radius);
		for (int i = 0; i < CAP_SEGMENTS; ++i) {
			const float a0 = begin_angle + rad_per_seg * i;
			const float a1 = begin_angle + rad_per_seg * (i + 1);
			body_tri(p_end, p_end + Vector2(cos(a0), sin(a0)) * radius, p_end + Vector2(cos(a1), sin(a1)) * radius,
					 uv_c, uv_arc, uv_arc, p_color);
		}
	}

	void body_kite(const Vector2 &p_head, const Vector2 &p_dir, const Vector2 &p_perp, const float p_length, const float p_wide_half, const float p_tip_half, const Color &p_color) {
		const float spring_ofs = p_wide_half * KITE_SPRINT_AT;
		const Vector2 spring_pos = p_head + p_dir * spring_ofs;
		const Vector2 tip_pos = p_head + p_dir * p_length;

		const Vector2 spring_a = spring_pos + p_perp * p_wide_half;
		const Vector2 spring_b = spring_pos - p_perp * p_wide_half;
		const Vector2 tip_a = tip_pos + p_perp * p_tip_half;
		const Vector2 tip_b = tip_pos - p_perp * p_tip_half;

		// TODO: 怎么计算的UV?
		const Vector2 uv_h(p_wide_half, p_wide_half);
		const Vector2 uv_sa(p_wide_half, p_wide_half);
		const Vector2 uv_sb(-p_wide_half, p_wide_half);
		const Vector2 uv_ta(p_tip_half, p_tip_half);
		const Vector2 uv_tb(-p_tip_half, p_tip_half);

		body_tri(p_head, spring_a, spring_b, uv_h, uv_sa, uv_sb, p_color);
		body_tri(spring_a, tip_a, tip_b, uv_sa, uv_ta, uv_tb, p_color);
		body_tri(spring_a, tip_b, spring_b, uv_sa, uv_tb, uv_sb, p_color);
	}
};

} //namespace

void append_debug_bone_geometry(const DebugBone &p_bone, const DebugDraw &p_props,
								DebugDrawGeometry &r_geometry) {
	const float radius = MAX(p_props.get_bone_pivot_radius(), 0.5f);

	const Vector2 dir = p_bone.dir;
	const Vector2 perp(-dir.y, dir.x);
	const Vector2 &center = p_bone.start;

	const bool is_ik_target = p_bone.kind == DebugBone::KIND_IK_TARGET;
	// 受 IK 约束的骨骼描边用橙色：用 UV.y 的符号把标记传给着色器。
	r_geometry.uv_sign = (p_bone.kind == DebugBone::KIND_IK_DRIVEN) ? -1.0f : 1.0f;

	// ------------------------------------------------------------------
	// 枢轴是「起点圆 + 筝形」合一的一个轮廓。
	// ------------------------------------------------------------------
	const float diameter = radius * 2.0f;

	const bool has_kite = p_bone.length > diameter;
	const Color body_color = is_ik_target ? p_props.color_ik_target : p_props.color_bone;

	if (has_kite) {
		const Vector2 head = center + dir * radius;
		const float kite_length = p_bone.length - radius;
		const float width_half = radius * KITE_HALF_WIDTH;
		const float tip_half = radius * KITE_TIP_HALF;
		r_geometry.body_kite(head, dir, perp, kite_length, width_half, tip_half, body_color);
	}

	const float ring_w = radius * (is_ik_target ? IK_RING_W : PLAIN_RING_W);
	const float ring_outer = radius + ring_w * 0.5;
	const float ring_inner = radius - ring_w * 0.5;
	r_geometry.body_annulus(center, ring_outer, ring_inner, body_color);

	if (is_ik_target) {
		// 粗环内部的低不透明度深色圆盘。
		r_geometry.body_circle(center, ring_inner, faded(body_color, IK_DISC_ALPHA));
	}

	// ------------------------------------------------------------------
	// 连线：从圆心向外，指示骨骼旋转。
	// ------------------------------------------------------------------
	const float spoke_w = radius * (is_ik_target ? IK_ARM_W : PLAIN_SPOKE_W);

	if (is_ik_target) {
		const float arm_out = radius * IK_ARM_OUT;
		const float arm_in = radius * 0.6f;

		const float spoke_to = has_kite ? (radius - spoke_w * 0.5f) : (MAX(p_bone.length, arm_out));
		r_geometry.body_segment(center, center + dir * spoke_to, spoke_w, body_color);

		r_geometry.body_segment(center - perp * arm_out, center - perp * arm_in, spoke_w, body_color);
		r_geometry.body_segment(center + perp * arm_out, center + perp * arm_in, spoke_w, body_color);
		r_geometry.body_segment(center - dir * arm_out, center - dir * arm_in, spoke_w, body_color);
	} else {
		const float spoke_to = has_kite ? (radius - spoke_w * 0.5f) : (MAX(p_bone.length, radius - spoke_w * 0.5f));
		r_geometry.body_segment(center, center + dir * spoke_to, spoke_w, body_color);
	}
}

void draw_debug_bone_names(CanvasItem *p_owner, const LocalVector<DebugBone> &p_bone_data, const DebugDraw &p_props) {
	const Ref<Font> font = ThemeDB::get_singleton()->get_fallback_font();
	if (font.is_null()) {
		return;
	}

	const int font_size = DEBUG_BONE_NAME_FONT_SIZE;
	const float label_pad = p_props.get_bone_pivot_radius() * 2.0f + 2.0f;

	for (const DebugBone &bone : p_bone_data) {
		const Vector2 perp(-bone.dir.y, bone.dir.x);
		const Vector2 pos = bone.start + perp * label_pad;
		const Color name_color = bone.kind == DebugBone::KIND_IK_TARGET
				? p_props.color_ik_target
				: p_props.color_bone;

		p_owner->draw_string(font, pos, bone.name, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, name_color);
		p_owner->draw_string_outline(font, pos, bone.name, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, 1, rim_color(name_color));
	}
}

template <typename PackedArray, typename Elem, std::enable_if_t<std::is_same_v<std::decay_t<decltype(PackedArray()[0])>, Elem>> *_dummy = nullptr>
PackedArray to_packed_array(const LocalVector<Elem> &p_points) {
	PackedArray out;
	out.resize(p_points.size());
	memcpy((uint8_t *)out.ptrw(), (uint8_t *)p_points.ptr(), p_points.size() * sizeof(Elem));
	return out;
}

DebugDraw::~DebugDraw() {
	const auto RS = RenderingServer::get_singleton();
	if (debug_mesh.is_valid()) {
		RenderingServer::get_singleton()->free_rid(debug_mesh);
	}
}

void DebugDraw::set_enabled(bool p_enabled) {
	if (p_enabled) {
		if (!debug_mesh.is_valid())
			debug_mesh = RenderingServer::get_singleton()->mesh_create();
	} else {
		if (debug_mesh.is_valid()) {
			RenderingServer::get_singleton()->free_rid(debug_mesh);
			debug_mesh = {};
		}
	}
}

void DebugDraw::draw(DragonBonesArmature *p_root_armature, const DrawData &p_draw_data) {
	ERR_FAIL_NULL(p_root_armature);

	const auto RS = RenderingServer::get_singleton();
	RS->mesh_clear(debug_mesh);

	const Transform2D identity{};

	uint32_t surface_idx = -1;

	// ---- 插槽线框 ----
	if (has_flag(DRAW_MESH)) {
		PackedInt32Array debug_mesh_indices;
		PackedVector2Array debug_vertices;
		PackedColorArray debug_colors;

		for (const DrawData::Layer &layer : p_draw_data) {
			for (const auto &data : layer.data) {
				auto base_index = debug_vertices.size();
				auto insert_begin_index = debug_mesh_indices.size();
				auto data_indices_count = data.indices.size();

				debug_mesh_indices.resize(debug_mesh_indices.size() + data_indices_count);
				auto idx_ptrw = debug_mesh_indices.ptrw() + insert_begin_index;
				auto src_ptr = data.indices.ptr();

				while (data_indices_count > 0) {
					*idx_ptrw = *src_ptr + base_index;
					++idx_ptrw;
					++src_ptr;
					--data_indices_count;
				}

				debug_vertices.append_array(data.transform.xform(data.vertices));

				PackedColorArray colors;
				colors.resize(data.vertices.size());
				colors.fill(data.debug_color);
				debug_colors.append_array(colors);
			}
		}

		// 三角形拆成线段。
		PackedInt32Array line_indices;
		line_indices.resize(debug_mesh_indices.size() * 2);
		for (int n = 0; n < debug_mesh_indices.size(); n += 3) {
			const int bi = 2 * n;
			line_indices[bi] = debug_mesh_indices[n];
			line_indices[bi + 1] = debug_mesh_indices[n + 1];
			line_indices[bi + 2] = debug_mesh_indices[n + 1];
			line_indices[bi + 3] = debug_mesh_indices[n + 2];
			line_indices[bi + 4] = debug_mesh_indices[n + 2];
			line_indices[bi + 5] = debug_mesh_indices[n];
		}

		if (!line_indices.is_empty()) {
			// 线框不打描边：UV.y 取极大 → d 极大 → 片元直接输出顶点色。
			PackedVector2Array line_uv;
			line_uv.resize(debug_vertices.size());
			for (int i = 0; i < line_uv.size(); ++i) {
				line_uv[i] = Vector2(0.0f, 1e6f);
			}

			Array arr;
			arr.resize(RenderingServer::ARRAY_MAX);
			arr[RenderingServer::ARRAY_INDEX] = line_indices;
			arr[RenderingServer::ARRAY_VERTEX] = debug_vertices;
			arr[RenderingServer::ARRAY_COLOR] = debug_colors;
			arr[RenderingServer::ARRAY_TEX_UV] = line_uv;
			RS->mesh_add_surface_from_arrays(debug_mesh, RenderingServer::PRIMITIVE_LINES, arr);
			surface_idx++;
		}
	}

	// ---- 骨骼 ----
	// 描边和填充放进同一个表面，填充在后，于是每个形状盖住自己的内侧描边。
	if (draw_flags & (DRAW_BONE | DRAW_BONE_NAME)) {
		if (!cached) {
			cache_ik_bones(p_root_armature);
		}

		LocalVector<DebugBone> bone_data; // TODO: 是否作为成员变量进行缓存比较好？

		Transform2D global_transform{};
		p_root_armature->for_each_armature_recursively([&global_transform, &ik_targets = ik_targets, &ik_driven = ik_driven, &bone_data](DragonBonesArmature *p_armature, int) {
			global_transform = global_transform * p_armature->transform;
			for (const auto &[bone_name, bone] : p_armature->get_bones()) {
				if (!bone.is_valid()) {
					continue;
				}

				// 再叠加外层骨架的基准变换，使嵌套骨架的坐标也落到同一空间。
				const Transform2D bone_transform = global_transform * bone->get_global_transform();
				const Vector2 start = bone_transform.get_origin();
				// 朝向取该变换的 X 轴。必须走同一个 `bone_transform`，否则嵌套骨架里
				const Vector2 direction = bone_transform[0].normalized();

				// 长度取自骨架数据。
				const float length = bone->get_length();

				DebugBone::Kind kind = DebugBone::KIND_PLAIN;
				if (ik_targets.has(bone_name)) {
					kind = DebugBone::KIND_IK_TARGET;
				} else if (ik_driven.has(bone_name)) {
					kind = DebugBone::KIND_IK_DRIVEN;
				}

				bone_data.push_back({
						start,
						direction,
						length,
						bone_name,
						kind,
				});
			}
		});

		if (has_flag(DRAW_BONE)) {
			// 所有骨骼的填充几何收进同一个表面；描边由材质在片元里按 UV 距离算。
			DebugDrawGeometry geometry;
			for (const DebugBone &bone : bone_data) {
				append_debug_bone_geometry(bone, *this, geometry);
			}

			if (!geometry.indices.is_empty()) {
				Array arr;
				arr.resize(RenderingServer::ARRAY_MAX);
				arr[RenderingServer::ARRAY_INDEX] = to_packed_array<PackedInt32Array>(geometry.indices);
				arr[RenderingServer::ARRAY_VERTEX] = to_packed_array<PackedVector2Array>(geometry.vertices);
				arr[RenderingServer::ARRAY_COLOR] = to_packed_array<PackedColorArray>(geometry.colors);
				arr[RenderingServer::ARRAY_TEX_UV] = to_packed_array<PackedVector2Array>(geometry.vertex_uv);
				RS->mesh_add_surface_from_arrays(debug_mesh, RenderingServer::PRIMITIVE_TRIANGLES, arr);
				surface_idx++;
				RS->mesh_surface_set_material(debug_mesh, surface_idx, get_bone_material()->get_rid());
			}
		}

		if (has_flag(DRAW_BONE_NAME)) {
			draw_debug_bone_names(owner, bone_data, *this);
		}
	}

	// 线框 + 骨骼一次提交：同一份 debug_mesh、一个材质（材质挂在子画布上）。
	if (is_enabled()) {
		get_bone_material()->set_shader_parameter("outline_px", DEBUG_OUTLINE_PX);
		get_bone_material()->set_shader_parameter("outline_color_ik", color_ik_bone_outline);
		// 筝形小端半宽（局部单位），供着色器描小端平头。
		get_bone_material()->set_shader_parameter("tip_half", MAX(get_bone_pivot_radius(), 0.5f) * KITE_TIP_HALF);
		RS->canvas_item_add_mesh(owner->get_canvas_item(), debug_mesh, identity, Color(1, 1, 1, 1));
	}
}

void DebugDraw::clear_cache() {
	ik_targets.clear();
	ik_driven.clear();
}

void DebugDraw::cache_ik_bones(DragonBonesArmature *p_armature) {
	p_armature->for_each_armature_recursively([this](DragonBonesArmature *p_a, int) {
		for (const dragonBones::Constraint *constraint : p_a->getArmature()->_constraints) {
			if (constraint == nullptr || constraint->_constraintData == nullptr) {
				continue;
			}
			if (constraint->_constraintData->target != nullptr) {
				ik_targets.push_back(StringName(to_gd_str(constraint->_constraintData->target->name)));
			}
			if (constraint->_constraintData->root != nullptr) {
				ik_driven.push_back(StringName(to_gd_str(constraint->_constraintData->root->name)));
			}
			if (constraint->_constraintData->bone != nullptr) {
				ik_driven.push_back(StringName(to_gd_str(constraint->_constraintData->bone->name)));
			}
		}
	});

	cached = true;
}

#endif // DEBUG_ENABLED
} //namespace godot
