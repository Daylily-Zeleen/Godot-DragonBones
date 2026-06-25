/**************************************************************************/
/*  texture_atlas_data.h                                                  */
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

#include <godot_dragon_bones.h>

#include <dragonBones/model/TextureAtlasData.h>

#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/core/math.hpp>

namespace godot {
class DragonBonesTextureData : public dragonBones::TextureData {
	BIND_CLASS_TYPE_B(DragonBonesTextureData);

private:
	// TODO: 考虑移除
	static bool is_rect_equal(const dragonBones::Rectangle &p_a, const dragonBones::Rectangle &p_b) {
		return Math::is_equal_approx(p_a.x, p_b.x) &&
				Math::is_equal_approx(p_a.y, p_b.y) &&
				Math::is_equal_approx(p_a.width, p_b.width) &&
				Math::is_equal_approx(p_a.height, p_b.height);
	}

public:
	DragonBonesTextureData() { _onClear(); }
	virtual ~DragonBonesTextureData() override { _onClear(); }

	// TODO: 考虑移除以下成员
	bool operator!=(const DragonBonesTextureData &p_other) const {
		return !operator==(p_other);
	}

	bool operator==(const DragonBonesTextureData &p_other) const {
		if ((frame && !p_other.frame) || (!frame && p_other.frame)) {
			return false;
		}

		if (rotated != p_other.rotated) {
			return false;
		}

		if (frame && p_other.frame &&
				!is_rect_equal(*frame, *p_other.frame)) {
			return false;
		}

		if (!is_rect_equal(region, p_other.region)) {
			return false;
		}

		return name == p_other.name;
	}
};

class DragonBonesTextureAtlasData : public dragonBones::TextureAtlasData {
	BIND_CLASS_TYPE_B(DragonBonesTextureAtlasData);

private:
	Ref<Texture2D> display_texture;

public:
	DragonBonesTextureAtlasData() { _onClear(); }
	virtual ~DragonBonesTextureAtlasData() override { _onClear(); }

	virtual dragonBones::TextureData *createTexture() const override {
		return BaseObject::borrowObject<DragonBonesTextureData>();
	}

	void init(const Ref<Texture2D> &p_texture) { display_texture = p_texture; }
	const Ref<Texture2D> &get_display_texture() const { return display_texture; }

	virtual void _onClear() override {
		dragonBones::TextureAtlasData::_onClear();
		display_texture.unref();
	}

	// TODO: 考虑移除以下成员
	bool operator!=(const DragonBonesTextureAtlasData &p_other) const { return !operator==(p_other); }
	bool operator==(const DragonBonesTextureAtlasData &p_other) const {
		if (autoSearch != p_other.autoSearch ||
				format != p_other.format ||
				width != p_other.width ||
				height != p_other.height ||
				scale != p_other.scale ||
				name != p_other.name ||
				imagePath != p_other.imagePath ||
				textures.size() != p_other.textures.size()) {
			return false;
		}

		for (const auto &kv : textures) {
			const auto &texture_name = kv.first;

			const auto it = p_other.textures.find(texture_name);
			if (it == p_other.textures.end()) {
				return false;
			}

			const auto texture = static_cast<DragonBonesTextureData *>(kv.second);
			const auto other_texture = static_cast<DragonBonesTextureData *>(it->second);

			if (!texture && !other_texture) {
				continue;
			}
			if ((!texture && other_texture) || (texture && !other_texture) || (*texture != *other_texture)) {
				return false;
			}
		}

		return true;
	}
};

} //namespace godot