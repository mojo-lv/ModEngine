#include "pch.h"
#include "MinHook/MinHook.h"
#include "MemoryPatcher/MemoryPatcher.h"
#include "ModLoader/ModLoader.h"
#include "MyTest/MyTest.h"

std::vector<HMODULE> g_LoadedDLLs;

bool g_test = true;

static void OnAttach()
{
    if (g_test) {
        FILE *stream;
        freopen_s(&stream, ".\\mod_engine.log", "w", stdout);
    }
    PatchMemory();
    MH_Initialize();
    LoadModFiles();
    if (g_test) {
        Test();
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
