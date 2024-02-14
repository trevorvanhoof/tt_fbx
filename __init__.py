import ctypes
import json
from typing import *
import os

from tt_fbx.cgmath import Vec4
from tt_fbx.constants import appDir
from tt_fbx.ioUtils.binaryIO import BinaryWriter
from tt_fbx.scene.scene import Transform, Scene

from tt_fbx.fbx import api
from tt_fbx.fbx.dataModel import *

_dll = api.initialize()


def _extractScene(filePath: str, upVector: UpVector, frontVector: FrontVector, coordSystem: CoordSystem, units: Units):
    ptr = _dll.importFbx(ctypes.create_string_buffer(filePath.encode('utf-8')), upVector, frontVector, coordSystem, units)
    context: FbxImportContext = ptr.contents

    if context.errorCode not in (ErrorCode.OK, ErrorCode.WARNING):
        raise RuntimeError(ErrorCode(context.errorCode).name, context.errorMessage.buffer[:context.errorMessage.length])

    nodeCount = ctypes.c_uint32()
    nodes = _dll.extractNodes(context, ctypes.byref(nodeCount))

    takeCount = ctypes.c_uint32()
    takes = _dll.extractTakes(context, 60.0, ctypes.byref(takeCount))

    meshCount = ctypes.c_uint32()
    meshes = _dll.extractMeshes(context, ctypes.byref(meshCount))

    # Clean up the FbxScene and FbxManager
    _dll.freeFbx(context)

    return nodes, nodeCount, takes, takeCount, meshes, meshCount


class _StubModel:
    """
    Binds a sub-mesh and a material together.
    """

    def __init__(self, meshFilePath: str, meshIndex: int):
        self._result = (meshFilePath, meshIndex), ''

    def serialize(self, *_) -> Tuple[Tuple[str, int], str]:
        return self._result


def _saveNodes(scenePath: str, nodes, nodeCount: ctypes.c_uint32, multiMeshOffsetCount: List[Tuple[int, int]], relMeshPath: str) -> Dict[int, int]:
    s = Scene()
    originalIndex: Dict[Transform, int] = {}
    for nodeIndex in range(nodeCount.value):
        node: Node = nodes[nodeIndex]
        t = Transform()
        t.name = node.name.buffer[:node.name.length].decode('utf-8')
        t.translate = Vec4(node.translateX, node.translateY, node.translateZ, 1.0)
        t.rotate = Vec4(node.rotateX, node.rotateY, node.rotateZ, 1.0)
        t.scale = Vec4(node.scaleX, node.scaleY, node.scaleZ, 1.0)
        t.rotateOrder = node.rotateOrder
        if node.parentIndex != -1:
            t.setParent(s.allTransforms[node.parentIndex])
        else:
            s.rootTransforms.append(t)
        if node.meshIndex != -1:
            offset, count = multiMeshOffsetCount[node.meshIndex]
            t.models = [_StubModel(relMeshPath, i) for i in range(offset, offset + count)]
        s.allTransforms.append(t)
        originalIndex[t] = nodeIndex

    with open(scenePath, 'w') as fh:
        # Because we use the StubModel we don't need a context object and can pass in None instead
        json.dump(s.serialize(None), fh, indent=2)

    # The scene serializes depth-first, where the FBX api returns the scene breadth-first,
    # so we must remap the joint and animation node IDs.
    indexRemap: Dict[int, int] = {}
    for index, t in enumerate(s.recurse()):
        indexRemap[originalIndex[t]] = index
    return indexRemap


def _saveTakes(animPath: str, takes, takeCount: ctypes.c_uint32, indexRemap: Dict[int, int]):
    # TODO: This file format is not well thought out yet
    with BinaryWriter(animPath) as fh:
        fh.u32(takeCount.value)
        for takeIndex in range(takeCount.value):
            take: AnimationChannels = takes[takeIndex]
            fh.u32(take.length)
            for channelIndex in range(take.length):
                channel: AnimationChannel = take.channels[channelIndex]
                # if transforms[channel.nodeId].name == 'ikHandle1':
                #    print(channel.targetChannel, channel.data[0])
                fh.u32(indexRemap[channel.nodeId])
                fh.u8(channel.targetChannel)
                fh.u32(channel.size)
                fh.f64array(channel.data, channel.size)


def _mergeMaterials(meshes, meshCount: ctypes.c_uint32, indexRemap: Dict[int, int]):
    # Collect material names
    materialNames: List[str] = []

    # Return remapped joint indices per sub-mesh
    meshJointInfo: List[List[int]] = []

    for multiMeshIndex in range(meshCount.value):
        multiMesh: MultiMeshData = meshes[multiMeshIndex]

        # Collect unique material names and track at what  index in the merged material list they live
        materialIndexMap: Dict[int, int] = {}
        for materialIndex in range(multiMesh.materialNameCount):
            materialName: str = multiMesh.materialNames[materialIndex].buffer[:multiMesh.materialNames[materialIndex].length].decode('utf-8')
            if materialName not in materialNames:
                materialIndexMap[materialIndex] = len(materialNames)
                materialNames.append(materialName)
            else:
                materialIndexMap[materialIndex] = materialNames.index(materialName)

        # Patch the sub-mesh materialId to match the merged output
        for meshIndex in range(multiMesh.meshCount):
            mesh: MeshData = multiMesh.meshes[meshIndex]
            mesh.materialId = materialIndexMap[mesh.materialId]

            # Patch joint indices to match the output scene transform indices
            meshJointInfo.append([])
            for jointIndex in range(multiMesh.jointCount):
                meshJointInfo[-1].append(indexRemap[multiMesh.jointIds[jointIndex]])

    return materialNames, meshJointInfo


# This map helps compress the vertex attribute layout
# to avoid exceeding GL_MAX_VERTEX_ATTRIBS, which tends to be 16.
# It will error if there are too many attributes of any given type.
_semanticMapping = [
    0, 1, 2, 3, 4,
    5, 6, None, None, None, None, None, None,  # N
    7, 8, None, None, None, None, None, None,  # T
    9, 10, None, None, None, None, None, None,  # B
    11, 12, None, None, None, None, None, None,  # UV
    13, 14, 15,  # Cd
]


def _writeSubMesh(fh: BinaryWriter, multiMesh: MultiMeshData, meshIndex: int, meshJointInfo: List[int]):
    mesh: MeshData = multiMesh.meshes[meshIndex]

    # Name
    name: bytes = multiMesh.name.buffer[:multiMesh.name.length]
    fh.byteString(name + str(meshIndex).encode('utf-8'))

    # Material id
    fh.u32(mesh.materialId)

    # Attrib layout
    fh.u8(multiMesh.attributeCount)
    for attributeIndex in range(multiMesh.attributeCount):
        attribute = multiMesh.attributeLayout[attributeIndex]
        fh.u8(_semanticMapping[attribute.semantic])
        fh.u8(attribute.numElements)
        fh.u32(attribute.elementType)

    # Prim type
    fh.u32(multiMesh.primitiveType)

    # Blob sizes
    fh.u32(mesh.vertexDataSizeInBytes)
    fh.u32(mesh.indexDataSizeInBytes)
    if mesh.indexDataSizeInBytes:
        fh.u8(multiMesh.indexElementSizeInBytes)

    # Blobs
    buf = ctypes.cast(mesh.vertexDataBlob, ctypes.POINTER(ctypes.c_uint8 * mesh.vertexDataSizeInBytes)).contents
    fh.write(buf)

    if mesh.indexDataSizeInBytes:
        buf = ctypes.cast(mesh.indexDataBlob, ctypes.POINTER(ctypes.c_uint8 * mesh.indexDataSizeInBytes)).contents
        fh.write(buf)

    # Joints
    fh.u32(len(meshJointInfo))
    for jointId in meshJointInfo:
        fh.u32(jointId)


def _saveMeshes(meshPath: str, meshes, meshCount: ctypes.c_uint32, totalMeshCount: int, indexRemap: Dict[int, int]):
    # Pre-process
    materialNames, meshJointInfo = _mergeMaterials(meshes, meshCount, indexRemap)

    with BinaryWriter(meshPath) as fh:
        # Version
        fh.string('1')

        # Material name count
        fh.u32(len(materialNames))
        # Material names
        for materialName in materialNames:
            fh.string(materialName)

        # Mesh count
        fh.u32(totalMeshCount)
        # Meshes
        meshCursor = 0
        for multiMeshIndex in range(meshCount.value):
            multiMesh: MultiMeshData = meshes[multiMeshIndex]
            for meshIndex in range(multiMesh.meshCount):
                _writeSubMesh(fh, multiMesh, meshIndex, meshJointInfo[meshCursor])
                meshCursor += 1


def convert(filePath: str, upVector: UpVector = UpVector.Y, frontVector: FrontVector = FrontVector.ParityEven, coordSystem: CoordSystem = CoordSystem.LeftHanded, units: Units = Units.m):
    nodes, nodeCount, takes, takeCount, meshes, meshCount = _extractScene(filePath, upVector, frontVector, coordSystem, units)

    # We will combine all meshes into one multi-mesh.
    # Track what FBX mesh ID maps to what range of final output meshes.
    totalMeshCount = 0
    multiMeshOffsetCount:List[Tuple[int, int]] = []
    for multiMeshIndex in range(meshCount.value):
        multiMesh: MultiMeshData = meshes[multiMeshIndex]
        multiMeshOffsetCount.append((totalMeshCount, multiMesh.meshCount))
        totalMeshCount += multiMesh.meshCount

    # Save the scene & model info
    meshPath = os.path.splitext(filePath)[0] + '.mesh'
    relMeshPath = os.path.relpath(meshPath, appDir).replace('\\', '/')
    scenePath = os.path.splitext(filePath)[0] + '.scene'
    indexRemap = _saveNodes(scenePath, nodes, nodeCount, multiMeshOffsetCount, relMeshPath)

    # Save the animation data
    animPath = os.path.splitext(filePath)[0] + '.anim'
    _saveTakes(animPath, takes, takeCount, indexRemap)

    # Collapse all the meshes into a single file
    _saveMeshes(meshPath, meshes, meshCount, totalMeshCount, indexRemap)

    _dll.freeTakes(takes, takeCount)
    _dll.freeNodes(nodes, nodeCount)
    _dll.freeMeshes(meshes, meshCount)


if __name__ == '__main__':
    # convert(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'unit_cube.fbx'), units=Units.cm)
    # Maya:
    # convert(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fbtest.fbx'),
    #        frontVector=FrontVector.ParityOdd,
    #        coordSystem=CoordSystem.RighHanded,
    #        units=Units.cm)
    # Blender:
    convert(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'mushroom_fella.fbx'),
            upVector=UpVector.Y,
            frontVector=FrontVector.ParityOdd,
            coordSystem=CoordSystem.RighHanded)
    """
    from PySide6.QtCore import *
    from PySide6.QtGui import *
    from PySide6.QtWidgets import *
    from PySide6.QtOpenGLWidgets import *

    a = QApplication([])


    class T(QOpenGLWidget):
        def initializeGL(self):
            from cubedMelon.renderPrimitives.mesh import Mesh
            Mesh.load(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'unit_cube.mesh'))


    w = T()
    w.show()
    """
