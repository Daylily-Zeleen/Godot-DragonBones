/**************************************************************************/
/*  packed_offset_check.cpp                                               */
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

#ifdef DEBUG_ENABLED
#include "packed_offset_check.h"

#include <rapidjson/document.h>

#include <string>

namespace godot {

namespace {

using JsonValue = rapidjson::Value;

// 某缓冲上偏移的峰值及触发它的槽位名。
struct Peak {
	int64_t value = 0;
	std::string where;

	void record(int64_t p_value, const std::string &p_where) {
		if (p_value > value) {
			value = p_value;
			where = p_where;
		}
	}
};

std::string get_string(const JsonValue &p_obj, const char *p_key, const char *p_default = "") {
	auto it = p_obj.FindMember(p_key);
	if (it == p_obj.MemberEnd() || !it->value.IsString()) {
		return std::string(p_default);
	}
	return std::string(it->value.GetString(), it->value.GetStringLength());
}

std::size_t member_array_size(const JsonValue &p_obj, const char *p_key) {
	auto it = p_obj.FindMember(p_key);
	if (it == p_obj.MemberEnd() || !it->value.IsArray()) {
		return 0;
	}
	return it->value.Size();
}

bool has_member(const JsonValue &p_obj, const char *p_key) {
	return p_obj.FindMember(p_key) != p_obj.MemberEnd();
}

// 同 runtime 的 _getDisplayType：字符串（忽略大小写）或数字，默认 image。
bool is_mesh_display(const JsonValue &p_display) {
	auto it = p_display.FindMember("type");
	if (it == p_display.MemberEnd()) {
		return false; // 无 type -> image。
	}
	if (it->value.IsString()) {
		std::string lower(it->value.GetString(), it->value.GetStringLength());
		for (auto &c : lower) {
			c = (char)::tolower((unsigned char)c);
		}
		return lower == "mesh";
	}
	if (it->value.IsNumber()) {
		return (int)it->value.GetDouble() == 2; // DisplayType::Mesh
	}
	return false;
}

// 是否带显式颜色（决定新增 8 项颜色块，还是复用默认块）。
bool has_explicit_color(const JsonValue &p_frame) {
	auto it = p_frame.FindMember("value");
	if (it == p_frame.MemberEnd()) {
		it = p_frame.FindMember("color");
	}
	if (it == p_frame.MemberEnd() || !it->value.IsObject()) {
		return false;
	}
	static const char *kKeys[] = { "aM", "rM", "gM", "bM", "aO", "rO", "gO", "bO" };
	for (const char *key : kKeys) {
		if (has_member(it->value, key)) {
			return true;
		}
	}
	return false;
}

// 复算偏移，镜像 JSONDataParser 的缓冲增长顺序。
// 覆盖 floatArray（mesh/weight 偏移）与 intArray（颜色块位置）两类。
// 二者都是解析期 `.size()` 快照，按遍历顺序累加，故顺序必须与解析器一致：
// 每个 armature 内先 skin，再 animation。
struct Replay {
	int64_t int_array = 0;
	int64_t float_array = 0;

	Peak float_peak; // floatArray 上的偏移（mesh / weight）
	Peak int_peak; // intArray 上的偏移（颜色块）

	// 偏移记的是**增长前**的缓冲大小，故先记录再增长。
	void parse_mesh(const JsonValue &p_display, const std::string &p_slot_name) {
		const int64_t vertex_count = (int64_t)member_array_size(p_display, "vertices") / 2;
		const int64_t triangle_count = (int64_t)member_array_size(p_display, "triangles") / 3;

		float_peak.record(float_array, p_slot_name);
		int_array += 4 + triangle_count * 3;
		float_array += vertex_count * 2 + vertex_count * 2;

		if (!has_member(p_display, "weights")) {
			return;
		}

		float_peak.record(float_array, p_slot_name);

		const int64_t weight_bone_count = (int64_t)member_array_size(p_display, "bonePose") / 7;
		const int64_t weight_count = ((int64_t)member_array_size(p_display, "weights") - vertex_count) / 2;

		int_array += 2 + weight_bone_count + vertex_count + weight_count;
		float_array += weight_count * 3;
	}

	String overflowed() const {
		std::string summary;
		append(summary, "floatArray", float_peak);
		append(summary, "intArray", int_peak);
		return String(summary.c_str());
	}

	static void append(std::string &r_out, const char *p_name, const Peak &p_peak) {
		if (p_peak.value <= PACKED_OFFSET_LIMIT) {
			return;
		}
		if (!r_out.empty()) {
			r_out += "; ";
		}
		r_out += p_name;
		r_out += " peak ";
		r_out += std::to_string(p_peak.value);
		r_out += " (limit ";
		r_out += std::to_string(PACKED_OFFSET_LIMIT);
		r_out += ") at \"";
		r_out += p_peak.where;
		r_out += "\"";
	}
};

// 单个 armature：先全部 skin，再全部 animation（与 _parseArmature 一致）。
void replay_armature(const JsonValue &p_armature, Replay &r_replay) {
	auto skins = p_armature.FindMember("skin");
	if (skins != p_armature.MemberEnd() && skins->value.IsArray()) {
		for (auto &skin : skins->value.GetArray()) {
			auto slots = skin.FindMember("slot");
			if (slots == skin.MemberEnd() || !slots->value.IsArray()) {
				continue;
			}
			for (auto &slot : slots->value.GetArray()) {
				const std::string slot_name = get_string(slot, "name");
				auto displays = slot.FindMember("display");
				if (displays == slot.MemberEnd() || !displays->value.IsArray()) {
					continue;
				}
				for (auto &display : displays->value.GetArray()) {
					if (!display.IsObject()) {
						continue;
					}
					// 带 share 的 mesh 不推进缓冲（复用被共享者的偏移）。
					if (!is_mesh_display(display) || has_member(display, "share")) {
						continue;
					}
					r_replay.parse_mesh(display, slot_name);
				}
			}
		}
	}

	// 每处显式颜色新建 8 项；无显式值时复用默认块，不占空间。
	auto animations = p_armature.FindMember("animation");
	if (animations == p_armature.MemberEnd() || !animations->value.IsArray()) {
		return;
	}
	for (auto &animation : animations->value.GetArray()) {
		auto slots = animation.FindMember("slot");
		if (slots == animation.MemberEnd() || !slots->value.IsArray()) {
			continue;
		}
		for (auto &slot : slots->value.GetArray()) {
			const std::string slot_name = get_string(slot, "name");
			auto frames = slot.FindMember("colorFrame");
			if (frames == slot.MemberEnd() || !frames->value.IsArray()) {
				continue;
			}
			for (auto &frame : frames->value.GetArray()) {
				if (!frame.IsObject() || !has_explicit_color(frame)) {
					continue;
				}
				r_replay.int_peak.record(r_replay.int_array, slot_name);
				r_replay.int_array += 8;
			}
		}
	}
}

} //namespace

String check_packed_offsets(const char *p_ske_content) {
	if (p_ske_content == nullptr) {
		return String();
	}

	rapidjson::Document doc;
	doc.Parse(p_ske_content);
	// 非 JSON（如 .dbbin）或 JSON 非法：交由既有解析路径报错。
	if (doc.HasParseError() || !doc.IsObject()) {
		return String();
	}

	auto armatures = doc.FindMember("armature");
	if (armatures == doc.MemberEnd() || !armatures->value.IsArray()) {
		return String();
	}

	Replay replay;
	for (auto &armature : armatures->value.GetArray()) {
		if (armature.IsObject()) {
			replay_armature(armature, replay);
		}
	}

	return replay.overflowed();
}

} //namespace godot

#endif // DEBUG_ENABLED
