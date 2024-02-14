#pragma once

#include <fbxsdk/scene/fbxaxissystem.h>

#include "common.h"

namespace TT_FBX {
    // This struct encapsulates some preprocessed data computed directly after import.
    // The data is not exposed but part of FbxImportContext to accellerate processing the scene
    // without overcomplicatng the API.
    struct SceneInfo {
        FbxArray<FbxNode*> transforms;
        FbxArray<int> transformParentIds;
    };
}

extern "C" {
    // FBX load error codes. See FbxImportContext.
    enum class ErrorCode {
        OK,
        WARNING,
        MANAGER_CREATE_FAILED,
        SCENE_CREATE_FAILED,
        SCENE_IMPORT_FAILED,
        INVALID_ARGUMENT,
        TRIANGULATION_FAILED,
    };

    // These map to FBX SDK units
    enum class Units {
        mm,
        dm,
        cm,
        m,
        km,
        Inch,
        Foot,
        Mile,
        Yard,
    };

    // Bitfield, set bits to perform the operation
    enum class ScenePatchFlags {
        ConvertNurbsToPolygons = 1 << 0,
        Triangulate = 1 << 1,
        RemoveBadPolygons = 1 << 2,
        CollapseMeshes = 1 << 3,
        SplitMeshesPerMaterial = 1 << 4,
        CenterScene = 1 << 5,
    };

    // This object provides a handle to the Fbx scene to pass around,
    // as well as wrap error state. It is returned by the FBX API calls,
    // before any actual parsing is done.
    struct FbxImportContext {
        class FbxManager* manager = nullptr;
        class FbxScene* scene = nullptr;
        ErrorCode errorCode = ErrorCode::OK;
        // We do not always set an error message, sometimes the code is enough.
        String errorMessage;
        
        // Every scene operaton wants to understand the node hierarchy, so we extract this pre-emptyively after loading the scene into memory.
        TT_FBX::SceneInfo* info = nullptr;
    };

    __declspec(dllexport) FbxImportContext* importFbx(const char* filePath, FbxAxisSystem::EUpVector up, FbxAxisSystem::EFrontVector front, FbxAxisSystem::ECoordSystem flip, Units unit);
    __declspec(dllexport) void freeFbx(const FbxImportContext* context);
}

namespace TT_FBX {
    bool checkContext(const ::FbxImportContext*);
}
