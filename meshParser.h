#pragma once

#include "common.h"

extern "C" {
    // The numeric values of these are important,
    // we just use this enum as a set of constants for readability.
    // When extending / deprecating fields in this, make sure
    // values do not overlap!
    enum class Semantic : uint8_t {
        // Fbx meshes can have multiple instances of the same attribute, 
        // for now we support up to 8 of each type.
        _Stride = 8,

        // Fbx has only 1 position attribute
        Position = 0,
        // For skinning we support 8 weights at most, so we consume 4 slots for those
        SkinIndices0,
        SkinIndices1,
        SkinWeights0,
        SkinWeights1,
        Normal,
        Tangent = Normal + _Stride,
        Binormal = Tangent + _Stride,
        UV = Binormal + _Stride,
        // Color can theoretically keep going, so any extra data can be stored as color
        Color = UV + _Stride,
    };

    // Vertex attributes can be float, vec2, vec3 or vec4
    enum class NumElements : uint8_t {
        Invalid = 0,
        Vec1,
        Vec2,
        Vec3,
        Vec4
    };

    // For convenience these values match the OpenGL constants
    enum class ElementType : uint32_t {
        UInt32 = 0x1405,
        Float = 0x1406,
    };

    // All vertex data comes interleaved as 1 buffer.
    // This layout describes which bytes represent what information
    // and is intended to be used in conjunction with glVertexAttribPointer.
    // The semantic integer will probably exceed GL_MAX_VERTEX_ATTRIBS,
    // so some remapping is required before using this struct as arguments directly.
    struct VertexAttribute {
        // See the Semantic enum for more details.
        Semantic semantic = Semantic::Position;
        // 1, 2, 3 or 4
        NumElements numElements = NumElements::Vec3;
        // GLenum that directly feeds glVertexAttribPointer
        ElementType elementType = ElementType::Float;
    };

    // A mesh is split up by material, the submeshes share the same vertex attributes
    // but have their own vertex and index buffers, as well as a handle to identify the material.
    struct MeshData {
        // An index into MultiMeshData::materialNames
        uint32_t materialId = 0;

        uint32_t vertexDataSizeInBytes = 0;
        uint32_t indexDataSizeInBytes = 0; // can be 0

        uint8_t* vertexDataBlob = nullptr;
        uint8_t* indexDataBlob = nullptr;
    };

    // Each FbxMesh in the scene gets converted to a MutliMeshData instance.
    struct MultiMeshData {
        String version;

        String name;

        uint32_t materialNameCount = 0;
        String* materialNames = nullptr;

        uint32_t uvSetNameCount = 0;
        String* uvSetNames = nullptr;

        uint32_t attributeCount = 0;
        VertexAttribute* attributeLayout = nullptr;

        uint32_t primitiveType = 0; // GLenum

        uint8_t indexElementSizeInBytes = 0; // 1, 2, 4, or 0 if data size is 0

        uint32_t meshCount = 0;
        MeshData* meshes = nullptr;

        uint32_t jointCount = 0;
        uint32_t* jointIndexData = nullptr;
    };

    __declspec(dllexport) MultiMeshData* extractMeshes(const struct FbxImportContext* context, uint32_t* outCount);
    __declspec(dllexport) void freeMeshes(const MultiMeshData* meshes, uint32_t meshCount);
}
