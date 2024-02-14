import os
import ctypes
from tt_fbx.fbx.dataModel import FbxImportContext, AnimationChannels, MultiMeshData, Node


def initialize():
    dll = ctypes.CDLL(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'fbx', 'x64', 'Release', 'fbx.dll'))

    dll.importFbx.argtypes = (ctypes.c_char_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int)
    dll.importFbx.restype = ctypes.POINTER(FbxImportContext)
    dll.freeFbx.argtypes = (ctypes.POINTER(FbxImportContext),)
    dll.freeFbx.restype = None

    dll.extractNodes.argtypes = (ctypes.POINTER(FbxImportContext), ctypes.POINTER(ctypes.c_uint32))
    dll.extractNodes.restype = ctypes.POINTER(Node)
    dll.freeNodes.argtypes = (ctypes.POINTER(Node), ctypes.c_uint32)
    dll.freeNodes.restype = None

    dll.extractTakes.argtypes = (ctypes.POINTER(FbxImportContext), ctypes.c_double, ctypes.POINTER(ctypes.c_uint32))
    dll.extractTakes.restype = ctypes.POINTER(AnimationChannels)
    dll.freeTakes.argtypes = (ctypes.POINTER(AnimationChannels), ctypes.c_uint32)
    dll.freeTakes.restype = None

    dll.extractMeshes.argtypes = (ctypes.POINTER(FbxImportContext), ctypes.POINTER(ctypes.c_uint32))
    dll.extractMeshes.restype = ctypes.POINTER(MultiMeshData)
    dll.freeMeshes.argtypes = (ctypes.POINTER(MultiMeshData), ctypes.c_uint32)
    dll.freeMeshes.restype = None

    return dll
