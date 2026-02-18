#include "pch.h"
#include "FilesMod/FilesMod.h"
#include "InputProcess/InputProcess.h"
#include "MemoryPatch/MemoryPatch.h"
#include "DebugMenu/DebugMenu.h"
#include "DebugMenu/Graphics.h"
#include "D3D11Hook/D3D11Hook.h"

std::vector<HMODULE> g_LoadedDLLs;
INIReader g_INI("./mod_engine.ini");

static void OnAttach()
{
    FILE *stream;
    if (g_INI.GetBoolean("logs", "console", false)) {
        AllocConsole();
        freopen_s(&stream, "CONOUT$", "w", stdout);
    } else {
        freopen_s(&stream, "./mod_engine.log", "w", stdout);
    }

    MH_Initialize();

    ApplyFilesMod();
    EnableInputProcess();
    ApplyMemoryPatch();

    if (g_INI.GetBoolean("debug_menu", "enable", false)) {
        EnableDebugMenu();
        CreateThread(NULL, 0, ApplyD3D11Hook, NULL, NULL, NULL);
    }

    MH_EnableHook(MH_ALL_HOOKS);
}

static void OnDetach()
{
    for (auto dll : g_LoadedDLLs) {
        if (dll) {
            FreeLibrary(dll);
        }
    }

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    ShutdownImGui();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            if (g_INI.ParseError() < 0) {
                FILE *stream;
                freopen_s(&stream, "./mod_engine.log", "w", stdout);
                std::cout << "Can't load 'mod_engine.ini'" << std::endl;
                return FALSE;
            }

            DisableThreadLibraryCalls(hModule);
            OnAttach();
            break;
        case DLL_PROCESS_DETACH:
            OnDetach();
            break;
    }
    return TRUE;
}
