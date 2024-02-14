#pragma once

#include "common.h"

extern "C" {
    // We supported this limited set of animated channels
    enum class ChannelIdentifier {
        Invalid,
        TranslateX,
        TranslateY,
        TranslateZ,
        RotateX,
        RotateY,
        RotateZ,
        ScaleX,
        ScaleY,
        ScaleZ,
    };

    // An animation channel
    struct AnimationChannel {
        // This points to a node in the scene. Similar to node::parentIndex
        int nodeId = -1;
        ChannelIdentifier targetChannel = ChannelIdentifier::Invalid;
        // Double array with length
        uint32_t size = 0;
        double* data = nullptr;
    };

    // A collcetion of animation channels, in FBX land this is called a Take.
    struct AnimationChannels {
        uint32_t length = 0;
        AnimationChannel* channels = nullptr;
    };

    __declspec(dllexport) AnimationChannels* extractTakes(const struct FbxImportContext* context, double requestedFramesPerSecond, unsigned int* outCount);
    __declspec(dllexport) void freeTakes(const AnimationChannels* takes, uint32_t takeCount);
}
