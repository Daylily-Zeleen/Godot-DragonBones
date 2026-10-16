/**************************************************************************/
/*  packed_offset_check_test.h                                            */
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
/* "Software"), to deal in the Software without restriction, including     */
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

#include <packed_offset_check.h>

#include <string>

namespace {

using godot::check_packed_offsets;

// 造一份骨架 JSON。每个 mesh 带 vertexCount 个顶点，偏移按解析顺序累加，
// 故「前面的部件越大，后面的偏移越靠后」—— 这正是越限的成因。
// colourFrameCount 个带显式颜色的帧会让 intArray 每帧多 8 项。
std::string make_skeleton(int mesh_count, int vertex_count, int color_frame_count = 0) {
	std::string vertices = "[";
	for (int i = 0; i < vertex_count * 2; ++i) {
		vertices += (i == 0 ? "0" : ",0");
	}
	vertices += "]";

	std::string displays;
	for (int i = 0; i < mesh_count; ++i) {
		if (i != 0) {
			displays += ",";
		}
		displays += "{\"type\":\"mesh\",\"name\":\"m" + std::to_string(i) +
				"\",\"width\":1,\"height\":1,\"vertices\":" + vertices +
				",\"uvs\":" + vertices + ",\"triangles\":[0,1,2,0,2,3]}";
	}

	std::string color_frames;
	for (int i = 0; i < color_frame_count; ++i) {
		if (i != 0) {
			color_frames += ",";
		}
		color_frames += "{\"duration\":1,\"value\":{\"aM\":50}}";
	}

	std::string out =
			"{\"version\":\"5.5\",\"compatibleVersion\":\"5.5\",\"name\":\"t\",\"frameRate\":30,"
			"\"armature\":[{\"type\":\"Armature\",\"frameRate\":30,\"name\":\"a\","
			"\"bone\":[{\"name\":\"root\"}],\"slot\":[{\"name\":\"s0\",\"parent\":\"root\"}],"
			"\"skin\":[{\"slot\":[{\"name\":\"s0\",\"display\":[" + displays + "]}]}],"
			"\"animation\":[";
	if (color_frame_count > 0) {
		out += "{\"duration\":1,\"name\":\"anim\",\"slot\":[{\"name\":\"s0\",\"colorFrame\":[" +
				color_frames + "]}]}";
	}
	out += "]}]}";
	return out;
}

// 越限判据：单个 mesh 让 floatArray 总量超 65535，但若它自己的 MeshFloatOffset 是 0，
// 窄模式下并不会读错 —— 误报会让合法资源无法加载，故必须按峰值而非总量判定。
TEST_CASE("check_packed_offsets: silent when every offset is inside the limit") {
	const std::string json = make_skeleton(4, 8);
	CHECK(check_packed_offsets(json.c_str()).is_empty());
}

TEST_CASE("check_packed_offsets: a big single mesh does not trip the check") {
	// 单 mesh 40000 顶点 → floatArray 总量 160000（远超 65535），
	// 但它自己的偏移是 0，窄模式下不会读错。
	const std::string json = make_skeleton(1, 40000);
	CHECK(check_packed_offsets(json.c_str()).is_empty());
}

TEST_CASE("check_packed_offsets: reports the overflowing buffer with its peak and slot") {
	// 首 mesh 40000 顶点 → 其后每个 mesh 的偏移推进 160000。
	const std::string json = make_skeleton(2, 40000);
	const godot::String summary = check_packed_offsets(json.c_str());
	REQUIRE(!summary.is_empty());
	// 报错必须含缓冲名、真实峰值与定位名，否则用户无法判断该改哪个部件。
	CHECK(summary.find("floatArray") >= 0);
	CHECK(summary.find("160000") >= 0);
	CHECK(summary.find("65535") >= 0);
	CHECK(summary.find("s0") >= 0);
}

// 颜色块（intArray）与 mesh 偏移（floatArray）是两处独立缺陷，必须都能报出。
// intArray 的增长由**三角形数**驱动（`4 + 3*triangleCount`/mesh），故这里构造
// 大三角形数的网格：每个 mesh 用 8000 三角形 → intArray 每 mesh 涨 24004，
// 3 个 mesh 后 intArray 达 72012；颜色块偏移记的是写入前的位置。
TEST_CASE("check_packed_offsets: covers intArray colour offsets independently") {
	// 三角形索引是扁平数组，n 个三角形需 3n 个索引。
	std::string triangles = "[";
	for (int i = 0; i < 8000 * 3; ++i) {
		triangles += (i == 0 ? "0" : ",0");
	}
	triangles += "]";

	const std::string mesh = "{\"type\":\"mesh\",\"name\":\"m\",\"width\":1,\"height\":1,"
							 "\"vertices\":[0,0,0,0,0,0],\"uvs\":[0,0,0,0,0,0],"
							 "\"triangles\":" + triangles + "}";

	// 3 个 mesh（intArray 涨到 72012，越限）+ 1 个显式颜色帧（记录该位置）。
	const std::string json =
			"{\"version\":\"5.5\",\"compatibleVersion\":\"5.5\",\"name\":\"t\",\"frameRate\":30,"
			"\"armature\":[{\"type\":\"Armature\",\"frameRate\":30,\"name\":\"a\","
			"\"bone\":[{\"name\":\"root\"}],\"slot\":[{\"name\":\"s0\",\"parent\":\"root\"}],"
			"\"skin\":[{\"slot\":[{\"name\":\"s0\",\"display\":[" + mesh + "," + mesh + "," + mesh + "]}]}],"
			"\"animation\":[{\"duration\":1,\"name\":\"anim\",\"slot\":[{\"name\":\"s0\","
			"\"colorFrame\":[{\"duration\":1,\"value\":{\"aM\":50}}]}]}]}]}";

	const godot::String summary = check_packed_offsets(json.c_str());
	REQUIRE(!summary.is_empty());
	// 越限的是 intArray（颜色块位置 72012），且必须给出缓冲名与定位名。
	CHECK(summary.find("intArray") >= 0);
	CHECK(summary.find("72012") >= 0);
	CHECK(summary.find("s0") >= 0);
}

// 边界：恰好等于上限合法（65535 存得进 16 位无符号），超出 1 即越限。
TEST_CASE("check_packed_offsets: the limit itself is legal") {
	// 每个 mesh 推进 4*vertexCount 项；用 4096 顶点 → 每 mesh 16384 项。
	// 3 个 mesh 时最后偏移 = 2*16384 = 32768（在上限内，但已在 32767 之上，
	// 窄模式下靠回绕补偿仍能正确读出 —— 故不得报错）。
	const std::string inside = make_skeleton(3, 4096);
	CHECK(check_packed_offsets(inside.c_str()).is_empty());
}

// 非 JSON 输入（例如 .dbbin 二进制）不能崩，也不能误报 ——
// 那类失败交由既有解析路径报错。
TEST_CASE("check_packed_offsets: non-JSON input is not reported") {
	CHECK(check_packed_offsets("").is_empty());
	CHECK(check_packed_offsets("DBDT\x01\x02\x03\x04").is_empty());
	CHECK(check_packed_offsets("{ not json").is_empty());
	// 合法 JSON 但没有 armature：无可检查内容。
	CHECK(check_packed_offsets("{\"version\":\"5.5\"}").is_empty());
}

} //namespace
