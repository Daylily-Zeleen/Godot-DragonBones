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

public:
	DragonBonesTextureData() { _onClear(); }
	virtual ~DragonBonesTextureData() override { _onClear(); }

	virtual Ref<Texture2D> get_texture() const;
	virtual Size2 get_texture_size() const;
};

class DragonBonesTextureDataScattered : public dragonBones::TextureData {
	BIND_CLASS_TYPE_B(DragonBonesTextureDataScattered);

public:
	DragonBonesTextureDataScattered() { _onClear(); }
	virtual ~DragonBonesTextureDataScattered() override { _onClear(); }

	// 散图模式：该显示对象自己的独立纹理与尺寸（图集模式为空，UV 使用图集宽高归一化）。
	Ref<Texture2D> texture;

	virtual Ref<Texture2D> get_texture() const;
	virtual Size2 get_texture_size() const;

	virtual void _onClear() override {
		dragonBones::TextureData::_onClear();
		texture.unref();
	}
};

class DragonBonesTextureAtlasData : public dragonBones::TextureAtlasData {
	BIND_CLASS_TYPE_B(DragonBonesTextureAtlasData);

private:
	Ref<Texture2D> texture;

public:
	DragonBonesTextureAtlasData() { _onClear(); }
	virtual ~DragonBonesTextureAtlasData() override { _onClear(); }

	virtual dragonBones::TextureData *createTexture() const override {
		return BaseObject::borrowObject<DragonBonesTextureData>();
	}

	void init(const Ref<Texture2D> &p_texture) { texture = p_texture; }
	const Ref<Texture2D> &get_texture() const { return texture; }

	virtual void _onClear() override {
		dragonBones::TextureAtlasData::_onClear();
		texture.unref();
	}
};

} //namespace godot