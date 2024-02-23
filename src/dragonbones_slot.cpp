#include "dragonbones_slot.h"

#include "dragonbones_armature.h"

#include "wrappers/GDDisplay.h"
#include "wrappers/GDMesh.h"
#include "wrappers/GDTextureAtlasData.h"

#include "godot_cpp/classes/resource_loader.hpp"

using namespace godot;
using namespace dragonBones;

Ref<Texture2D> Slot_GD::get_display_texture() const {
	if (!_textureData) {
		return {};
	}
	auto atlas = static_cast<GDTextureAtlasData *>(_textureData->parent);
	ERR_FAIL_NULL_V(atlas, {});
	return ResourceLoader::get_singleton()->load(atlas->get_image_file_path());
}

void Slot_GD::update_display_texutre() const {
	if (_renderDisplay) {
		_renderDisplay->texture = get_display_texture();
	}
}

void Slot_GD::_updateZOrder() {
	_renderDisplay->set_z_index(_zOrder);
}

void Slot_GD::_updateVisible() {
	if (_parent->getVisible())
		_renderDisplay->show();
	else
		_renderDisplay->hide();
}

void Slot_GD::_updateBlendMode() {
	if (_renderDisplay) {
		CanvasItemMaterial::BlendMode __blend = CanvasItemMaterial::BLEND_MODE_MIX;

		switch (_blendMode) {
			case BlendMode::Normal:
				__blend = CanvasItemMaterial::BLEND_MODE_MIX;
				break;

			case BlendMode::Add:
				__blend = CanvasItemMaterial::BLEND_MODE_ADD;
				break;

			case BlendMode::Multiply:
				__blend = CanvasItemMaterial::BLEND_MODE_MUL;
				break;

			case BlendMode::Subtract:
				__blend = CanvasItemMaterial::BLEND_MODE_SUB;
				break;

			default:
				break;
		}
		_renderDisplay->set_blend_mode(__blend);
		_renderDisplay->queue_redraw();
	} else if (_childArmature) {
		for (const auto slot : _childArmature->getSlots()) {
			slot->_blendMode = _blendMode;
			slot->_updateBlendMode();
		}
	}
}

void Slot_GD::_updateColor() {
	if (!_renderDisplay)
		return;

	Color __color(_colorTransform.redMultiplier,
			_colorTransform.greenMultiplier,
			_colorTransform.blueMultiplier,
			_colorTransform.alphaMultiplier);

	GDOwnerNode *__p_owner = _renderDisplay->p_owner;
	if (__p_owner) {
		__color.a *= __p_owner->get_modulate().a;
		__color.r *= __p_owner->get_modulate().r;
		__color.g *= __p_owner->get_modulate().g;
		__color.b *= __p_owner->get_modulate().b;
	}

	_renderDisplay->update_modulate(__color);
	_renderDisplay->queue_redraw();
}

void Slot_GD::_initDisplay(void *value, bool isRetain) {
	update_display_texutre();
}

void Slot_GD::_disposeDisplay(void *value, bool isRelease) {
}

void Slot_GD::_onUpdateDisplay() {
	_renderDisplay = static_cast<GDDisplay *>(_display != nullptr ? _display : _rawDisplay);
	update_display_texutre();
}

void Slot_GD::_addDisplay() {
	update_display_texutre();
}

void Slot_GD::_replaceDisplay(void *value, bool isArmatureDisplay) {
	static_cast<GDDisplay *>(_renderDisplay)->show();

	if (value != nullptr) {
		static_cast<GDDisplay *>(value)->hide();
	}
}

void Slot_GD::_removeDisplay() {
}

void Slot_GD::__get_uv_pt(Point2 &_pt, bool _is_rot, float _u, float _v, const Rectangle &_reg, const TextureAtlasData *_p_atlas) {
	Size2 tex_size(_p_atlas->width, _p_atlas->height);
	if (_renderDisplay && _renderDisplay->texture.is_valid()) {
		// 以实际纹理为准
		tex_size = _renderDisplay->texture->get_size();
	}

	if (_is_rot) {
		_pt.x = (_reg.x + (1.f - _v) * _reg.width) / tex_size.x;
		_pt.y = (_reg.y + _u * _reg.height) / tex_size.y;
	} else {
		_pt.x = (_reg.x + _u * _reg.width) / tex_size.x;
		_pt.y = (_reg.y + _v * _reg.height) / tex_size.y;
	}
}

void Slot_GD::_updateFrame() {
	const auto currentVerticesData = (_deformVertices != nullptr && _display == _meshDisplay) ? _deformVertices->verticesData : nullptr;
	auto currentTextureData = static_cast<GDTextureData *>(_textureData);

	if (_displayIndex >= 0 && _display != nullptr && currentTextureData != nullptr) {
		const auto atlas = currentTextureData->getParent();
		const auto &region = currentTextureData->region;
		auto frameDisplay = static_cast<GDMesh *>(_renderDisplay);

		if (currentVerticesData != nullptr) {
			// Mesh.
			const auto &deformVertices = _deformVertices->vertices;
			const auto hasFFD = !deformVertices.empty();

			const auto data = currentVerticesData->data;
			const auto intArray = data->intArray;
			const auto floatArray = data->floatArray;
			const unsigned vertexCount = intArray[currentVerticesData->offset + (unsigned)BinaryOffset::MeshVertexCount];
			const unsigned triangleCount = intArray[currentVerticesData->offset + (unsigned)BinaryOffset::MeshTriangleCount];
			int vertexOffset = intArray[currentVerticesData->offset + (unsigned)BinaryOffset::MeshFloatOffset];

			if (vertexOffset < 0) {
				vertexOffset += 65536; // Fixed out of bouds bug.
			}

			const unsigned uvOffset = vertexOffset + (vertexCount << 1);

			frameDisplay->indices.resize(triangleCount * 3);
			frameDisplay->verticesColor.resize(vertexCount);
			frameDisplay->verticesUV.resize(vertexCount);
			frameDisplay->verticesPos.resize(vertexCount);
			auto indices_ptr = frameDisplay->indices.ptrw();
			auto verticesColor_ptr = frameDisplay->verticesColor.ptrw();
			auto verticesUV_ptr = frameDisplay->verticesUV.ptrw();
			auto verticesPos_ptr = frameDisplay->verticesPos.ptrw();

			Point2 __uv;
			std::size_t iH;
			float u, v;
			for (std::size_t i = 0, l = (vertexCount << 1); i < l; i += 2) {
				iH = i >> 1;
				u = floatArray[uvOffset + i];
				v = floatArray[uvOffset + i + 1];
				__get_uv_pt(__uv, currentTextureData->rotated, u, v, region, atlas);

				verticesColor_ptr[iH] = Color(1, 1, 1, 1);
				verticesUV_ptr[iH] = __uv;
				verticesPos_ptr[iH] = Point2(floatArray[vertexOffset + i],
						hasFFD * floatArray[vertexOffset + i + 1]);
			}

			// setup indicies
			for (std::size_t i = 0; i < triangleCount * 3; ++i) {
				indices_ptr[i] = intArray[currentVerticesData->offset + (unsigned)BinaryOffset::MeshVertexIndices + i];
			}

			_textureScale = 1.0f;
			_identityTransform();
		} else {
			// Normal texture
			frameDisplay->indices.resize(6);
			frameDisplay->verticesColor.resize(4);
			frameDisplay->verticesUV.resize(4);
			frameDisplay->verticesPos.resize(4);
			auto indices_ptr = frameDisplay->indices.ptrw();
			auto verticesColor_ptr = frameDisplay->verticesColor.ptrw();
			auto verticesUV_ptr = frameDisplay->verticesUV.ptrw();
			auto verticesPos_ptr = frameDisplay->verticesPos.ptrw();

			indices_ptr[0] = 0;
			indices_ptr[1] = 1;
			indices_ptr[2] = 2;
			indices_ptr[3] = 2;
			indices_ptr[4] = 3;
			indices_ptr[5] = 0;

			const auto scale = currentTextureData->parent->scale * _armature->_armatureData->scale;
			const auto height = (currentTextureData->rotated ? region.width : region.height) * scale / 2.f;
			const auto width = (currentTextureData->rotated ? region.height : region.width) * scale / 2.f;

			verticesColor_ptr[0] = Color(1, 1, 1, 1);
			verticesColor_ptr[1] = Color(1, 1, 1, 1);
			verticesColor_ptr[2] = Color(1, 1, 1, 1);
			verticesColor_ptr[3] = Color(1, 1, 1, 1);

			verticesPos_ptr[3] = Vector2(-width, -height);
			verticesPos_ptr[2] = Vector2(width, -height);
			verticesPos_ptr[1] = Vector2(width, height);
			verticesPos_ptr[0] = Vector2(-width, height);

			__get_uv_pt(verticesUV_ptr[0], currentTextureData->rotated, 0, 0, region, atlas);
			__get_uv_pt(verticesUV_ptr[1], currentTextureData->rotated, 1.f, 0, region, atlas);
			__get_uv_pt(verticesUV_ptr[2], currentTextureData->rotated, 1.f, 1.f, region, atlas);
			__get_uv_pt(verticesUV_ptr[3], currentTextureData->rotated, 0, 1.f, region, atlas);

			_pivotY = 0;
			_pivotX = 0;
			_textureScale = scale;
			_identityTransform();
		}

		_visibleDirty = true;
		_blendModeDirty = true;
		_colorDirty = true;
		_renderDisplay->queue_redraw();
		return;
	}
	_renderDisplay->hide();
}

void Slot_GD::_updateMesh() {
	const auto scale = _armature->_armatureData->scale;
	const auto textureData = static_cast<GDTextureData *>(_textureData);
	const auto &deformVertices = _deformVertices->vertices;
	const auto hasFFD = !deformVertices.empty();
	const auto &bones = _deformVertices->bones;
	const auto verticesData = _deformVertices->verticesData;
	const auto weightData = verticesData->weight;
	const auto meshDisplay = static_cast<GDMesh *>(_renderDisplay);

	if (!textureData) {
		return;
	}

	if (!meshDisplay->indices.size()) {
		_armature->invalidUpdate("", true);
		return;
	}

	if (weightData != nullptr) {
		const auto data = verticesData->data;
		const auto intArray = data->intArray;
		const auto floatArray = data->floatArray;
		const auto vertexCount = (std::size_t)intArray[verticesData->offset + (unsigned)BinaryOffset::MeshVertexCount];
		int weightFloatOffset = intArray[weightData->offset + (unsigned)BinaryOffset::WeigthFloatOffset];

		if (weightFloatOffset < 0) {
			weightFloatOffset += 65536;
		}

		auto verticesPos_ptr = meshDisplay->verticesPos.ptrw();
		for (
				std::size_t i = 0, iD = 0, iB = weightData->offset + (unsigned)BinaryOffset::WeigthBoneIndices + weightData->bones.size(), iV = (std::size_t)weightFloatOffset, iF = 0;
				i < vertexCount;
				++i) {
			const auto boneCount = (std::size_t)intArray[iB++];
			auto xG = 0.0f, yG = 0.0f;
			for (std::size_t j = 0; j < boneCount; ++j) {
				const auto boneIndex = (unsigned)intArray[iB++];
				const auto bone = bones[boneIndex];
				if (bone != nullptr) {
					const auto &matrix = bone->globalTransformMatrix;
					const auto weight = floatArray[iV++];
					auto xL = floatArray[iV++] * scale;
					auto yL = floatArray[iV++] * scale;

					if (hasFFD) {
						xL += deformVertices[iF++];
						yL += deformVertices[iF++];
					}

					xG += (matrix.a * xL + matrix.c * yL + matrix.tx) * weight;
					yG += (matrix.b * xL + matrix.d * yL + matrix.ty) * weight;
				}
			}

			meshDisplay->verticesPos[i] = Vector2(xG, yG);
		}
	} else if (hasFFD) {
		const auto data = verticesData->data;
		const auto intArray = data->intArray;
		const auto floatArray = data->floatArray;
		const auto vertexCount = (std::size_t)intArray[verticesData->offset + (unsigned)BinaryOffset::MeshVertexCount];
		int vertexOffset = (std::size_t)intArray[verticesData->offset + (unsigned)BinaryOffset::MeshFloatOffset];

		if (vertexOffset < 0) {
			vertexOffset += 65536;
		}

		auto verticesPos_ptr = meshDisplay->verticesPos.ptrw();
		for (std::size_t i = 0, l = (vertexCount << 1); i < l; i += 2) {
			const auto iH = (i >> 1);
			const auto xG = floatArray[vertexOffset + i] * scale + deformVertices[i];
			const auto yG = floatArray[vertexOffset + i + 1] * scale + deformVertices[i + 1];

			verticesPos_ptr[iH] = Vector2(xG, -yG);
		}
	}

	_renderDisplay->queue_redraw();
}

void Slot_GD::_identityTransform() {
	Transform2D matrix;
	matrix.scale({ _textureScale, _textureScale });
	_renderDisplay->set_transform(matrix);
	_renderDisplay->queue_redraw();
}

void Slot_GD::_updateTransform() {
	Vector2 pos = Vector2(0, 0);
	if (((void *)_renderDisplay) == _rawDisplay || ((void *)_renderDisplay) == _meshDisplay) {
		pos.x = globalTransformMatrix.tx - (globalTransformMatrix.a * _pivotX + globalTransformMatrix.c * _pivotY);
		pos.y = globalTransformMatrix.ty - (globalTransformMatrix.b * _pivotX + globalTransformMatrix.d * _pivotY);
	} else if (_childArmature) {
		pos.x = globalTransformMatrix.tx;
		pos.y = globalTransformMatrix.ty;
	} else {
		Vector2 anchorPoint(1.f, 1.f);

		pos.x = globalTransformMatrix.tx - (globalTransformMatrix.a * anchorPoint.x - globalTransformMatrix.c * anchorPoint.y);
		pos.y = globalTransformMatrix.ty - (globalTransformMatrix.b * anchorPoint.x - globalTransformMatrix.d * anchorPoint.y);
	}

	Transform2D matrix{
		globalTransformMatrix.a * _textureScale,
		globalTransformMatrix.b * _textureScale,
		-globalTransformMatrix.c * _textureScale,
		-globalTransformMatrix.d * _textureScale,
		pos.x * _textureScale,
		pos.y * _textureScale
	};

	_renderDisplay->set_transform(matrix);
	_renderDisplay->queue_redraw();
}

void Slot_GD::_onClear() {
	Slot::_onClear();

	_textureScale = 1.0f;
	_renderDisplay = nullptr;
}

/* GODOT CLASS WRAPPER FOR GIVING SCRIPT ACCESS */
void DragonBonesSlot::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_display_color_multiplier"), &DragonBonesSlot::get_display_color_multiplier);
	ClassDB::bind_method(D_METHOD("set_display_color_multiplier", "color"), &DragonBonesSlot::set_display_color_multiplier);
	ClassDB::bind_method(D_METHOD("set_display_index", "index"), &DragonBonesSlot::set_display_index);
	ClassDB::bind_method(D_METHOD("set_display_by_name", "name"), &DragonBonesSlot::set_display_by_name);
	ClassDB::bind_method(D_METHOD("get_display_index"), &DragonBonesSlot::get_display_index);
	ClassDB::bind_method(D_METHOD("get_display_count"), &DragonBonesSlot::get_display_count);
	ClassDB::bind_method(D_METHOD("next_display"), &DragonBonesSlot::next_display);
	ClassDB::bind_method(D_METHOD("previous_display"), &DragonBonesSlot::previous_display);
	ClassDB::bind_method(D_METHOD("get_child_armature"), &DragonBonesSlot::get_child_armature);
	ClassDB::bind_method(D_METHOD("get_slot_name"), &DragonBonesSlot::get_slot_name);
}

Color DragonBonesSlot::get_display_color_multiplier() {
	ERR_FAIL_NULL_V(slot, {});
	ColorTransform transform(slot->_colorTransform);

	Color return_color;
	return_color.r = transform.redMultiplier;
	return_color.g = transform.greenMultiplier;
	return_color.b = transform.blueMultiplier;
	return_color.a = transform.alphaMultiplier;

	return return_color;
}

void DragonBonesSlot::set_display_color_multiplier(const Color &_color) {
	ERR_FAIL_NULL(slot);
	ColorTransform _new_color;
	_new_color.redMultiplier = _color.r;
	_new_color.greenMultiplier = _color.g;
	_new_color.blueMultiplier = _color.b;
	_new_color.alphaMultiplier = _color.a;

	slot->_setColor(_new_color);
}

void DragonBonesSlot::set_display_index(int index) {
	ERR_FAIL_NULL(slot);
	slot->setDisplayIndex(index);
}

void DragonBonesSlot::set_display_by_name(const String &_name) {
	ERR_FAIL_NULL(slot);
	const std::vector<DisplayData *> *rawData = slot->getRawDisplayDatas();

	// we only want to update the slot if there's a choice
	if (rawData->size() > 1) {
		const char *desired_item = _name.ascii().get_data();
		std::string NONE_STRING("none");

		if (NONE_STRING.compare(desired_item) == 0) {
			slot->setDisplayIndex(-1);
		}

		for (int i = 0; i < rawData->size(); i++) {
			DisplayData *display_data = rawData->at(i);

			if (display_data->name.compare(desired_item) == 0) {
				slot->setDisplayIndex(i);
				return;
			}
		}
	} else {
		WARN_PRINT("Slot has only 1 item; refusing to set slot");
		return;
	}

	WARN_PRINT("Slot has no item called \"" + _name);
}

int DragonBonesSlot::get_display_index() {
	ERR_FAIL_NULL_V(slot, -1);
	return slot->getDisplayIndex();
}

int DragonBonesSlot::get_display_count() {
	ERR_FAIL_NULL_V(slot, -1);
	return slot->getDisplayList().size();
}

void DragonBonesSlot::next_display() {
	ERR_FAIL_NULL(slot);
	int current_slot = slot->getDisplayIndex();
	current_slot++;

	current_slot = current_slot < get_display_count() ? current_slot : -1;

	set_display_index(current_slot);
}

void DragonBonesSlot::previous_display() {
	ERR_FAIL_NULL(slot);
	int current_slot = slot->getDisplayIndex();
	current_slot--;

	current_slot = current_slot >= -1 ? current_slot : get_display_count() - 1;

	set_display_index(current_slot);
}

String DragonBonesSlot::get_slot_name() {
	ERR_FAIL_NULL_V(slot, {});
	return slot->getName().c_str();
}

DragonBonesArmature *DragonBonesSlot::get_child_armature() {
	if (!slot || slot->getDisplayList().size() == 0)
		return nullptr;

	std::pair<void *, DisplayType> display = slot->getDisplayList()[slot->getDisplayIndex()];
	if (display.second == DisplayType::Armature) {
		Armature *armature = static_cast<Armature *>(display.first);
		DragonBonesArmature *converted = static_cast<DragonBonesArmature *>(armature->getDisplay());
		return converted;
	}
	return nullptr;
}
