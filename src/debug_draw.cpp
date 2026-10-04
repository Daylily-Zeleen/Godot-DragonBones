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
// Pivot symbol
//
// The pivot is a circle of radius `r` at the bone's start. An IK target and an ordinary
// bone differ in what sits on and around that circle, never in the circle's own size:
// one property scales both.
// ---------------------------------------------------------------------------

// Ordinary bone: the circle outline, and the spoke from the centre out to the ring that
// shows the bone's rotation. The line is thinner than an IK target's ring, as specified.
constexpr float PLAIN_RING_W = 0.20f; // ring thickness, in radii
constexpr float PLAIN_SPOKE_W = 0.16f; // spoke thickness, in radii

// IK target: a low-opacity dark disc, a high-opacity thick ring around it, and four
// crosshair arms of the same thickness as that ring, one of which reaches the centre.
constexpr float IK_DISC_ALPHA = 0.35f; // disc opacity, over the body colour
constexpr float IK_RING_W = 0.34f; // ring thickness, in radii
constexpr float IK_ARM_OUT = 1.30f; // arm reach outside the ring
constexpr float IK_ARM_W = 0.34f; // arm thickness, equal to the ring by specification

// ---------------------------------------------------------------------------
// Kite
//
// The kite's tip sits exactly at the bone's end: the distance from the pivot's centre to
// that tip IS the bone's length, which is the whole point of the glyph. It is not scaled
// down or inset - shortening it would throw away the one thing the kite communicates.
// The wide end starts on the pivot circle so pivot and kite meet without a gap, but it is
// much narrower than the circle. At the full diameter the kite is exactly as wide as the
// ring it grows out of, which reads as a broad triangle rather than a kite.
//
// The tip is at the bone's end, and the truncated end there is half the wide end, so the
// shape tapers all the way rather than vanishing to nothing.
constexpr float KITE_WIDE_AT = 1.00f; // wide end, in radii
constexpr float KITE_HALF_WIDTH = 0.45f; // half the wide end, in radii
constexpr float KITE_TIP_HALF = 0.28f; // half the truncated end at the bone's end

constexpr int DISC_SEGMENTS = 32;
constexpr int CAP_SEGMENTS = 8;
constexpr int ARC_SEGMENTS = 32;

constexpr float TAU_F = 6.28318531f;
constexpr float HALF_TURN_F = 3.14159265f;

// The border is grown on the CPU by this many SCREEN pixels, so it stays a hairline at
// any zoom. `Geometry` works in local units, so the caller passes the converted value.
constexpr float OUTLINE_MIN_PX = 0.75f;

// Per-vertex custom value read by the feather shader: 0 on the grown outer edge, 1 on
// the original outline. The two boundaries are exactly one outline-width apart in
// screen space, so interpolating between them yields a linear screen distance, which
// makes the feather exact rather than a guess.
constexpr float EDGE_OUTER = 0.0f;
constexpr float EDGE_INNER = 1.0f;

// Screen-pixel size of the bone name labels; see the view's draw call.
constexpr int DEBUG_BONE_NAME_FONT_SIZE = 14;

// Width of the bone outline, in screen pixels, as specified: fixed at one or two pixels
// so it stays a hairline at any zoom. The view converts it to local units by the node's
// own scale before handing it to the geometry builder.
static constexpr float DEBUG_OUTLINE_PX = 1.6f;

// One bone, ready to be drawn. The start point and direction both come from the bone's
// composed matrix (DragonBonesBone::get_global_transform), so parent rotations are
// included.
struct DebugBone {
	enum Kind {
		KIND_PLAIN, // ordinary bone
		KIND_IK_TARGET, // the `target` of an IK constraint
		KIND_IK_DRIVEN, // the `bone` an IK constraint acts on
	};

	Vector2 start;
	Vector2 dir{ 1.0f, 0.0f }; // unit, in view space
	float length = 0.0f;
	StringName name;
	Kind kind = KIND_PLAIN;
};

// The geometry of one bone, split into two layers that become two mesh surfaces.
//
// The border layer holds the outline ring of every shape, grown outwards by the outline
// width; the body layer holds the filled shapes. Keeping them apart is what gives each
// shape a clean edge of its own: the body is painted over its own border, and no shape
// covers a neighbour's outline.
//
// `border_edges` / `body_edges` are per-vertex custom values: 0 on the grown outer edge,
// 1 on the original outline. Interpolating between two boundaries exactly one
// outline-width apart in screen space yields a linear screen distance, which is what
// lets the feather shader compute an exact edge instead of guessing.
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

// The editor draws a near-black edge around each glyph. A blur is not available on a
// plain triangle surface, so the border pass approximates it with a rim plus the
// feather in the shader.
Color rim_color(const Color &p_body) {
	return Color(0.0f, 0.0f, 0.0f, p_body.a);
}

// Emits geometry into two independent layers: border rings and filled bodies. They
// become two mesh surfaces, each with its own material, and the body is painted over
// the border - so every shape has a clean edge of its own, and no border is applied as
// a separate flat pass that would be overdrawn by its own fill.
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

	// A border quad: `p_in_a`/`p_in_b` lie on the shape (EDGE_INNER), `p_out_b`/`p_out_a`
	// on the grown outline (EDGE_OUTER).
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
		// A ring whose inner radius collapses must not be built: multiplying a direction by a
		// negative radius mirrors the point through the centre, and the resulting quad spans
		// the whole disc as a long spike. Clamping the inner radius keeps it a ring.
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

	// One straight outline segment: a quad between the edge itself and the same edge grown
	// outwards by the outline width.
	//
	// `p_outward` is the side of the line the shape's interior is NOT on. It has to be
	// passed in rather than derived from the edge's direction: the silhouette is traced as a
	// loop, so two edges of the same kite run in opposite directions and a fixed normal
	// would grow one of them inwards, leaving that side with no outline at all.
	void border_edge(const Vector2 &p_a, const Vector2 &p_b, const Vector2 &p_outward, const Color &p_color) {
		const Vector2 axis = p_b - p_a;
		if (axis.length() <= 0.0f) {
			return;
		}
		border_raw(p_a, p_b, p_b + p_outward * rim, p_a + p_outward * rim, p_color);
	}

	// The outline over an arc of the circle, from `p_from` round to `p_to` the short way
	// the sweep describes. Used to trace the part of the ring the kite does not cover, so
	// the ring and the kite are outlined as one continuous loop.
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
	const float radius = MAX(p_props.bone_pivot_radius, 0.5f);
	Geometry g(MAX(p_outline_px, OUTLINE_MIN_PX));

	const Vector2 dir = p_bone.dir;
	const Vector2 perp(-dir.y, dir.x);
	const Vector2 &center = p_bone.start;

	const bool is_ik_target = p_bone.kind == DebugBone::KIND_IK_TARGET;
	const bool is_ik_driven = p_bone.kind == DebugBone::KIND_IK_DRIVEN;

	// ------------------------------------------------------------------
	// The symbol is ONE silhouette: the pivot, plus the kite when the bone is long enough
	// for one. Both are filled first and the outline is traced once around the result, so
	// the joint between them is not cut by a line of its own.
	// ------------------------------------------------------------------
	const float diameter = radius * 2.0f;

	// A kite only exists once the bone is longer than the pivot is wide. Below that there
	// is no room for a glyph, and the symbol's own spoke is carried on to the bone's end
	// instead - the same line reaching further, not a second line drawn over the first.
	const bool has_kite = p_bone.length > diameter;
	const float spoke_to = MIN(MAX(p_bone.length, radius), radius * 3.0f);

	const Color body_color = is_ik_target ? p_props.color_ik_target : p_props.color_bone;
	// The outline takes the opacity of what it outlines, as specified: a bone drawn at 0.8
	// gets a 0.8 outline, an IK target's 0.9 gets 0.9.
	const Color outline_color = rim_color(is_ik_driven ? p_props.color_ik_bone_outline : body_color);

	// ------------------------------------------------------------------
	// Kite: from the pivot circle out to the bone's end, tapering all the way. The far end
	// sits exactly `p_bone.length` from the centre, so the glyph measures the bone; the only
	// exception is a bone so short it barely clears the wide end, where the end is pushed
	// just past it so the shape still tapers instead of ending square.
	// ------------------------------------------------------------------
	const float half = radius * KITE_HALF_WIDTH;
	const float tip_half = radius * KITE_TIP_HALF;
	const float wide_at = radius * KITE_WIDE_AT;
	const float tip_at = MAX(p_bone.length, wide_at + radius * 0.25f);

	// The far end is a short edge rather than a single point, so the taper is visible and
	// the outline has something to close against.
	const Vector2 tip_a = center + dir * tip_at + perp * tip_half;
	const Vector2 tip_b = center + dir * tip_at - perp * tip_half;

	// ------------------------------------------------------------------
	// Pivot: the ring at the bone's start, filled on the arc the kite does not cover, so
	// the kite's own fill carries that stretch and the two read as one shape.
	const float ring_w = radius * (is_ik_target ? IK_RING_W : PLAIN_RING_W);
	const float ring_inner = radius - ring_w;
	// The ring is filled and outlined over the whole circle except the stretch the kite
	// actually covers: the wedge between its two springing corners. That wedge is narrow,
	// so assuming a half turn left a visible gap in the ring on every bone with a kite.
	// These are the two angles that bound it, taken from the corners themselves.
	const Vector2 spring_a = center + dir * radius + perp * half;
	const Vector2 spring_b = center + dir * radius - perp * half;
	const float trail = Math::atan2((spring_b - center).y, (spring_b - center).x);
	const float lead = Math::atan2((spring_a - center).y, (spring_a - center).x);

	if (has_kite) {
		// The kite's wide end sits on the circle, so its corners are on the ring's outer
		// edge and it starts exactly where the ring's drawn arc ends. Convex, so a fan from
		// the wide end covers it exactly.
		g.body_tri(spring_a, tip_a, tip_b, body_color);
		g.body_tri(spring_a, tip_b, spring_b, body_color);

		// The ring, over the rest of the circle. The kite covers only the narrow wedge
		// between its two corners, so this is most of the circle.
		g.body_arc(center, radius, ring_inner, lead, trail + TAU_F, body_color);
	} else {
		g.body_annulus(center, radius, ring_inner, body_color);
	}

	if (is_ik_target) {
		// A dark, low-opacity disc inside the thick ring.
		g.body_disc(center, ring_inner, faded(body_color, IK_DISC_ALPHA));
	}

	// ------------------------------------------------------------------
	// The spoke: the line from the centre out, showing the bone's rotation. Drawn once, at
	// its final length, so a short bone is that same line reaching further.
	// ------------------------------------------------------------------
	const float spoke_w = radius * (is_ik_target ? IK_ARM_W : PLAIN_SPOKE_W);
	g.body_segment(center, center + dir * spoke_to, spoke_w, body_color);

	if (is_ik_target) {
		// The other three crosshair arms, at the ring's own thickness, stopping clear of the
		// disc so that only the spoke reaches the centre.
		const float arm_out = radius * IK_ARM_OUT;
		const float arm_in = radius + spoke_w * 0.5f;
		g.body_segment(center - perp * arm_out, center - perp * arm_in, spoke_w, body_color);
		g.body_segment(center + perp * arm_out, center + perp * arm_in, spoke_w, body_color);
		g.body_segment(center - dir * arm_out, center - dir * arm_in, spoke_w, body_color);
	}

	// ------------------------------------------------------------------
	// The outline, traced in ONE pass around the combined silhouette: the kite's two sides
	// out to its truncated end, that end, then the circle's arc from the kite's trailing
	// corner all the way round to its leading corner. The stretch the kite covers is
	// skipped - there the kite's own side is the silhouette, and the joint is interior.
	// ------------------------------------------------------------------
	if (has_kite) {
		// Each side is grown away from the kite's interior, so the outline lands on the
		// outside of the shape whichever way that side happens to run. The far end is grown
		// straight out along the bone.
		g.border_edge(spring_b, tip_b, -perp, outline_color);
		g.border_edge(tip_b, tip_a, dir, outline_color);
		g.border_edge(tip_a, spring_a, perp, outline_color);
		g.border_arc(center, radius, lead, trail + TAU_F, outline_color);
	} else {
		g.border_arc(center, radius, 0.0f, TAU_F, outline_color);
	}

	// Appended, not assigned: the caller accumulates every bone into one geometry, so
	// assigning here would leave only the last bone drawn.
	//
	// The indices are local to `g`, so they are rebased by the number of vertices that
	// already precede them - not by the number of indices, which is a different count.
	// `LocalVector` has no `append_array`, so each list is grown and copied in turn.
	auto append_all = [](auto &p_dst, const auto &p_src) {
		const uint32_t base = p_dst.size();
		p_dst.resize(base + p_src.size());
		for (uint32_t i = 0; i < p_src.size(); ++i) {
			p_dst[base + i] = p_src[i];
		}
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

	// Fixed font size: no scale compensation. The size is whatever `draw_string` is handed,
	// whatever the node scale, the 2D zoom or the window stretch.
	const int font_size = DEBUG_BONE_NAME_FONT_SIZE;
	const float label_pad = p_props.bone_pivot_radius * 2.0f + 2.0f;

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
	for (uint32_t i = 0; i < p_points.size(); ++i) {
		out[i] = p_points[i];
	}
	return out;
}

void append_bone_debug_data(DragonBonesArmature *p_armature, LocalVector<DebugBone> &r_data, const Transform2D &p_base_transform = {}) {
	const Transform2D global_transform = p_base_transform * p_armature->transform;

	// Classify the bones from the IK constraints. The JSON has no "bone type"
	// field, but each constraint names the bone it targets and the chain of bones
	// it drives.
	//
	// `chain` is already resolved when the skeleton is parsed: for a chain of N+1
	// bones, `_parseIKConstraint` sets `root` to the topmost one and `bone` to the one
	// below it (JSONDataParser.cpp), so both are driven and both get the
	// constraint's outline. Marking only `bone` would leave the rest of the chain
	// looking like ordinary bones, which defeats the point of an IK chain.
	LocalVector<StringName> ik_targets;
	LocalVector<StringName> ik_driven;
	for (const dragonBones::Constraint *constraint : p_armature->getArmature()->_constraints) {
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

	for (const auto &[name, bone] : p_armature->get_bones()) {
		if (!bone.is_valid()) {
			continue;
		}

		// 合成矩阵来自 `get_global_transform()`（内部读 `globalTransformMatrix`），
		// 再叠加外层骨架的基准变换，使嵌套骨架的坐标也落到同一空间。
		const Transform2D bone_transform = global_transform * bone->get_global_transform();
		const Vector2 start = bone_transform.get_origin();
		// 朝向取该变换的 X 轴。必须走同一个 `bone_transform`，否则嵌套骨架里
		// 外层变换不会作用到朝向上，方向与起点会分处两个空间。
		const Vector2 direction = bone_transform.columns[0].normalized();

		// 长度取自骨架数据。零长骨骼只画起点符号——长度不做猜测，编造出来的长度
		// 会和真实数据一样显示，无法区分。
		const float length = bone->get_length();

		const StringName bone_name(name);
		DebugBone::Kind kind = DebugBone::KIND_PLAIN;
		if (ik_targets.has(bone_name)) {
			kind = DebugBone::KIND_IK_TARGET;
		} else if (ik_driven.has(bone_name)) {
			kind = DebugBone::KIND_IK_DRIVEN;
		}

		r_data.push_back({
				start,
				direction,
				length,
				name,
				kind,
		});
	}

	// Recurse into nested armatures: `append_draw_data` reaches them through
	// `Slot::getDisplay()`, which returns the child armature's display.
	for (const dragonBones::Slot *raw_slot : p_armature->getArmature()->getSlots()) {
		const Slot_GD *slot = static_cast<const Slot_GD *>(raw_slot);
		if (auto display = slot->get_display()) {
			if (auto *child_armature = dynamic_cast<DragonBonesArmature *>(display)) {
				append_bone_debug_data(child_armature, r_data, global_transform);
			}
		}
	}
}

void DebugDraw::draw(DragonBonesArmature *p_root_armature, const DrawData &p_draw_data, const RID &p_debug_mesh) {
	const auto RS = RenderingServer::get_singleton();
	RS->mesh_clear(p_debug_mesh);

	const Transform2D identity{};

	// ---- Slot wireframe ----
	if (draw_mesh) {
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

		// Triangles to lines.
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

	// ---- Bones ----
	// The border ring and the filled body go into a single surface, body last, so
	// each shape covers its own outline without a second pass to flatten the stack.
	if (draw_bone || draw_bone_name) {
		LocalVector<DebugBone> bone_data;
		append_bone_debug_data(p_root_armature, bone_data, {});
		if (draw_bone) {
			if (!bone_data.is_empty()) {
				DebugDrawGeometry geometry;
				// The outline is a fixed number of screen pixels, as specified, so it stays a
				// hairline at any zoom. It is converted to local units by the node's own
				// scale, which is what the local-space geometry is measured in.
				const float draw_scale = Math::abs(owner->get_global_transform_with_canvas().get_scale().y);
				const float outline_px = draw_scale > 0.0f ? DEBUG_OUTLINE_PX / draw_scale : DEBUG_OUTLINE_PX;
				for (const DebugBone &bone : bone_data) {
					append_debug_bone_geometry(bone, *this, outline_px, geometry);
				}

				if (!geometry.body_indices.is_empty()) {
					// Body indices are shifted past the border vertices that precede them.
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

		if (draw_bone_name) {
			draw_debug_bone_names(owner, bone_data, *this);
		}
	}

	// `canvas_item_add_mesh` submits every surface of the mesh, so it is called once,
	// after all of them are in place.
	if (draw_mesh || draw_bone || draw_bone_name) {
		RS->canvas_item_add_mesh(owner->get_canvas_item(), p_debug_mesh, identity, owner->get_modulate());
	}
}

#endif // DEBUG_ENABLED

} //namespace godot