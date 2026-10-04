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
constexpr float KITE_WIDE_AT = 1.00f; // 大端位置，单位：半径
constexpr float KITE_HALF_WIDTH = 0.45f; // 大端半宽，单位：半径
constexpr float KITE_TIP_HALF = 0.28f; // 骨骼末端截平边的半宽

constexpr int DISC_SEGMENTS = 32;
constexpr int CAP_SEGMENTS = 8;
constexpr int ARC_SEGMENTS = 32;

constexpr float TAU_F = 6.28318531f;
constexpr float HALF_TURN_F = 3.14159265f;

// 描边在 CPU 上外扩的最小屏幕像素数，保证任何缩放下都看得见。
// `Geometry` 用局部单位，调用方负责换算。
constexpr float OUTLINE_MIN_PX = 0.75f;

// 逐顶点自定义值（供描边柔化使用）：0 在外扩边缘，1 在原轮廓上。
// 两条边界在屏幕空间正好相隔一个描边宽度，插值即得线性距离。
constexpr float EDGE_OUTER = 0.0f;
constexpr float EDGE_INNER = 1.0f;

// 骨骼名称标签的字号（屏幕像素）。
constexpr int DEBUG_BONE_NAME_FONT_SIZE = 14;

// 描边线宽（屏幕像素）：固定 1~2 像素，任何缩放都不变粗。
// 视图按节点自身缩放换算成局部单位后再交给几何构建。
static constexpr float DEBUG_OUTLINE_PX = 1.6f;

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

// 一根骨骼的几何，分两层，最终成为两个网格表面：
//
//   border（描边）——各形状外扩后的轮廓；
//   body（填充）——各形状本体。
// 先描边后填充，填充盖住自己内侧的描边，每根骨骼只留外轮廓，也不会盖住相邻骨骼的描边。
struct DebugDrawGeometry {
	LocalVector<Vector2> border_vertices;
	LocalVector<Color> border_colors;
	LocalVector<float> border_edges;
	LocalVector<int32_t> border_indices;

	LocalVector<Vector2> body_vertices;
	LocalVector<Color> body_colors;
	LocalVector<float> body_edges;
	LocalVector<int32_t> body_indices;

	_FORCE_INLINE_ bool is_empty() const { return body_indices.is_empty() && border_indices.is_empty(); }
};

Color faded(const Color &p_color, float p_factor) {
	return Color(p_color.r, p_color.g, p_color.b, p_color.a * p_factor);
}

// 描边颜色：黑色，不透明度跟随主体。
Color rim_color(const Color &p_body) {
	return Color(0.0f, 0.0f, 0.0f, p_body.a);
}

// 几何构建器：分别向 border / body 两层写顶点，详见 DebugDrawGeometry。
class Geometry {
public:
	LocalVector<Vector2> border_vertices;
	LocalVector<Color> border_colors;
	LocalVector<float> border_edges;
	LocalVector<int32_t> border_indices;

	LocalVector<Vector2> body_vertices;
	LocalVector<Color> body_colors;
	LocalVector<float> body_edges;
	LocalVector<int32_t> body_indices;

private:
	float rim;

	_FORCE_INLINE_ void body_raw(const Vector2 &a, const Vector2 &b, const Vector2 &c, const Color &p_color) {
		const int32_t base = body_vertices.size();
		body_vertices.push_back(a);
		body_vertices.push_back(b);
		body_vertices.push_back(c);
		body_colors.push_back(p_color);
		body_colors.push_back(p_color);
		body_colors.push_back(p_color);
		body_edges.push_back(EDGE_INNER);
		body_edges.push_back(EDGE_INNER);
		body_edges.push_back(EDGE_INNER);
		body_indices.push_back(base);
		body_indices.push_back(base + 1);
		body_indices.push_back(base + 2);
	}

	// 描边四边形：p_in_* 在形状上，p_out_* 在外扩后的轮廓上。
	//
	void border_raw(const Vector2 &p_in_a, const Vector2 &p_in_b, const Vector2 &p_out_b, const Vector2 &p_out_a,
					const Color &p_color) {
		const int32_t base = border_vertices.size();
		border_vertices.push_back(p_out_a);
		border_vertices.push_back(p_out_b);
		border_vertices.push_back(p_in_b);
		border_vertices.push_back(p_in_a);
		border_colors.push_back(p_color);
		border_colors.push_back(p_color);
		border_colors.push_back(p_color);
		border_colors.push_back(p_color);
		border_edges.push_back(EDGE_OUTER);
		border_edges.push_back(EDGE_OUTER);
		border_edges.push_back(EDGE_INNER);
		border_edges.push_back(EDGE_INNER);
		border_indices.push_back(base);
		border_indices.push_back(base + 1);
		border_indices.push_back(base + 2);
		border_indices.push_back(base);
		border_indices.push_back(base + 2);
		border_indices.push_back(base + 3);
	}

public:
	explicit Geometry(float p_rim) :
			rim(p_rim) {}

	_FORCE_INLINE_ float rim_width() const { return rim; }

	_FORCE_INLINE_ void body_tri(const Vector2 &a, const Vector2 &b, const Vector2 &c, const Color &p_color) {
		body_raw(a, b, c, p_color);
	}

	_FORCE_INLINE_ void body_quad(const Vector2 &a, const Vector2 &b, const Vector2 &c, const Vector2 &d, const Color &p_color) {
		body_raw(a, b, c, p_color);
		body_raw(a, c, d, p_color);
	}

	void body_disc(const Vector2 &p_c, float p_r, const Color &p_color) {
		for (int i = 0; i < DISC_SEGMENTS; ++i) {
			const float a0 = TAU_F * float(i) / float(DISC_SEGMENTS);
			const float a1 = TAU_F * float(i + 1) / float(DISC_SEGMENTS);
			body_raw(p_c, p_c + Vector2(cos(a0), sin(a0)) * p_r, p_c + Vector2(cos(a1), sin(a1)) * p_r, p_color);
		}
	}

	void body_annulus(const Vector2 &p_c, float p_outer, float p_inner, const Color &p_color) {
		// 内半径塌缩时不能构造：负半径会把点镜像到对侧，四边形退化成贯穿整个圆的长刺。
		if (p_inner <= 0.0f) {
			body_disc(p_c, p_outer, p_color);
			return;
		}
		if (p_inner >= p_outer) {
			return;
		}
		for (int i = 0; i < DISC_SEGMENTS; ++i) {
			const float a0 = TAU_F * float(i) / float(DISC_SEGMENTS);
			const float a1 = TAU_F * float(i + 1) / float(DISC_SEGMENTS);
			const Vector2 d0(cos(a0), sin(a0));
			const Vector2 d1(cos(a1), sin(a1));
			body_quad(p_c + d0 * p_inner, p_c + d0 * p_outer, p_c + d1 * p_outer, p_c + d1 * p_inner, p_color);
		}
	}

	void body_arc(const Vector2 &p_c, float p_outer, float p_inner, float p_from, float p_to, const Color &p_color) {
		for (int i = 0; i < ARC_SEGMENTS; ++i) {
			const float a0 = p_from + (p_to - p_from) * float(i) / float(ARC_SEGMENTS);
			const float a1 = p_from + (p_to - p_from) * float(i + 1) / float(ARC_SEGMENTS);
			const Vector2 d0(cos(a0), sin(a0));
			const Vector2 d1(cos(a1), sin(a1));
			body_quad(p_c + d0 * p_inner, p_c + d0 * p_outer, p_c + d1 * p_outer, p_c + d1 * p_inner, p_color);
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
		body_quad(p_a + n, p_b + n, p_b - n, p_a - n, p_color);
		body_cap(p_a, dir, n, p_color);
		body_cap(p_b, dir, n, p_color);
	}

private:
	void body_cap(const Vector2 &p_end, const Vector2 &p_dir, const Vector2 &p_n, const Color &p_color) {
		const float base = Math::atan2(p_n.y, p_n.x);
		const float from = (p_dir.x * p_n.x + p_dir.y * p_n.y) > 0.0f ? base : base + HALF_TURN_F;
		const float r = p_n.length() * 0.5f;
		for (int i = 0; i < CAP_SEGMENTS; ++i) {
			const float a0 = from - HALF_TURN_F * float(i) / float(CAP_SEGMENTS);
			const float a1 = from - HALF_TURN_F * float(i + 1) / float(CAP_SEGMENTS);
			body_raw(p_end, p_end + Vector2(cos(a0), sin(a0)) * r, p_end + Vector2(cos(a1), sin(a1)) * r, p_color);
		}
	}

public:
	void border_disc(const Vector2 &p_c, float p_r, const Color &p_color) {
		const float outer = p_r + rim;
		for (int i = 0; i < DISC_SEGMENTS; ++i) {
			const float a0 = TAU_F * float(i) / float(DISC_SEGMENTS);
			const float a1 = TAU_F * float(i + 1) / float(DISC_SEGMENTS);
			const Vector2 d0(cos(a0), sin(a0));
			const Vector2 d1(cos(a1), sin(a1));
			border_raw(p_c + d0 * p_r, p_c + d1 * p_r, p_c + d1 * outer, p_c + d0 * outer, p_color);
		}
	}

	void border_annulus(const Vector2 &p_c, float p_outer, float p_inner, const Color &p_color) {
		const float grown = p_outer + rim;
		for (int i = 0; i < DISC_SEGMENTS; ++i) {
			const float a0 = TAU_F * float(i) / float(DISC_SEGMENTS);
			const float a1 = TAU_F * float(i + 1) / float(DISC_SEGMENTS);
			const Vector2 d0(cos(a0), sin(a0));
			const Vector2 d1(cos(a1), sin(a1));
			border_raw(p_c + d0 * p_inner, p_c + d1 * p_inner, p_c + d1 * grown, p_c + d0 * grown, p_color);
		}
	}

	// 一段直线描边：边本身与它外扩后的边之间的四边形。
	// p_outward 是形状外侧方向，必须由调用方给出：轮廓是绕圈画的，
	// 筝形两条边走向相反，用固定法线会让其中一条向内扩，那一侧就没有描边。
	void border_edge(const Vector2 &p_a, const Vector2 &p_b, const Vector2 &p_outward, const Color &p_color) {
		const Vector2 axis = p_b - p_a;
		if (axis.length() <= 0.0f) {
			return;
		}
		border_raw(p_a, p_b, p_b + p_outward * rim, p_a + p_outward * rim, p_color);
	}

	// 圆弧描边，从 p_from 扫到 p_to。用来画筝形没盖住的那段圆环，
	// 使圆环与筝形的描边连成一条闭合轮廓。
	void border_arc(const Vector2 &p_c, float p_r, float p_from, float p_to, const Color &p_color) {
		float sweep = p_to - p_from;
		if (sweep <= 0.0f) {
			return;
		}
		if (sweep >= TAU_F) {
			sweep = TAU_F;
		}
		const float outer = p_r + rim;
		const int steps = MAX(1, int(ceilf(sweep * float(DISC_SEGMENTS) / TAU_F)));
		const float step = sweep / float(steps);
		for (int i = 0; i < steps; ++i) {
			const float a0 = p_from + step * float(i);
			const float a1 = a0 + step;
			const Vector2 d0(cos(a0), sin(a0));
			const Vector2 d1(cos(a1), sin(a1));
			border_raw(p_c + d0 * p_r, p_c + d1 * p_r, p_c + d1 * outer, p_c + d0 * outer, p_color);
		}
	}
};

} //namespace

void append_debug_bone_geometry(const DebugBone &p_bone, const DebugDraw &p_props, float p_outline_px,
								DebugDrawGeometry &r_geometry) {
	const float radius = MAX(p_props.get_bone_pivot_radius(), 0.5f);
	Geometry g(MAX(p_outline_px, OUTLINE_MIN_PX));

	const Vector2 dir = p_bone.dir;
	const Vector2 perp(-dir.y, dir.x);
	const Vector2 &center = p_bone.start;

	const bool is_ik_target = p_bone.kind == DebugBone::KIND_IK_TARGET;
	const bool is_ik_driven = p_bone.kind == DebugBone::KIND_IK_DRIVEN;

	// ------------------------------------------------------------------
	// 枢轴是「起点圆 + 筝形」合一的一个轮廓：先填两者，再整体描一圈边。
	// 分别描会在接缝处多出一条线，把一个符号切成两个。
	//
	// ------------------------------------------------------------------
	const float diameter = radius * 2.0f;

	// 骨骼长于圆的直径才画筝形。否则用符号自身的连线延伸到骨骼末端来指示长度，
	// 是同一根线画得更长，不是再叠一根。
	//
	const bool has_kite = p_bone.length > diameter;
	const float spoke_to = MIN(MAX(p_bone.length, radius), radius * 3.0f);

	const Color body_color = is_ik_target ? p_props.color_ik_target : p_props.color_bone;
	// 描边色：受 IK 约束的骨骼用 color_ik_bone_outline 本身（含它自己的不透明度）；
	// 其余骨骼用黑边，不透明度跟随主体（主体 0.8 描边就 0.8，IK 目标的 0.9 就 0.9）。
	const Color outline_color = is_ik_driven ? p_props.color_ik_bone_outline : rim_color(body_color);

	// ------------------------------------------------------------------
	// 筝形：从圆周一路收窄到骨骼末端，末端距圆心正好 p_bone.length，用它来度量骨骼。
	// 唯一例外是刚超过大端的短骨，小端被稍稍推后，保证仍能收窄而不是方头。
	// ------------------------------------------------------------------
	const float half = radius * KITE_HALF_WIDTH;
	const float tip_half = radius * KITE_TIP_HALF;
	const float wide_at = radius * KITE_WIDE_AT;
	const float tip_at = MAX(p_bone.length, wide_at + radius * 0.25f);

	// 小端是短边而不是一个点，收窄才看得出来，描边也有地方收口。
	//
	const Vector2 tip_a = center + dir * tip_at + perp * tip_half;
	const Vector2 tip_b = center + dir * tip_at - perp * tip_half;

	// ------------------------------------------------------------------
	// 起点圆环：只在筝形没盖到的那段圆弧上填充，被筝形盖住的部分由筝形填充接管，
	// 这样两者看起来是一个整体。
	const float ring_w = radius * (is_ik_target ? IK_RING_W : PLAIN_RING_W);
	const float ring_inner = radius - ring_w;
	// 筝形只盖住两个起点角之间很窄的一段弧，所以圆环的填充和描边都要覆盖整圆减去这一段。
	// 这里是该边界的两个角度（按半圈算会让每个有筝形的骨缺一大段）。
	const Vector2 spring_a = center + dir * radius + perp * half;
	const Vector2 spring_b = center + dir * radius - perp * half;
	const float trail = Math::atan2((spring_b - center).y, (spring_b - center).x);
	const float lead = Math::atan2((spring_a - center).y, (spring_a - center).x);

	if (has_kite) {
		// 筝形大端在圆周上，两角落在圆环外缘，与圆环所画圆弧正好衔接。
		// 筝形是凸的，从大端扇形三角化即可填满。
		//
		g.body_tri(spring_a, tip_a, tip_b, body_color);
		g.body_tri(spring_a, tip_b, spring_b, body_color);

		// 圆环：圆上除筝形所盖窄楔外的其余部分，也就是绝大部分。
		//
		g.body_arc(center, radius, ring_inner, lead, trail + TAU_F, body_color);
	} else {
		g.body_annulus(center, radius, ring_inner, body_color);
	}

	if (is_ik_target) {
		// 粗环内部的低不透明度深色圆盘。
		g.body_disc(center, ring_inner, faded(body_color, IK_DISC_ALPHA));
	}

	// ------------------------------------------------------------------
	// 连线：从圆心向外，指示骨骼旋转。只画一次、按最终长度画，短骨就是它伸得更长。
	//
	// ------------------------------------------------------------------
	const float spoke_w = radius * (is_ik_target ? IK_ARM_W : PLAIN_SPOKE_W);
	g.body_segment(center, center + dir * spoke_to, spoke_w, body_color);

	if (is_ik_target) {
		// 其余三条十字准星短线，与环同宽，止于圆盘外，只有连线能到圆心。
		//
		const float arm_out = radius * IK_ARM_OUT;
		const float arm_in = radius + spoke_w * 0.5f;
		g.body_segment(center - perp * arm_out, center - perp * arm_in, spoke_w, body_color);
		g.body_segment(center + perp * arm_out, center + perp * arm_in, spoke_w, body_color);
		g.body_segment(center - dir * arm_out, center - dir * arm_in, spoke_w, body_color);
	}

	// ------------------------------------------------------------------
	// 描边：绕合一后的轮廓走一圈——筝形两条边到小端、小端、再从后角沿圆弧绕回前角。
	// 筝形盖住的那段跳过，那里的轮廓由筝形自己的边充当，接缝属于内部。
	// ------------------------------------------------------------------
	if (has_kite) {
		// 每条边都朝筝形外侧外扩，无论走向如何描边都落在形状外；小端沿骨骼方向外扩。
		g.border_edge(spring_b, tip_b, -perp, outline_color);
		g.border_edge(tip_b, tip_a, dir, outline_color);
		g.border_edge(tip_a, spring_a, perp, outline_color);
		g.border_arc(center, radius, lead, trail + TAU_F, outline_color);
	} else {
		g.border_arc(center, radius, 0.0f, TAU_F, outline_color);
	}

	// 这里是追加而不是赋值：调用方把所有骨骼累积进同一份几何，赋值只会留下最后一根。
	// 索引相对 g 是局部的，要按「已存在的顶点数」（不是索引数）重新基准。
	// LocalVector 没有 append_array，只能逐个扩容拷贝。
	auto append_all = [](auto &p_dst, const auto &p_src) {
		const uint32_t base = p_dst.size();
		p_dst.resize(base + p_src.size());
		using ElemTy = std::remove_pointer_t<decltype(p_src.ptr())>;
		memcpy((uint8_t *)(p_dst.ptr() + base), (uint8_t *)p_src.ptr(), sizeof(ElemTy) * p_src.size());
	};
	auto append_indices = [](LocalVector<int32_t> &p_dst, const LocalVector<int32_t> &p_src, int32_t p_vertex_base) {
		const uint32_t base = p_dst.size();
		p_dst.resize(base + p_src.size());
		for (uint32_t i = 0; i < p_src.size(); ++i) {
			p_dst[base + i] = p_src[i] + p_vertex_base;
		}
	};
	const int32_t border_vertex_base = static_cast<int32_t>(r_geometry.border_vertices.size());
	append_all(r_geometry.border_vertices, g.border_vertices);
	append_all(r_geometry.border_colors, g.border_colors);
	append_indices(r_geometry.border_indices, g.border_indices, border_vertex_base);

	const int32_t body_vertex_base = static_cast<int32_t>(r_geometry.body_vertices.size());
	append_all(r_geometry.body_vertices, g.body_vertices);
	append_all(r_geometry.body_colors, g.body_colors);
	append_indices(r_geometry.body_indices, g.body_indices, body_vertex_base);
}

void draw_debug_bone_names(CanvasItem *p_owner, const LocalVector<DebugBone> &p_bone_data, const DebugDraw &p_props) {
	const Ref<Font> font = ThemeDB::get_singleton()->get_fallback_font();
	if (font.is_null()) {
		return;
	}

	// 字号固定，不随节点缩放、2D 缩放或窗口拉伸补偿。
	const int font_size = DEBUG_BONE_NAME_FONT_SIZE;
	const float label_pad = p_props.get_bone_pivot_radius() * 2.0f + 2.0f;

	for (const DebugBone &bone : p_bone_data) {
		const Vector2 perp(-bone.dir.y, bone.dir.x);
		const Vector2 pos = bone.start + perp * label_pad;
		const Color name_color = bone.kind == DebugBone::KIND_IK_TARGET
				? p_props.color_ik_target
				: p_props.color_bone;

		p_owner->draw_string(font, pos, bone.name, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, name_color);
	}
}

template <typename PackedArray, typename Elem, std::enable_if_t<std::is_same_v<std::decay_t<decltype(PackedArray()[0])>, Elem>> *_dummy = nullptr>
PackedArray to_packed_array(const LocalVector<Elem> &p_points) {
	PackedArray out;
	out.resize(p_points.size());
	memcpy((uint8_t *)out.ptrw(), (uint8_t *)p_points.ptr(), p_points.size() * sizeof(Elem));
	return out;
}

void DebugDraw::draw(DragonBonesArmature *p_root_armature, const DrawData &p_draw_data, const RID &p_debug_mesh) {
	ERR_FAIL_NULL(p_root_armature);

	const auto RS = RenderingServer::get_singleton();
	RS->mesh_clear(p_debug_mesh);

	const Transform2D identity{};

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
			Array arr;
			arr.resize(RenderingServer::ARRAY_MAX);
			arr[RenderingServer::ARRAY_INDEX] = line_indices;
			arr[RenderingServer::ARRAY_VERTEX] = debug_vertices;
			arr[RenderingServer::ARRAY_COLOR] = debug_colors;
			RS->mesh_add_surface_from_arrays(p_debug_mesh, RenderingServer::PRIMITIVE_LINES, arr);
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
			if (!bone_data.is_empty()) {
				DebugDrawGeometry geometry;
				// 描边固定为若干屏幕像素，任何缩放都不变粗；按节点自身缩放换算成局部单位。
				const float draw_scale = Math::abs(owner->get_global_transform_with_canvas().get_scale().y);
				const float outline_px = draw_scale > 0.0f ? DEBUG_OUTLINE_PX / draw_scale : DEBUG_OUTLINE_PX;
				for (const DebugBone &bone : bone_data) {
					append_debug_bone_geometry(bone, *this, outline_px, geometry);
				}

				if (!geometry.body_indices.is_empty()) {
					// 填充索引整体后移，跳过前面所有描边顶点。
					const int32_t base = static_cast<int32_t>(geometry.border_vertices.size());

					PackedVector2Array vertices = to_packed_array<PackedVector2Array>(geometry.border_vertices);
					vertices.append_array(to_packed_array<PackedVector2Array>(geometry.body_vertices));

					PackedColorArray colors = to_packed_array<PackedColorArray>(geometry.border_colors);
					colors.append_array(to_packed_array<PackedColorArray>(geometry.body_colors));

					PackedInt32Array indices = to_packed_array<PackedInt32Array>(geometry.border_indices);
					const PackedInt32Array body_indices = to_packed_array<PackedInt32Array>(geometry.body_indices);
					const int32_t border_count = indices.size();
					indices.resize(border_count + body_indices.size());
					for (int32_t i = 0; i < body_indices.size(); ++i) {
						indices[border_count + i] = body_indices[i] + base;
					}

					Array arr;
					arr.resize(RenderingServer::ARRAY_MAX);
					arr[RenderingServer::ARRAY_INDEX] = indices;
					arr[RenderingServer::ARRAY_VERTEX] = vertices;
					arr[RenderingServer::ARRAY_COLOR] = colors;
					RS->mesh_add_surface_from_arrays(p_debug_mesh, RenderingServer::PRIMITIVE_TRIANGLES, arr);
				}
			}
		}

		if (has_flag(DRAW_BONE_NAME)) {
			draw_debug_bone_names(owner, bone_data, *this);
		}
	}

	// canvas_item_add_mesh 会提交网格的全部表面，因此在所有表面就位后只调用一次。
	if (draw_flags & ~(DRAW_ENABLED)) {
		RS->canvas_item_add_mesh(owner->get_canvas_item(), p_debug_mesh, identity, owner->get_modulate());
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
