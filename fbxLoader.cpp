#include <vector>

#include <fbxsdk.h>

#include "fbxLoader.h"

namespace TT_FBX {
    bool checkContext(const ::FbxImportContext* context) {
        return context != nullptr && (context->errorCode == ErrorCode::OK || context->errorCode == ErrorCode::WARNING);
    }
}

namespace {
    // Because we se this enum in a DLL functon, lets sanity check.
    static_assert(sizeof(FbxAxisSystem::EUpVector) == sizeof(int32_t), "");

    // Utlity to convert to a String struct.
    String makeString(const FbxArray<FbxString*>& details) {
        uint32_t bufSize = 0;
        for (int i = 0; i < details.GetCount(); i++) {
            bufSize += (uint32_t)details[i]->GetLen() + 1;
        }
        char* buf = new char[bufSize];
        char* cursor = buf;
        for (int i = 0; i < details.GetCount(); i++) {
            memcpy(cursor, details[i]->Buffer(), details[i]->GetLen());
            cursor += (uint32_t)details[i]->GetLen();
            *cursor = '\n';
            cursor += 1;
        }
        return { bufSize, cursor };
    }

    String makeString(const std::vector<std::string>& details) {
        uint32_t bufSize = 0;
        for (size_t i = 0; i < details.size(); i++) {
            bufSize += (uint32_t)details[i].size() + 1;
        }
        char* buf = new char[bufSize];
        char* cursor = buf;
        for (int i = 0; i < details.size(); i++) {
            memcpy(cursor, details[i].data(), details[i].size());
            cursor += details[i].size();
            *cursor = '\n';
            cursor += 1;
        }
        return { bufSize, buf };
    }

    // FBX allocator
    FbxManager* makeManager(ErrorCode& status) {
        FbxManager* manager = FbxManager::Create();
        if (!manager)
            status = ErrorCode::MANAGER_CREATE_FAILED;
        return manager;
    }

    // FBX scene container
    FbxScene* makeScene(FbxManager* manager, ErrorCode& status) {
        FbxIOSettings* ios = FbxIOSettings::Create(manager, IOSROOT);
        manager->SetIOSettings(ios);
        FbxString path = FbxGetApplicationDirectory();
        manager->LoadPluginsDirectory(path.Buffer());
        FbxScene* scene = FbxScene::Create(manager, "My Scene");
        if (!scene)
            status = ErrorCode::SCENE_CREATE_FAILED;
        return scene;
    }

    // Load an FBX file into a container
    void importIntoScene(FbxImportContext* context, const char* filePath) {
        // Create importer
        int lFileFormat = -1;
        FbxImporter* pImporter = FbxImporter::Create(context->manager, "");

        // Default to binary if format is not evident from file header
        if (!context->manager->GetIOPluginRegistry()->DetectReaderFileFormat(filePath, lFileFormat))
            lFileFormat = context->manager->GetIOPluginRegistry()->FindReaderIDByDescription("FBX binary (*.fbx)");;
        if (!pImporter->Initialize(filePath, lFileFormat)) exit(1);

        // Load file
        if (!pImporter->Import(context->scene))
            context->errorCode = ErrorCode::SCENE_IMPORT_FAILED;

        // Check the scene integrity!
        FbxArray<FbxString*> details;
        FbxStatus fbStatus;
        FbxSceneCheckUtility sceneCheck(context->scene, &fbStatus, &details);

        // TODO: Does this contain fatal errors or only warnings? For now the status remains OK.
        if (details.GetCount() != 0) {
            context->errorMessage = makeString(details);
        }

        if (pImporter->GetStatus().GetCode() != FbxStatus::eSuccess) {
            context->errorCode = ErrorCode::SCENE_IMPORT_FAILED;
            context->errorMessage = TT_FBX::makeString(pImporter->GetStatus().GetErrorString());
        }

        pImporter->Destroy();
    }

    // Import an fbx file and keep the relevant resources in memory
    FbxImportContext* beginImport(const char* filePath) {
        FbxImportContext* context = new FbxImportContext;

        context->manager = makeManager(context->errorCode);
        if (!TT_FBX::checkContext(context)) return context;

        context->scene = makeScene(context->manager, context->errorCode);
        if (!TT_FBX::checkContext(context))  {
            context->manager->Destroy();
            context->manager = nullptr;
            return context;
        }

        importIntoScene(context, filePath);
        if (!TT_FBX::checkContext(context)) {
            context->manager->Destroy();
            context->scene->Destroy();
            context->manager = nullptr;
            context->scene = nullptr;
            return context;
        }

        return context;
    }

    // Convert the current fbx scene
    void setAxisSystem(FbxImportContext* context, FbxAxisSystem::EUpVector up, FbxAxisSystem::EFrontVector front, FbxAxisSystem::ECoordSystem flip) {
        if (!TT_FBX::checkContext(context)) return;

        if (up != FbxAxisSystem::EUpVector::eXAxis && up != FbxAxisSystem::EUpVector::eYAxis && up != FbxAxisSystem::EUpVector::eZAxis) {
            context->errorCode = ErrorCode::INVALID_ARGUMENT;
            return;
        }

        if (front != FbxAxisSystem::EFrontVector::eParityEven && front != FbxAxisSystem::EFrontVector::eParityOdd) {
            context->errorCode = ErrorCode::INVALID_ARGUMENT;
            return;
        }

        if (flip != FbxAxisSystem::ECoordSystem::eLeftHanded && flip != FbxAxisSystem::ECoordSystem::eRightHanded) {
            context->errorCode = ErrorCode::INVALID_ARGUMENT;
            return;
        }

        // Convert Axis System to what is desired
        FbxAxisSystem sceneAxisSystem = context->scene->GetGlobalSettings().GetAxisSystem();
        FbxAxisSystem ourAxisSystem(up, front, flip);
        if (sceneAxisSystem != ourAxisSystem) {
            // ourAxisSystem.ConvertScene(context->scene);
            ourAxisSystem.DeepConvertScene(context->scene);
        }

        return;
    }

    // Convert the current fbx scene
    void setUnits(FbxImportContext* context, Units unit) {
        if (!TT_FBX::checkContext(context)) return;

        switch (unit) {
        case Units::mm:
            FbxSystemUnit::mm.ConvertScene(context->scene);
            break;
        case Units::dm:
            FbxSystemUnit::dm.ConvertScene(context->scene);
            break;
        case Units::cm:
            FbxSystemUnit::cm.ConvertScene(context->scene);
            break;
        case Units::m:
            FbxSystemUnit::m.ConvertScene(context->scene);
            break;
        case Units::km:
            FbxSystemUnit::km.ConvertScene(context->scene);
            break;
        case Units::Inch:
            FbxSystemUnit::Inch.ConvertScene(context->scene);
            break;
        case Units::Foot:
            FbxSystemUnit::Foot.ConvertScene(context->scene);
            break;
        case Units::Mile:
            FbxSystemUnit::Mile.ConvertScene(context->scene);
            break;
        case Units::Yard:
            FbxSystemUnit::Yard.ConvertScene(context->scene);
            break;
        default:
            context->errorCode = ErrorCode::INVALID_ARGUMENT;
            break;
        }
        return;
    }

    // Given a set of operatons, patch the scene
    void patchScene(FbxImportContext* context, ScenePatchFlags flags) {
        if (!TT_FBX::checkContext(context)) return;

        FbxGeometryConverter lGeomConverter(context->manager);

        if ((int)flags & (int)ScenePatchFlags::ConvertNurbsToPolygons) {
            // TODO: This requires us to loop over all the nurbs surfaces in the scene manually
            // lGeomConvert.ConvertNurbsSurfaceToNurbs();
        }

        if ((int)flags & (int)ScenePatchFlags::Triangulate) {
            // Triangulate mesh
            try {
                lGeomConverter.Triangulate(context->scene, /*replace*/true);
            } catch (std::runtime_error) {
                context->errorCode = ErrorCode::TRIANGULATION_FAILED;
            }
        }

        if ((int)flags & (int)ScenePatchFlags::RemoveBadPolygons)
            lGeomConverter.RemoveBadPolygonsFromMeshes(context->scene);

        if ((int)flags & (int)ScenePatchFlags::CollapseMeshes) {
            // TODO: This requires a list of meshes to merge, we must list all meshes manually first
            // lGeomConverter.MergeMeshes();
        }

        if ((int)flags & (int)ScenePatchFlags::SplitMeshesPerMaterial)
            lGeomConverter.SplitMeshesPerMaterial(context->scene, /*replace*/true);

        if ((int)flags & (int)ScenePatchFlags::CenterScene)
            lGeomConverter.RecenterSceneToWorldCenter(context->scene, /*replace*/true);

        return;
    }

    void getSceneInfo(FbxImportContext* context) {
        if (!TT_FBX::checkContext(context)) return;

        std::vector<std::string> warnings;

        context->info = new TT_FBX::SceneInfo;
        context->info->transforms.Add(context->scene->GetRootNode());
        context->info->transformParentIds.Add(-1);

        int cursor = 0;
        while (cursor < context->info->transforms.GetCount()) {
            FbxNode* node = context->info->transforms[cursor++];
            if (node->InheritType.Get() != FbxTransform::EInheritType::eInheritRSrs) {
                // TODO: Add the node name
                warnings.push_back("Unsupported transform inheritance type. We only support RSrs, as that is the only mode that results in a simple chld * parent matrix multiplication.");
            }

            for (int i = 0; i < node->GetChildCount(); i++) {
                context->info->transforms.Add(node->GetChild(i));
                context->info->transformParentIds.Add(cursor - 1);
            }
        }

        if (warnings.size() > 0) {
            context->errorCode = ErrorCode::WARNING;
            context->errorMessage = makeString(warnings);
        }
    }
}

extern "C" {
    // Load the scene into memory and convert it in whichever shape is desred.
    // The resulting context will need to be freed.
    __declspec(dllexport) FbxImportContext* importFbx(const char* filePath, FbxAxisSystem::EUpVector up, FbxAxisSystem::EFrontVector front, FbxAxisSystem::ECoordSystem flip, Units unit) {
        FbxImportContext* context = beginImport(filePath);
        if (!TT_FBX::checkContext(context)) return context;

        setAxisSystem(context, up, front, flip);
        if (!TT_FBX::checkContext(context)) return context;

        setUnits(context, unit);
        if (!TT_FBX::checkContext(context)) return context;

        patchScene(context, ScenePatchFlags::Triangulate);
        if (!TT_FBX::checkContext(context)) return context;

        getSceneInfo(context);
        if (!TT_FBX::checkContext(context)) return context;

        return context;
    }

    // The context can be freed after processing is done
    __declspec(dllexport) void freeFbx(const FbxImportContext* context) {
        if (context) {
            if (context->scene) context->scene->Destroy();
            if (context->manager) context->manager->Destroy();
            delete context->info;
            delete context;
        }
    }
}
