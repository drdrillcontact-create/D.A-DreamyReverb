#!/usr/bin/env python3
"""On Windows, compare resources in a relocated EXE with the built VST3 files."""
import argparse
import ctypes
from ctypes import wintypes
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('exe', type=Path)
parser.add_argument('bundle', type=Path)
args = parser.parse_args()

kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.LoadLibraryExW.argtypes = (wintypes.LPCWSTR, wintypes.HANDLE, wintypes.DWORD)
kernel.LoadLibraryExW.restype = wintypes.HMODULE
kernel.FindResourceW.argtypes = (wintypes.HMODULE, ctypes.c_void_p, ctypes.c_void_p)
kernel.FindResourceW.restype = wintypes.HANDLE
kernel.LoadResource.argtypes = (wintypes.HMODULE, wintypes.HANDLE)
kernel.LoadResource.restype = wintypes.HANDLE
kernel.LockResource.argtypes = (wintypes.HANDLE,)
kernel.LockResource.restype = ctypes.c_void_p
kernel.SizeofResource.argtypes = (wintypes.HMODULE, wintypes.HANDLE)
kernel.SizeofResource.restype = wintypes.DWORD
kernel.FreeLibrary.argtypes = (wintypes.HMODULE,)
module = kernel.LoadLibraryExW(str(args.exe.resolve()), None, 0x00000002)  # LOAD_LIBRARY_AS_DATAFILE
if not module:
    raise ctypes.WinError(ctypes.get_last_error())
try:
    files = sorted(p for p in args.bundle.resolve().rglob('*') if p.is_file())
    if not files:
        raise RuntimeError('No source files to compare')
    for resource_id, path in enumerate(files, 101):
        resource = kernel.FindResourceW(module, resource_id, 10)  # RT_RCDATA
        if not resource:
            raise ctypes.WinError(ctypes.get_last_error())
        size = kernel.SizeofResource(module, resource)
        data = kernel.LockResource(kernel.LoadResource(module, resource))
        if not data or ctypes.string_at(data, size) != path.read_bytes():
            raise RuntimeError(f'Embedded payload differs from {path}')
    print(f'Verified {len(files)} embedded VST3 files in isolated installer')
finally:
    kernel.FreeLibrary(module)
