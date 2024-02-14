import ctypes
from enum import IntEnum


# Fbx sdk enums
class UpVector(IntEnum):
    X = 1
    Y = 2
    Z = 3


class FrontVector(IntEnum):
    ParityEven = 1
    ParityOdd = 2


class CoordSystem(IntEnum):
    RighHanded = 0
    LeftHanded = 1


# API enums
class ErrorCode(IntEnum):
    OK = 0
    WARNING = 1
    MANAGER_CREATE_FAILED = 2
    SCENE_CREATE_FAILED = 3
    SCENE_IMPORT_FAILED = 4
    INVALID_ARGUMENT = 5
    TRIANGULATION_FAILED = 6


class Units(IntEnum):
    mm = 0
    dm = 1
    cm = 2
    m = 3
    km = 4
    Inch = 5
    Foot = 6
    Mile = 7
    Yard = 8


class ScenePatchFlags(IntEnum):
    ConvertNurbsToPolygons = 1 << 0
    Triangulate = 1 << 1
    RemoveBadPolygons = 1 << 2
    CollapseMeshes = 1 << 3
    SplitMeshesPerMaterial = 1 << 4
    CenterScene = 1 << 5


class ChannelIdentifier(IntEnum):
    Invalid = 0
    TranslateX = 1
    TranslateY = 2
    TranslateZ = 3
    RotateX = 4
    RotateY = 5
    RotateZ = 6
    ScaleX = 7
    ScaleY = 8
    ScaleZ = 9


class String(ctypes.Structure):
    _fields_ = [
        ("length", ctypes.c_uint32),
        ("buffer", ctypes.c_char_p),
    ]


class Node(ctypes.Structure):
    _fields_ = [
        ("name", String),
        ("translateX", ctypes.c_double),
        ("translateY", ctypes.c_double),
        ("translateZ", ctypes.c_double),
        ("rotateX", ctypes.c_double),
        ("rotateY", ctypes.c_double),
        ("rotateZ", ctypes.c_double),
        ("scaleX", ctypes.c_double),
        ("scaleY", ctypes.c_double),
        ("scaleZ", ctypes.c_double),
        ("rotateOrder", ctypes.c_int32),
        ("parentIndex", ctypes.c_int32),
        ("meshIndex", ctypes.c_int32),
    ]


class AnimationChannel(ctypes.Structure):
    _fields_ = [
        ("nodeId", ctypes.c_int32),
        ("targetChannel", ctypes.c_int),
        ("size", ctypes.c_uint32),
        ("data", ctypes.POINTER(ctypes.c_double)),
    ]


class AnimationChannels(ctypes.Structure):
    _fields_ = [
        ("length", ctypes.c_uint32),
        ("channels", ctypes.POINTER(AnimationChannel)),
    ]


class VertexAttribute(ctypes.Structure):
    _fields_ = [
        ("semantic", ctypes.c_uint8),
        ("numElements", ctypes.c_uint8),
        ("elementType", ctypes.c_uint32),
    ]


class MeshData(ctypes.Structure):
    _fields_ = [
        ("materialId", ctypes.c_uint32),
        ("vertexDataSizeInBytes", ctypes.c_uint32),
        ("indexDataSizeInBytes", ctypes.c_uint32),
        ("vertexDataBlob", ctypes.c_void_p),
        ("indexDataBlob", ctypes.c_void_p),
    ]


class MultiMeshData(ctypes.Structure):
    _fields_ = [
        ("version", String),
        ("name", String),
        ("materialNameCount", ctypes.c_uint32),
        ("materialNames", ctypes.POINTER(String)),
        ("uvSetNameCount", ctypes.c_uint32),
        ("uvSetNames", ctypes.POINTER(String)),
        ("attributeCount", ctypes.c_uint32),
        ("attributeLayout", ctypes.POINTER(VertexAttribute)),
        ("primitiveType", ctypes.c_uint32),
        ("indexElementSizeInBytes", ctypes.c_uint8),
        ("meshCount", ctypes.c_uint32),
        ("meshes", ctypes.POINTER(MeshData)),
        ("jointCount", ctypes.c_uint32),
        ("jointIds", ctypes.POINTER(ctypes.c_uint32)),
    ]


class FbxImportContext(ctypes.Structure):
    _fields_ = [
        # These void pointers are internal FBX importer state.
        ("manager", ctypes.c_void_p),
        ("scene", ctypes.c_void_p),
        ("errorCode", ctypes.c_int),
        ("errorMessage", String),
        ("info", ctypes.c_void_p),
    ]
