#pragma once

#include <stdint.h>
#include <vector>

#include <fbxsdk/scene/geometry/fbxnodeattribute.h>

extern "C" {
    // String with length
    struct String {
        uint32_t length = 0;
        char* buffer = nullptr;
    };
}

namespace TT_FBX {
    // Utility to convert a vector to a C-array
    template<typename T>
    T* flattenList(const std::vector<T>& list) {
        T* result = new T[list.size()];
        int cursor = 0;
        for (const T& element : list)
            result[cursor++] = element;
        return result;
    }

    FbxAMatrix matrixFromEuler(FbxEuler::EOrder order, FbxVector4 euler);

    String makeString(const char* text);
}
