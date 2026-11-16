// Windows only: finds the plug-in files that were linked into the installer as a resource
// (see Installer/payload.rc.in). Kept apart from the rest so that <windows.h> and the JUCE
// headers never meet.

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <windows.h>
#include <cstddef>

extern "C" int daaura_findBuiltInPayload (const void** data, size_t* size);

extern "C" int daaura_findBuiltInPayload (const void** data, size_t* size)
{
    // (RT_RCDATA is resource type 10)
    const HRSRC found = FindResourceW (nullptr, L"DAAURA_PAYLOAD", MAKEINTRESOURCEW (10));

    if (found == nullptr)
        return 0;

    const HGLOBAL loaded = LoadResource (nullptr, found);

    if (loaded == nullptr)
        return 0;

    *data = LockResource (loaded);
    *size = static_cast<size_t> (SizeofResource (nullptr, found));
    return (*data != nullptr && *size > 0) ? 1 : 0;
}

#endif
