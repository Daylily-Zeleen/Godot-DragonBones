#pragma once

#include "dragonBones/model/TextureAtlasData.h"

#include "godot_cpp/core/math.hpp"
#include "godot_cpp/variant/string.hpp"

namespace godot {
class GDTextureData : public dragonBones::TextureData {
	BIND_CLASS_TYPE_B(GDTextureData);

private:
	static bool is_rect_equal(const dragonBones::Rectangle &p_a, const dragonBones::Rectangle &p_b) {
		return Math::is_equal_approx(p_a.x, p_b.x) &&
				Math::is_equal_approx(p_a.y, p_b.y) &&
				Math::is_equal_approx(p_a.width, p_b.width) &&
				Math::is_equal_approx(p_a.height, p_b.height);
	}

public:
	GDTextureData() { _onClear(); }
	virtual ~GDTextureData() { _onClear(); }

	bool operator!=(const GDTextureData &p_other) const {
		return !operator==(p_other);
	}

	bool operator==(const GDTextureData &p_other) const {
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

class GDTextureAtlasData : public dragonBones::TextureAtlasData {
	BIND_CLASS_TYPE_B(GDTextureAtlasData);

private:
	String image_file_path;

public:
	GDTextureAtlasData() { _onClear(); }
	virtual ~GDTextureAtlasData() { _onClear(); }

	virtual dragonBones::TextureData *createTexture() const override {
		return BaseObject::borrowObject<GDTextureData>();
	}

	void init(const String &p_image_file_path) { image_file_path = p_image_file_path; }
	const String &get_image_file_path() const { return image_file_path; }
	virtual void _onClear() override {
		dragonBones::TextureAtlasData::_onClear();
		image_file_path = "";
	}

	void setRenderTexture() {
		// for (const auto &pair : textures) {
		// 	const auto textureData = static_cast<GDTextureData *>(pair.second);
		// 	Rect2i rect(textureData->region.x, textureData->region.y,
		// 			textureData->rotated ? textureData->region.height : textureData->region.width,
		// 			textureData->rotated ? textureData->region.width : textureData->region.height);
		// 	textureData->textureRect = std::move(rect);
		// }
	}

	bool operator!=(const GDTextureAtlasData &p_other) const { return !operator==(p_other); }
	bool operator==(const GDTextureAtlasData &p_other) const {
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

			const auto texture = static_cast<GDTextureData *>(kv.second);
			const auto other_texture = static_cast<GDTextureData *>(it->second);

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