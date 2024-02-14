#include <vector>
#include <fbxsdk.h>

#include "fbxLoader.h"
#include "animationParser.h"

namespace {
    struct Take {
        FbxAnimStack* take = nullptr;
        FbxTime start = 0;
        FbxTime stop = 0;
    };

    inline std::pair<FbxTime, FbxTime> getTakeStartStop(const FbxScene* scene, const FbxTakeInfo* takeInfo) {
        FbxTime start, stop;
        if (takeInfo) {
            start = takeInfo->mLocalTimeSpan.GetStart();
            stop = takeInfo->mLocalTimeSpan.GetStop();
        } else {
            // Take the time line value
            FbxTimeSpan lTimeLineTimeSpan;
            scene->GetGlobalSettings().GetTimelineDefaultTimeSpan(lTimeLineTimeSpan);
            start = lTimeLineTimeSpan.GetStart();
            stop = lTimeLineTimeSpan.GetStop();
        }
        return { start, stop };
    }

    inline std::vector<Take> findTakes(FbxScene* scene) {
        std::vector<Take> outTakes;

        FbxArray<FbxString*> animStackNameArray;
        scene->FillAnimStackNameArray(animStackNameArray);

        for (int i = 0; i < animStackNameArray.GetCount(); ++i) {
            FbxAnimStack* animStack = scene->FindMember<FbxAnimStack>(animStackNameArray[i]->Buffer());
            if (!animStack) continue; // TODO: Warning?

            FbxTakeInfo* takeInfo = scene->GetTakeInfo(i);
            std::pair<FbxTime, FbxTime> startStop = getTakeStartStop(scene, takeInfo);
            if (startStop.second < startStop.first) continue; // TODO: warning?

            outTakes.push_back({ animStack, startStop.first, startStop.second });
        }

        return outTakes;
    }

    void evaluateDouble3Property(
        FbxProperty* channel,
        std::vector<AnimationChannel>& takeResult,
        int nodeIndex,
        double start,
        double requestesFramesPerSecond,
        uint32_t numFrames,
        ChannelIdentifier x,
        ChannelIdentifier y,
        ChannelIdentifier z
    ) {
        if (channel == nullptr)
            return;
        size_t offset = takeResult.size();
        takeResult.push_back({ nodeIndex, x, numFrames, new double[numFrames] });
        takeResult.push_back({ nodeIndex, y, numFrames, new double[numFrames] });
        takeResult.push_back({ nodeIndex, z, numFrames, new double[numFrames] });
        // For each frame in the take
        for (uint32_t frame = 0; frame < numFrames; ++frame) {
            double buf[3];
            FbxTime frameTime;
            frameTime.SetSecondDouble(start + frame / requestesFramesPerSecond);
            channel->EvaluateValue(frameTime).Get(buf, EFbxType::eFbxDouble3);
            takeResult[offset].data[frame] = buf[0];
            takeResult[offset + 1].data[frame] = buf[1];
            takeResult[offset + 2].data[frame] = buf[2];
        }
    }

    void evaluateRotationProperty(
        FbxProperty* channel,
        std::vector<AnimationChannel>& takeResult,
        int nodeIndex,
        double start,
        double requestesFramesPerSecond,
        uint32_t numFrames,
        ChannelIdentifier x,
        ChannelIdentifier y,
        ChannelIdentifier z,
        FbxEuler::EOrder rotationOrder,
        FbxAMatrix pre,
        FbxAMatrix post
    ) {
        if (channel == nullptr)
            return;
        size_t offset = takeResult.size();
        takeResult.push_back({ nodeIndex, x, numFrames, new double[numFrames] });
        takeResult.push_back({ nodeIndex, y, numFrames, new double[numFrames] });
        takeResult.push_back({ nodeIndex, z, numFrames, new double[numFrames] });
        // For each frame in the take
        for (uint32_t frame = 0; frame < numFrames; ++frame) {
            double buf[3];
            FbxTime frameTime;
            frameTime.SetSecondDouble(start + frame / requestesFramesPerSecond);
            channel->EvaluateValue(frameTime).Get(buf, EFbxType::eFbxDouble3);
            FbxAMatrix q = pre * TT_FBX::matrixFromEuler(rotationOrder, FbxVector4(buf[0], buf[1], buf[2], 0.0)) * post;
            FbxVector4 v = q.GetR();
            takeResult[offset].data[frame] = v[0];
            takeResult[offset + 1].data[frame] = v[1];
            takeResult[offset + 2].data[frame] = v[2];
        }
    }
}

extern "C" {
    __declspec(dllexport) AnimationChannels* extractTakes(const FbxImportContext* context, double requestedFramesPerSecond, uint32_t* outCount) {
        if(!TT_FBX::checkContext(context)) {
            *outCount = 0;
            return nullptr;
        }

        std::vector<AnimationChannels> result;
        std::vector<Take> takes = findTakes(context->scene);

        // For each take
        for(const Take& take : takes) {
            // Enable the take so evaluate calls will use this animation data
            context->scene->SetCurrentAnimationStack(take.take);

            // Combine the layers so we don't need to support animlayers at runtime
            FbxTime period;
            period.SetSecondDouble(1.0 / requestedFramesPerSecond);
            take.take->BakeLayers(context->scene->GetAnimationEvaluator(), take.start, take.stop, period);
            
            // Bake the animated propertes in the remaning layer
            FbxAnimLayer* baseLayer = (FbxAnimLayer*)take.take->GetMember(0);
            std::vector<AnimationChannel> takeResult;

            double startSeconds = take.start.GetSecondDouble();
            uint32_t numFrames = (uint32_t)ceil((take.stop.GetSecondDouble() - startSeconds) * requestedFramesPerSecond);
            if (numFrames == 0) continue;

            // For each transform
            for (int j = 0; j < context->info->transforms.GetCount(); ++j) {
                FbxNode* node = context->info->transforms[j];

                // Check whch properties are animated
                FbxProperty* translate = node->LclTranslation.IsAnimated(baseLayer) ? &node->LclTranslation : nullptr;
                FbxProperty* rotate = node->LclRotation.IsAnimated(baseLayer) ? &node->LclRotation : nullptr;
                FbxProperty* scale = node->LclScaling.IsAnimated(baseLayer) ? &node->LclScaling : nullptr;
                FbxEuler::EOrder rotateOrder = node->RotationOrder.Get();
                FbxAMatrix preRotation = TT_FBX::matrixFromEuler(rotateOrder, node->PreRotation.Get());
                FbxAMatrix postRotation = TT_FBX::matrixFromEuler(rotateOrder, node->PostRotation.Get());

                // Evaluate the animated properties and add the resulting channels to the output take
                evaluateDouble3Property(translate, takeResult, j, startSeconds, requestedFramesPerSecond, numFrames,
                    ChannelIdentifier::TranslateX, ChannelIdentifier::TranslateY, ChannelIdentifier::TranslateZ);
                evaluateRotationProperty(rotate, takeResult, j, startSeconds, requestedFramesPerSecond, numFrames,
                    ChannelIdentifier::RotateX, ChannelIdentifier::RotateY, ChannelIdentifier::RotateZ, rotateOrder, preRotation, postRotation);
                evaluateDouble3Property(scale, takeResult, j, startSeconds, requestedFramesPerSecond, numFrames,
                    ChannelIdentifier::ScaleX, ChannelIdentifier::ScaleY, ChannelIdentifier::ScaleZ);
            }

            result.push_back({ (uint32_t)takeResult.size(), TT_FBX::flattenList(takeResult) });
        }

        *outCount = (uint32_t)result.size();
        return TT_FBX::flattenList(result);
    }

    __declspec(dllexport) void freeTakes(const AnimationChannels* takes, uint32_t takeCount) {
        for (unsigned int i = 0; i < takeCount; ++i) {
            for (unsigned int j = 0; j < takes[i].length; ++j)
                delete[] takes[i].channels[j].data;
            delete[] takes[i].channels;
        }
        delete[] takes;
    }
}
