#include <fbxsdk.h>
#include <vector>

#include "fbxLoader.h"
#include "sceneParser.h"

namespace {
    const int rotateOrderInts[] = {
        0b00'01'10,
        0b00'10'01,
        0b01'00'10,
        0b01'10'00,
        0b10'00'01,
        0b10'01'00,
    };
}

extern "C" {
    // Extract the scene hierarchy and their initial transforms
    __declspec(dllexport) Node* extractNodes(const FbxImportContext* context, uint32_t* outCount) {
        if (!TT_FBX::checkContext(context)) {
            *outCount = 0;
            return nullptr;
        }

        std::vector<Node> scene;

        int meshCounter = 0;
        for (int i = 0; i < context->info->transforms.GetCount(); ++i) {
            FbxNode* node = context->info->transforms[i];

            FbxVector4 t = node->EvaluateLocalTranslation();
            FbxVector4 r = node->EvaluateLocalRotation();
            FbxVector4 s = node->EvaluateLocalScaling();
            FbxEuler::EOrder rotateOrder = node->RotationOrder.Get();
            // TODO: Error if rotateOrder == FbxEuler::EOrder::eOrderSphericXYZ

            // Joints can have a rotation offset that needs to be applied beforehand
            FbxAMatrix preRotation = TT_FBX::matrixFromEuler(rotateOrder, node->PreRotation.Get());
            FbxAMatrix postRotation = TT_FBX::matrixFromEuler(rotateOrder, node->PostRotation.Get());
            FbxAMatrix rotation = preRotation * TT_FBX::matrixFromEuler(rotateOrder, r) * postRotation;
            r = rotation.GetR();

            // Get the node name as a buffer we own
            FbxString name = node->GetNameOnly();
            char* buffer = new char[name.Size()];
            memcpy(buffer, name.Buffer(), name.Size());

            // Track if this transform has a mesh, and if so, at what index that would be in the flattened mesh array
            int meshIndex = -1;
            if (node->GetNodeAttribute() && node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh) {
                meshIndex = meshCounter;
                meshCounter++;
            }

            scene.push_back({ { (unsigned int)name.Size(), buffer }, t[0], t[1], t[2], r[0], r[1], r[2], s[0], s[1], s[2], rotateOrderInts[(int)rotateOrder], context->info->transformParentIds[i], meshIndex });
        }

        // Output the resulting scene
        *outCount = (uint32_t)scene.size();
        return TT_FBX::flattenList(scene);
    }

    __declspec(dllexport) void freeNodes(const Node* nodes, uint32_t nodeCount) {
        for (unsigned int i = 0; i < nodeCount; ++i)
            delete[] nodes[i].name.buffer;
        delete[] nodes;
    }
}
