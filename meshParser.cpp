#include <fbxsdk.h>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>

#include "fbxLoader.h"
#include "meshParser.h"

namespace {
    struct Vertex {
        uint32_t cursor = 0;

        std::vector<unsigned char> binaryArray;

        void setFloat(float value) {
            unsigned char* blob = (unsigned char*)&value;
            for (uint32_t i = 0; i < sizeof(float); ++i)
                binaryArray[cursor++] = blob[i];
        }

        void setIndex(uint32_t value) {
            unsigned char* blob = (unsigned char*)&value;
            for (uint32_t i = 0; i < sizeof(uint32_t); ++i)
                binaryArray[cursor++] = blob[i];
        }

        void setVec2(FbxVector2 value) {
            setFloat((float)value[0]);
            setFloat((float)value[1]);
        }

        void setVec3(FbxVector4 value) {
            setFloat((float)value[0]);
            setFloat((float)value[1]);
            setFloat((float)value[2]);
        }

        void setColor(FbxColor value) {
            setFloat((float)value[0]);
            setFloat((float)value[1]);
            setFloat((float)value[2]);
            setFloat((float)value[3]);
        }
    };

    // Given an FbxLayerElement (vertex attribute), we resolve the index to read and get the value for the given vertex/face (polygon)/control point.
    template<typename T>
    T getVertexAttributeValue(int controlPointIndex, const FbxMesh* mesh, const FbxLayerElementTemplate<T>* element, size_t polygonIndex, size_t globalVertexIndex) {
        // Based on the mapping mode we need to sample a different index in the element's data.
        FbxGeometryElement::EMappingMode lMappingMode = element->GetMappingMode();
        int i;
        // Simple 1:1 mapping, mostly used by position (and often color) attributes.
        // Can be used by normals when all normals are soft.
        if (lMappingMode == FbxGeometryElement::eByControlPoint)
            i = controlPointIndex;
        // Unique value per real vertex, mostly used by everything that needs to be abele to split on edges,
        // like UV seams and hard normals.
        else if (lMappingMode == FbxGeometryElement::eByPolygonVertex)
            i = (int)globalVertexIndex;
        // Unique value per face, can be used if all normals are hard for example.
        else if (lMappingMode == FbxGeometryElement::eByPolygon)
            i = (int)polygonIndex;
        // Should probably never happen, but it is possible
        else if (lMappingMode == FbxGeometryElement::eNone)
            return {};
        // Not implemented yet
        // else if (lMappingMode == FbxGeometryElement::eByEdge)
        // else if (lMappingMode == FbxGeometryElement::eAllSame)
        else {
            // other modes not supported
            __debugbreak();
            return {};
        }

        // Next, the index can be an indirection as well, when the data is reusable we can have an index buffer
        // to map the index derived from the mapping mode to an actual data array index.
        if (element->GetReferenceMode() == FbxLayerElement::eIndexToDirect)
            i = element->GetIndexArray().GetAt(i);
        else if (element->GetReferenceMode() == FbxLayerElement::eDirect) {}
        // FbxLayerElement::eIndex implies that we go from input index to output index, but that output index appears meaningless / is not a pointer to the data?
        // else if (element->GetReferenceMode() == FbxLayerElement::eIndex)
        else {
            // other modes not supported
            __debugbreak();
            return {};
        }

        // Finally, return the value at the right index.
        return element->GetDirectArray().GetAt(i);
    }

    // Get data for a single vertex, by reading each attribute in the mesh and filling the Vertex structure.
    void getVertex(const FbxMesh* pMesh, size_t polygonIndex, size_t localVertexIndex, size_t globalVertexIndex, Vertex& vertexBuffer, const std::vector<std::vector<std::pair<int, double>>>& orderedSkinWeights) {
        // Reset the vertex buffer.
        vertexBuffer.cursor = 0;

        // Get the control point, required in case data is mapped by control point.
        int controlPointIndex = pMesh->GetPolygonVertex((int)polygonIndex, (int)localVertexIndex);

        // Positions are always stored by control point, so getting that is easy.
        vertexBuffer.setVec3(pMesh->GetControlPointAt(controlPointIndex));

        // If the mesh has skin weights, write those.
        if (orderedSkinWeights.size() > 0) {
            auto pairs = orderedSkinWeights[controlPointIndex];
            // Write 8 indices.
            for (int j = 0; j < 8; ++j) {
                int index = (int)pairs.size() - 1 - j;
                if (index < 0) {
                    // write index 0, value 0.
                    vertexBuffer.setIndex(0u);
                    continue;
                }
                // write index & weight.
                vertexBuffer.setIndex((uint32_t)pairs[index].first);
            }
            // Write 8 floats.
            for (int j = 0; j < 8; ++j) {
                int index = (int)pairs.size() - 1 - j;
                if (index < 0) {
                    // write index 0, value 0.
                    vertexBuffer.setFloat(0.0f);
                    continue;
                }
                // write index & weight.
                vertexBuffer.setFloat((float)pairs[index].second);
            }
        }

        // FbxMesh::GetElementNormalCount, FbxMesh::GetElementNormal, Vertex::setVec3

        // Finally, append each attribute in the mesh to the vertex buffer.
        for (size_t x = 0; x < std::min((int)Semantic::_Stride, pMesh->GetElementNormalCount()); ++x)
            vertexBuffer.setVec3(getVertexAttributeValue(controlPointIndex, pMesh, pMesh->GetElementNormal((int)x), polygonIndex, globalVertexIndex));

        for (size_t x = 0; x < std::min((int)Semantic::_Stride, pMesh->GetElementTangentCount()); ++x)
            vertexBuffer.setVec3(getVertexAttributeValue(controlPointIndex, pMesh, pMesh->GetElementTangent((int)x), polygonIndex, globalVertexIndex));

        for (size_t x = 0; x < std::min((int)Semantic::_Stride, pMesh->GetElementBinormalCount()); ++x)
            vertexBuffer.setVec3(getVertexAttributeValue(controlPointIndex, pMesh, pMesh->GetElementBinormal((int)x), polygonIndex, globalVertexIndex));

        for (size_t x = 0; x < std::min((int)Semantic::_Stride, pMesh->GetElementUVCount()); ++x)
            vertexBuffer.setVec2(getVertexAttributeValue(controlPointIndex, pMesh, pMesh->GetElementUV((int)x), polygonIndex, globalVertexIndex));

        for (size_t x = 0; x < std::min(255 - (int)Semantic::Color, pMesh->GetElementVertexColorCount()); ++x)
            vertexBuffer.setColor(getVertexAttributeValue(controlPointIndex, pMesh, pMesh->GetElementVertexColor((int)x), polygonIndex, globalVertexIndex));
    }
    
    struct ManagedMeshData {
        uint32_t materialId = 0;
        std::vector<unsigned char> vertexData;
        std::vector<uint32_t> indexData;
    };

    template<typename K, typename V>
    bool cmp(std::pair<K, V>& a, std::pair<K, V>& b) {
        return a.second < b.second;
    }

    template<typename K, typename V>
    std::vector<std::pair<K, V>> sort(std::map<K, V>& M) {
        std::vector<std::pair<K, V>> A;
        for (auto& it : M)
            A.push_back(it);
        std::sort(A.begin(), A.end(), cmp<K, V>);
        return A;
    }

    struct SkinnedMeshInfo {
        // For each vertex we have a pair of joint & weight, sorted from low to high weights.
        std::vector<std::vector<std::pair<int, double>>> orderedSkinWeights;
        // These indices map to the Node* array returned by processScene().
        std::vector<uint32_t> jointIdToNodeMap; 
    };

    // Get skin weights of the first skin in the mesh, result is empty if no skin.
    inline SkinnedMeshInfo extractSkinWeights(const FbxMesh* pMesh, const FbxArray<FbxNode*>& stack) {
        // Extract skin weights.
        int numSkins = pMesh->GetDeformerCount(FbxDeformer::eSkin);
        // TODO: warning if numSkins > 1, or should we support multiple? I don't know any DCC besides Maya where you can do this, and even there it is neigh impossible through the GUI.
        SkinnedMeshInfo result;
        if (numSkins > 0) {
            FbxSkin* skin = (FbxSkin*)pMesh->GetDeformer(0, FbxDeformer::eSkin);
            // TODO: error if (skin->GetSkinningType() != FbxSkin::eLinear || skin->GetSkinningType() != FbxSkin::eRigid);

            int vertexCount = pMesh->GetControlPointsCount();
            std::vector<std::map<int, double>> skinWeights;
            skinWeights.resize(vertexCount);
            for (int jointId = 0; jointId < skin->GetClusterCount(); ++jointId) {
                FbxCluster* lCluster = skin->GetCluster(jointId);
                FbxNode* link = lCluster->GetLink();
                // TODO: Error if !link
                result.jointIdToNodeMap.push_back(stack.Find(link));

                int vertexIndexCount = lCluster->GetControlPointIndicesCount();
                for (int k = 0; k < vertexIndexCount; ++k) {
                    int vertexId = lCluster->GetControlPointIndices()[k];
                    // Sometimes, the mesh can have less points than at the time of the skinning
                    // because a smooth operator was active when skinning but has been deactivated during export.
                    if (vertexId >= vertexCount)
                        continue;
                    double weight = lCluster->GetControlPointWeights()[k];
                    if (weight == 0.0)
                        continue;
                    // Now we know that jointId influences vertexId with weight, let's store that information.
                    skinWeights[vertexId][jointId] = weight;
                }
            }

            // For each vertex, sort the weights descending by value so we can nibble the tail for most important weights.
            for (auto entry : skinWeights)
                result.orderedSkinWeights.push_back(sort(entry));
        }
        return result;
    }

    // Describe the contents of the vertex buffer based on the available fbx attributes.
    inline std::vector<VertexAttribute> getMeshVertexLayout(const FbxMesh* mesh, bool isSkinned) {
        std::vector<VertexAttribute> layout;
        layout.push_back({ Semantic::Position, NumElements::Vec3, ElementType::Float });

        if (isSkinned) {
            layout.push_back({ Semantic::SkinIndices0, NumElements::Vec4, ElementType::UInt32 });
            layout.push_back({ Semantic::SkinIndices1, NumElements::Vec4, ElementType::UInt32 });
            layout.push_back({ Semantic::SkinWeights0, NumElements::Vec4, ElementType::Float });
            layout.push_back({ Semantic::SkinWeights1, NumElements::Vec4, ElementType::Float });
        }

        for (int offset = 0; offset < std::min((int)Semantic::_Stride, mesh->GetElementNormalCount()); ++offset) 
            layout.push_back({ (Semantic)((int)Semantic::Normal + offset), NumElements::Vec3, ElementType::Float });

        for (int offset = 0; offset < std::min((int)Semantic::_Stride, mesh->GetElementTangentCount()); ++offset) 
            layout.push_back({ (Semantic)((int)Semantic::Tangent + offset), NumElements::Vec3, ElementType::Float });

        for (int offset = 0; offset < std::min((int)Semantic::_Stride, mesh->GetElementBinormalCount()); ++offset) 
            layout.push_back({ (Semantic)((int)Semantic::Binormal + offset), NumElements::Vec3, ElementType::Float });

        for (int offset = 0; offset < std::min((int)Semantic::_Stride, mesh->GetElementUVCount()); ++offset)
            layout.push_back({ (Semantic)((int)Semantic::UV + offset), NumElements::Vec2, ElementType::Float });

        for (int offset = 0; offset < std::min(255 - (int)Semantic::Color, mesh->GetElementVertexColorCount()); ++offset) 
            layout.push_back({ (Semantic)((int)Semantic::Color + offset), NumElements::Vec4, ElementType::Float });

        return layout;
    }

    inline int strideFromlayout(const std::vector<VertexAttribute>& layout) {
        int stride = 0;
        for (const VertexAttribute& key : layout) {
            int elementSize = 0;
            switch (key.elementType) {
            case ElementType::Float:
                elementSize = 4;
                break;
            case ElementType::UInt32:
                elementSize = 4;
                break;
            default:
                // TODO: Not implemented error.
                __debugbreak();
                break;
            }
            stride += elementSize * (int)key.numElements;
        }
        return stride;
    }

    inline std::vector<std::string> getUvSetNames(const FbxMesh* mesh) {
        std::vector<std::string> uvSetNames;
        for (int i = 0; i < mesh->GetElementUVCount(); ++i)
            uvSetNames.emplace_back(mesh->GetElementUV(i)->GetName());
        return uvSetNames;
    }

    String* makeStringList(const std::vector<std::string>& list) {
        String* result = new String[list.size()];
        int cursor = 0;
        for (const std::string& text : list)
            result[cursor++] = TT_FBX::makeString(text.c_str());
        return result;
    }

    MeshData* flattenValues(const std::unordered_map<size_t, ManagedMeshData>& subMeshByMaterial) {
        MeshData* result = new MeshData[subMeshByMaterial.size()];
        size_t cursor = 0;
        for (const auto& pair : subMeshByMaterial) {
            MeshData& element = result[cursor];
            element.materialId = pair.second.materialId;

            element.vertexDataSizeInBytes = (unsigned int)pair.second.vertexData.size();
            element.vertexDataBlob = new unsigned char[element.vertexDataSizeInBytes];
            memcpy(element.vertexDataBlob, pair.second.vertexData.data(), element.vertexDataSizeInBytes);

            element.indexDataSizeInBytes = (unsigned int)pair.second.indexData.size() * sizeof(unsigned int);
            element.indexDataBlob = new unsigned char[element.indexDataSizeInBytes];
            memcpy(element.indexDataBlob, pair.second.indexData.data(), element.indexDataSizeInBytes);

            cursor++;
        }
        return result;
    }

    // Read a single mesh and return a multi-mesh with submeshes split up by material.
    MultiMeshData extractMesh(const FbxMesh* mesh, const FbxArray<FbxNode*>& stack) {
        const FbxNode* owner = mesh->GetNode();
        if (!owner) return {};

        // Extract skin weights.
        SkinnedMeshInfo skin = extractSkinWeights(mesh, stack);
        bool isSkinned = skin.orderedSkinWeights.size() != 0;
        
        // Verify we can fully export this mesh.
        if (mesh->GetElementPolygonGroupCount() != 0 ||
            mesh->GetElementSmoothingCount() != 0 ||
            mesh->GetElementVertexCreaseCount() != 0 ||
            mesh->GetElementEdgeCreaseCount() != 0 ||
            mesh->GetElementHoleCount() != 0 ||
            mesh->GetElementVisibilityCount() != 0 ||
            mesh->GetElementUserData() != 0) {
            // TODO: warning, unsupported vertex attribute.
        }

        // Extract uv set names
        std::vector<std::string> uvSetNames = getUvSetNames(mesh);

        // Get vertex layout
        std::vector<VertexAttribute> layout = getMeshVertexLayout(mesh, isSkinned);
        
        // Get number of bytes per vertex
        int stride = strideFromlayout(layout);

        // Set up a vertex buffer to write vertex data into.
        Vertex vertexBuffer;
        vertexBuffer.binaryArray.resize(stride);
        // We hash each vertex after reading it so we can reuse vertex data.
        std::hash<std::string_view> hasher;
        std::string_view view((const char* const)vertexBuffer.binaryArray.data(), vertexBuffer.binaryArray.size());

        // For each material we hash the name and store a submesh with the data for that mesh.
        std::unordered_map<size_t, ManagedMeshData> subMeshByMaterial;
        // For each submesh (same material hash as key), we store the hash of each vertex, and the index of that vertex.
        // That way, when the vertexBuffer hash already exists, we reuse the existing data instead of writing it twice.
        std::unordered_map<size_t, std::unordered_map<size_t, uint32_t>> vertexMaps;

        // Track used material names as we encounter them
        int materialCount = mesh->GetElementMaterialCount();
        std::vector<std::string> materialNames;
        // Use a map to speed up finding whether we can insert a material name or not
        // TODO: Can shouldn't we use an ordered set instead?
        std::unordered_map<size_t, size_t> materialNamesHashToIndex;
        std::hash<std::string> strHasher;

        // If the mesh has materials assigned, we can query which material to use from that mesh attribute
        FbxLayerElementArrayTemplate<int>* materialIndices = NULL;
        FbxGeometryElement::EMappingMode materialMappingMode = FbxGeometryElement::eNone;

        // Count the total number of vertices written so far
        size_t globalVertexIndex = 0;
        for (int polygonIndex = 0; polygonIndex < mesh->GetPolygonCount(); ++polygonIndex) {
            // We only support polygons with a surface area
            int polygonVertexCount = mesh->GetPolygonSize(polygonIndex);
            if (polygonVertexCount < 3) {
                globalVertexIndex += 2;
                continue;
            }
            
            // Get the material for the current face.
            int localMaterialIndex = 0;
            if (materialCount > 0) {
                materialIndices = &mesh->GetElementMaterial()->GetIndexArray();
                materialMappingMode = mesh->GetElementMaterial()->GetMappingMode();
            }
            if (materialIndices && materialMappingMode == FbxGeometryElement::eByPolygon)
                localMaterialIndex = materialIndices->GetAt(polygonIndex);
            FbxSurfaceMaterial* material = owner->GetMaterial(localMaterialIndex);

            // Generate a new submesh and insert the material name if this is the first time we see this material
            size_t nameHash = strHasher(material->GetName());
            if (subMeshByMaterial.find(nameHash) == subMeshByMaterial.end()) {
                materialNamesHashToIndex[nameHash] = materialNames.size();
                materialNames.push_back(material->GetName());
                
                subMeshByMaterial[nameHash] = {};
                vertexMaps[nameHash] = {};
            }

            // Get the submesh to write into
            ManagedMeshData& subMesh = subMeshByMaterial.find(nameHash)->second;

            // Get the vertex hash -> vertex index map for this submesh
            std::unordered_map<size_t, uint32_t>& vertexIndices = vertexMaps.find(nameHash)->second;

            // For polygons with more than 1 vertex we will track the first 
            // and previous vertex index so we can generate triangle fans.
            // TODO: Use earcut library instead?
            uint32_t anchor;
            uint32_t prev;

            // Read the vertices for this polygon
            for (size_t vertexIndex = 0; vertexIndex < polygonVertexCount; ++vertexIndex) {
                // This will fully overwrite the vertexBuffer with data for the current globalVertexIndex
                getVertex(mesh, polygonIndex, vertexIndex, globalVertexIndex, vertexBuffer, skin.orderedSkinWeights);

                // Hash the vertex and insert it if it is unique
                size_t hash = hasher(view);
                uint32_t index;
                auto it = vertexIndices.find(hash);
                if (it == vertexIndices.end()) {
                    index = (uint32_t)(subMesh.vertexData.size() / stride);
                    vertexIndices[hash] = index;
                    subMesh.vertexData.insert(subMesh.vertexData.end(), vertexBuffer.binaryArray.begin(), vertexBuffer.binaryArray.end());
                }  else {
                    // Else just reuse the existing vertex
                    index = it->second;
                }

                // Add the index
                subMesh.indexData.push_back(index);

                // Triangle-fan polygons with more than 3 vertices
                if (vertexIndex > 2) {
                    subMesh.indexData.push_back(prev);
                    subMesh.indexData.push_back(anchor);
                }

                ++globalVertexIndex;
                prev = index;
                if (vertexIndex == 0)
                    anchor = index;
            }
        }

        return {
            TT_FBX::makeString("1"),
            TT_FBX::makeString(mesh->GetName()),
            (uint32_t)materialNames.size(),
            makeStringList(materialNames),
            (uint32_t)uvSetNames.size(),
            makeStringList(uvSetNames),
            (uint32_t)layout.size(),
            TT_FBX::flattenList(layout),
            0x0004, // GL_TRIANGLES
            sizeof(uint32_t),
            (uint32_t)subMeshByMaterial.size(),
            flattenValues(subMeshByMaterial),
            (uint32_t)skin.jointIdToNodeMap.size(),
            TT_FBX::flattenList(skin.jointIdToNodeMap)
        };
    }
}

extern "C" {
    __declspec(dllexport) MultiMeshData* extractMeshes(const FbxImportContext* context, uint32_t* outCount) {
        if (!TT_FBX::checkContext(context)) {
            *outCount = 0;
            return nullptr;
        }

        std::vector<MultiMeshData> result;

        for (int i = 0; i < context->info->transforms.GetCount(); ++i) {
            FbxNode* node = context->info->transforms[i];
            if (node->GetNodeAttribute() && node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eMesh) {
                result.push_back(extractMesh((FbxMesh*)node->GetNodeAttribute(), context->info->transforms));
            }
        }

        *outCount = (uint32_t)result.size();
        return TT_FBX::flattenList(result);
    }

    __declspec(dllexport) void freeMeshes(const MultiMeshData* meshes, uint32_t meshCount) {
        for (unsigned int i = 0; i < meshCount; ++i) {
            delete[] meshes[i].version.buffer;
            delete[] meshes[i].name.buffer;

            for (unsigned int j = 0; j < meshes[i].materialNameCount; ++j)
                delete[] meshes[i].materialNames[j].buffer;
            delete[] meshes[i].materialNames;

            for (unsigned int j = 0; j < meshes[i].uvSetNameCount; ++j)
                delete[] meshes[i].uvSetNames[j].buffer;
            delete[] meshes[i].uvSetNames;

            delete[] meshes[i].attributeLayout;

            for (unsigned int j = 0; j < meshes[i].meshCount; ++j) {
                delete[] meshes[i].meshes[j].vertexDataBlob;
                delete[] meshes[i].meshes[j].indexDataBlob;
            }
            delete[] meshes[i].meshes;
        }
        delete[] meshes;
    }
}
