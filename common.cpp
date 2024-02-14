#include <vector>

#include <fbxsdk.h>

#include "common.h"
#include "fbxLoader.h"

namespace TT_FBX {
    FbxAMatrix matrixFromEuler(FbxEuler::EOrder order, FbxVector4 euler) {
        FbxAMatrix result;
        result.SetR(euler, order);
        return result;
    }

    String makeString(const char* text) {
        String r;
        r.length = (uint32_t)strlen(text);
        r.buffer = new char[r.length];
        memcpy(r.buffer, text, r.length);
        return r;
    }
}
