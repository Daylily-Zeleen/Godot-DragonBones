#ifndef DRAGONBONES_JSON_DATA_PARSER_H
#define DRAGONBONES_JSON_DATA_PARSER_H

#include "DataParser.h"
#include "rapidjson/document.h"

#include <memory/allocator.h>

using JsonDocument = rapidjson::GenericDocument<rapidjson::UTF8<>, GodotMemoryPoolAllocator, GodotAllocator>;
using JsonValue = rapidjson::GenericValue<rapidjson::UTF8<>, GodotMemoryPoolAllocator>;

DRAGONBONES_NAMESPACE_BEGIN

class ActionFrame 
{
public:
    unsigned frameStart;
    std::vector<unsigned> actions;

    bool operator < (const ActionFrame& b) const
    {
        return frameStart < b.frameStart;
    }
};

class JSONDataParser : public DataParser
{
    DRAGONBONES_DISALLOW_COPY_AND_ASSIGN(JSONDataParser)

protected:
    inline static bool _getBoolean(const JsonValue& rawData, const char* key, bool defaultValue)
    {
        if (rawData.HasMember(key))
        {
            const auto& value = rawData[key];
            if (value.IsBool())
            {
                return value.GetBool();
            }
            else if (value.IsString())
            {
                const std::string stringValue = value.GetString();
                if (
                    stringValue == "0" ||
                    stringValue == "NaN" ||
                    stringValue == "" ||
                    stringValue == "false" ||
                    stringValue == "null" ||
                    stringValue == "undefined"
                )
                {
                    return false;
                }

                return true;
            }
            else if (value.IsNumber())
            {
                return value.GetInt() != 0;
            }
        }

        return defaultValue;
    }

    inline static unsigned _getNumber(const JsonValue& rawData, const char* key, unsigned defaultValue)
    {
        if (rawData.HasMember(key))
        {
            return rawData[key].GetUint();
        }

        return defaultValue;
    }

    inline static int _getNumber(const JsonValue& rawData, const char* key, int defaultValue)
    {
        if (rawData.HasMember(key))
        {
            return rawData[key].GetInt();
        }

        return defaultValue;
    }

    inline static float _getNumber(const JsonValue& rawData, const char* key, float defaultValue)
    {
        if (rawData.HasMember(key) && rawData[key].IsNumber())
        {
            return rawData[key].GetDouble(); // cocos can not support GetFloat();
        }

        return defaultValue;
    }

    inline static std::string _getString(const JsonValue& rawData, const char* key, const std::string& defaultValue)
    {
        if (rawData.HasMember(key))
        {
            if (rawData[key].IsString())
            {
                return rawData[key].GetString();
            }

            return dragonBones::to_string(rawData[key].GetDouble());
        }

        return defaultValue;
    }

    inline static int _getParameter(const JsonValue& rawData, std::size_t index, int defaultValue)
    {
        if (rawData.Size() > index)
        {
            return rawData[(int) index].GetInt();
        }

        return defaultValue;
    }

    inline static float _getParameter(const JsonValue& rawData, std::size_t index, float defaultValue)
    {
        if (rawData.Size() > index)
        {
            return rawData[(int) index].GetDouble();
        }

        return defaultValue;
    }

    inline static std::string _getParameter(const JsonValue& rawData, std::size_t index, const std::string& defaultValue)
    {
        if (rawData.Size() > index)
        {
            return rawData[(int) index].GetString();
        }

        return defaultValue;
    }

protected:
    unsigned _rawTextureAtlasIndex;
    std::vector<BoneData*> _rawBones;
    DragonBonesData* _data;
    ArmatureData* _armature;
    BoneData* _bone;
    SlotData* _slot;
    SkinData* _skin;
    MeshDisplayData* _mesh;
    AnimationData* _animation;
    TimelineData* _timeline;
    JsonValue* _rawTextureAtlases;

private:
    int _defaultColorOffset;
    int _prevClockwise;
    float _prevRotation;
    Matrix _helpMatrixA;
    Matrix _helpMatrixB;
    Transform _helpTransform;
    ColorTransform _helpColorTransform;
    Point _helpPoint;
    std::vector<float> _helpArray;
    std::vector<std::int16_t> _intArray;
    std::vector<float> _floatArray;
    std::vector<std::int16_t> _frameIntArray;
    std::vector<float> _frameFloatArray;
    std::vector<std::int16_t> _frameArray;
    std::vector<std::uint16_t> _timelineArray;
    std::vector<const JsonValue*> _cacheRawMeshes;
    std::vector<MeshDisplayData*> _cacheMeshes;
    std::vector<ActionFrame> _actionFrames;
    std::map<std::string, const JsonValue*> _weightSlotPose;
    std::map<std::string, const JsonValue*> _weightBonePoses;
    std::map<std::string, std::vector<BoneData*>> _cacheBones;
    std::map<std::string, std::vector<ActionData*>> _slotChildActions;

public:
    JSONDataParser() :
        _rawTextureAtlasIndex(0),
        _rawBones(),
        _data(nullptr),
        _armature(nullptr),
        _bone(nullptr),
        _slot(nullptr),
        _skin(nullptr),
        _mesh(nullptr),
        _animation(nullptr),
        _timeline(nullptr),
        _rawTextureAtlases(nullptr),

        _defaultColorOffset(-1),
        _prevClockwise(0),
        _prevRotation(0.0f),
        _helpMatrixA(),
        _helpMatrixB(),
        _helpTransform(),
        _helpColorTransform(),
        _helpPoint(),
        _helpArray(),
        _intArray(),
        _floatArray(),
        _frameIntArray(),
        _frameFloatArray(),
        _frameArray(),
        _timelineArray(),
        _cacheMeshes(),
        _cacheRawMeshes(),
        _actionFrames(),
        _weightSlotPose(),
        _weightBonePoses(),
        _cacheBones(),
        _slotChildActions()
    {
    }
    virtual ~JSONDataParser()
    {
    }

private:
    void _getCurvePoint(
        float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, 
        float t,
        Point& result
    );
    void _samplingEasingCurve(const JsonValue& curve, std::vector<float>& samples);
    void _parseActionDataInFrame(const JsonValue& rawData, unsigned frameStart, BoneData* bone, SlotData* slot);
    void _mergeActionFrame(const JsonValue& rawData, unsigned frameStart, ActionType type, BoneData* bone, SlotData* slot);
    unsigned _parseCacheActionFrame(ActionFrame& frame);

protected:
    virtual ArmatureData* _parseArmature(const JsonValue& rawData, float scale);
    virtual BoneData* _parseBone(const JsonValue& rawData);
    virtual ConstraintData* _parseIKConstraint(const JsonValue& rawData);
    virtual SlotData* _parseSlot(const JsonValue& rawData, int zOrder);
    virtual SkinData* _parseSkin(const JsonValue& rawData);
    virtual DisplayData* _parseDisplay(const JsonValue& rawData);
    virtual void _parsePivot(const JsonValue& rawData, ImageDisplayData& display);
    virtual void _parseMesh(const JsonValue& rawData, MeshDisplayData& mesh);
    virtual BoundingBoxData* _parseBoundingBox(const JsonValue& rawData);
    virtual PolygonBoundingBoxData* _parsePolygonBoundingBox(const JsonValue& rawData);
    virtual AnimationData* _parseAnimation(const JsonValue& rawData);
    virtual TimelineData* _parseTimeline(
        const JsonValue& rawData, const char* framesKey, TimelineType type,
        bool addIntOffset, bool addFloatOffset, unsigned frameValueCount,
        const std::function<unsigned(const JsonValue& rawData, unsigned frameStart, unsigned frameCount)>& frameParser
    );
    virtual void _parseBoneTimeline(const JsonValue& rawData);
    virtual void _parseSlotTimeline(const JsonValue& rawData);
    virtual unsigned _parseFrame(const JsonValue& rawData, unsigned frameStart, unsigned frameCount);
    virtual unsigned _parseTweenFrame(const JsonValue& rawData, unsigned frameStart, unsigned frameCount);
    virtual unsigned _parseActionFrame(const ActionFrame& rawData, unsigned frameStart, unsigned frameCount);
    virtual unsigned _parseZOrderFrame(const JsonValue& rawData, unsigned frameStart, unsigned frameCount);
    virtual unsigned _parseBoneAllFrame(const JsonValue& rawData, unsigned frameStart, unsigned frameCount);
    virtual unsigned _parseBoneTranslateFrame(const JsonValue& rawData, unsigned frameStart, unsigned frameCount);
    virtual unsigned _parseBoneRotateFrame(const JsonValue& rawData, unsigned frameStart, unsigned frameCount);
    virtual unsigned _parseBoneScaleFrame(const JsonValue& rawData, unsigned frameStart, unsigned frameCount);
    virtual unsigned _parseSlotDisplayFrame(const JsonValue& rawData, unsigned frameStart, unsigned frameCount);
    virtual unsigned _parseSlotColorFrame(const JsonValue& rawData, unsigned frameStart, unsigned frameCount);
    virtual unsigned _parseSlotFFDFrame(const JsonValue& rawData, unsigned frameStart, unsigned frameCount);
    virtual unsigned _parseIKConstraintFrame(const JsonValue& rawData, unsigned frameStart, unsigned frameCount);
    virtual const std::vector<ActionData*>& _parseActionData(const JsonValue& rawData, ActionType type, BoneData* bone, SlotData* slot);
    virtual void _parseTransform(const JsonValue& rawData, Transform& transform, float scale);
    virtual void _parseColorTransform(const JsonValue& rawData, ColorTransform& color);
    virtual void _parseArray(const JsonValue& rawData);
    virtual DragonBonesData* _parseDragonBonesData(const JsonValue& rawData, float scale = 1.0f);
    virtual void _parseTextureAtlasData(const JsonValue& rawData, TextureAtlasData& textureAtlasData, float scale = 1.0f);

public:
    virtual DragonBonesData* parseDragonBonesData(const char* rawData, float scale = 1.0f) override;
    virtual bool parseTextureAtlasData(const char* rawData, TextureAtlasData& textureAtlasData, float scale = 1.0f) override;
};

DRAGONBONES_NAMESPACE_END
#endif // DRAGONBONES_JSON_DATA_PARSER_H
