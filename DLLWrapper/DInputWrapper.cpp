#include "pch.h"
#include "DInputWrapper.h"
#include <mutex>

// Function pointer for the original, real System Function
static t_DirectInput8Create fpOriginal = nullptr;

static std::once_flag g_init_flag;

static void GetOriginalFunction()
{
    wchar_t dllPath[MAX_PATH];
    GetSystemDirectoryW(dllPath, MAX_PATH);
    lstrcatW(dllPath, L"\\dinput8.dll");
    HMODULE hMod = LoadLibraryW(dllPath);
    if (hMod) {
        fpOriginal = (t_DirectInput8Create)GetProcAddress(hMod, "DirectInput8Create");
    }
}

// The main export that is called by the game.
HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter)
{
    std::call_once(g_init_flag, GetOriginalFunction);
    if (fpOriginal) {
        return fpOriginal(hinst, dwVersion, riidltf, ppvOut, punkOuter);
    }
    return E_FAIL;
}
