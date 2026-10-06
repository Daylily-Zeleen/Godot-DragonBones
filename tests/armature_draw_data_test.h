/**************************************************************************/
/*  armature_draw_data_test.h                                             */
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

#pragma once

#ifndef DOCTEST_CONFIG_NO_POSIX_SIGNALS
#define DOCTEST_CONFIG_NO_POSIX_SIGNALS
#endif
#ifndef DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#endif

#include <doctest/doctest.h>

#include <armature_draw_data.h>

namespace {

using godot::ArmatureDrawData;
using godot::LocalVector;
using godot::Transform2D;
using godot::Vector2;

// get_capacity_bytes 应等于「各缓冲的真实字节数」之和，量纲为字节。
// D2 回归：修复前对 layer.get_capacity_bytes()（已是字节）又乘了一次 sizeof(Data)，
// 在有一条命令时会让结果放大 sizeof(Data) 倍。
TEST_CASE("ArmatureDrawData: get_capacity_bytes counts real bytes (D2)") {
	ArmatureDrawData dd;
	CHECK(dd.get_capacity_bytes() == 0);

	LocalVector<Vector2> vertices;
	LocalVector<int32_t> indices;
	LocalVector<godot::Color> colors;
	LocalVector<Vector2> uvs;
	vertices.push_back(Vector2(0, 0));
	vertices.push_back(Vector2(1, 1));
	indices.push_back(0);
	indices.push_back(1);
	indices.push_back(0);
	colors.push_back(godot::Color(1, 1, 1, 1));
	colors.push_back(godot::Color(1, 1, 1, 1));
	uvs.push_back(Vector2(0, 0));
	uvs.push_back(Vector2(1, 1));

	dd.begin_frame();
	dd.add_data(0, Transform2D(), &vertices, &indices, &colors, &uvs, 0, godot::CanvasItemMaterial::BLEND_MODE_MIX
#ifdef DEBUG_ENABLED
				,
				godot::Color()
#endif // DEBUG_ENABLED
	);

	const size_t cap = dd.get_capacity_bytes();
	// 必须包含 layers 缓冲 + 该 layer 的 data 缓冲，且量纲合理（字节）。
	CHECK(cap >= sizeof(ArmatureDrawData::Data));
	// D2 回归：修复前会把 layer 字节数再乘 sizeof(Data)，结果 ≥ sizeof(Data)^2。
	// 断言真实值不会达到该量级（Data 含 4 个指针 + 变换等，sizeof 至少 48 字节）。
	CHECK(cap < sizeof(ArmatureDrawData::Data) * sizeof(ArmatureDrawData::Data));
}

} //namespace
