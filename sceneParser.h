#pragma once

#include "common.h"

extern "C" {
    // A transform in the scene hierarchy.
    // The FBX scene is traversed breadth-first and all nodes are wrapped
    // into this struct and added to an output array.
    // The parentIndex will point to the parent node in that array.
    // Similarly when loading meshes we get a MutliMeshData array,
    // and the meshIndex points to that.
    // -1 means no parent / no mesh.
    struct Node {
        String name;
        double translateX = 0.0;
        double translateY = 0.0;
        double translateZ = 0.0;
        double rotateX = 0.0;
        double rotateY = 0.0;
        double rotateZ = 0.0;
        double scaleX = 0.0;
        double scaleY = 0.0;
        double scaleZ = 0.0;
        int rotateOrder = 0;
        int parentIndex = -1;
        int meshIndex = -1;
    };

    __declspec(dllexport) Node* extractNodes(const struct FbxImportContext* context, uint32_t* outCount);
    __declspec(dllexport) void freeNodes(const Node* nodes, uint32_t nodeCount);
}
