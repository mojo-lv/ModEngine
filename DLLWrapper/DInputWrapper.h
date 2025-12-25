#pragma once

#include <windows.h>

// Function pointer type for the original
typedef HRESULT(WINAPI *t_DirectInput8Create)(
    HINSTANCE hinst,
    DWORD dwVersion,
    REFIID riidltf,
    LPVOID *ppvOut,
    LPUNKNOWN punkOuter
);

extern std::vector<HMODULE> g_LoadedDLLs;
