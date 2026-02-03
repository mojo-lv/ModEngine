#include "pch.h"
#include "FilesMod/FilesMod.h"
#include "InputProcess/InputProcess.h"
#include "MemoryPatch/MemoryPatch.h"
#include "DebugMenu/DebugMenu.h"
#include "DebugMenu/Graphics.h"
#include "D3D11Hook/D3D11Hook.h"

std::vector<HMODULE> g_LoadedDLLs;
bool g_log_debug_menu = false;
bool g_log_key_remap = false;

static void OnAttach()
{
    if (GetPrivateProfileIntW(L"logs", L"debug_menu", 0, L".\\mod_engine.ini") != 0) {
        g_log_debug_menu = true;
    }

    if (GetPrivateProfileIntW(L"logs", L"key_remap", 0, L".\\mod_engine.ini") != 0) {
        g_log_key_remap= true;
    }

    if (g_log_debug_menu || g_log_key_remap) {
        FILE *stream;
        freopen_s(&stream, ".\\mod_engine.log", "w", stdout);
    }

    MH_Initialize();

    ApplyFilesMod();
    EnableInputProcess();
    ApplyMemoryPatch();

    if (GetPrivateProfileIntW(L"debug_menu", L"enable", 0, L".\\mod_engine.ini") != 0) {
        EnableDebugMenu();
        CreateThread(NULL, 0, ApplyD3D11Hook, NULL, NULL, NULL);
    }

    MH_EnableHook(MH_ALL_HOOKS);
}

static void OnDetach()
{
    for (auto h : g_LoadedDLLs) {
        if (h) {
            FreeLibrary(h);
        }
    }
    g_LoadedDLLs.clear();

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    ShutdownImGui();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            OnAttach();
            break;
        case DLL_PROCESS_DETACH:
            OnDetach();
            break;
    }
    return TRUE;
}
