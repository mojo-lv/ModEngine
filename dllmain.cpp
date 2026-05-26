#include "pch.h"
#include "FilesMod/FilesMod.h"
#include "MemoryPatch/MemoryPatch.h"
#include "DebugMenu/DebugMenu.h"
#include "DebugMenu/Graphics.h"
#include "D3D11Hook/D3D11Hook.h"
#include "InputProcess/KeyRemap.h"
#include "InputProcess/NpcAnimChange.h"
#include "InputProcess/PlayerSkillChange.h"

std::vector<HMODULE> g_LoadedDLLs;
INIReader g_INI("mod_engine.ini");

typedef int64_t(*t_SteamAPI_Init)();
t_SteamAPI_Init fpSteamInit = nullptr;

static void ApplyPostUnpackHooks()
{
    if (g_INI.GetBoolean("debug_menu", "enable", false)) {
        EnableDebugMenu();
        CreateThread(NULL, 0, ApplyD3D11Hook, NULL, NULL, NULL);
    }

    ApplyFilesMod();
    EnableKeyRemap();
    EnableNpcAnimChange();
    EnablePlayerSkillChange();
    ApplyMemoryPatch();

    MH_EnableHook(MH_ALL_HOOKS);
}

int64_t Hook_SteamAPI_Init()
{
    ApplyPostUnpackHooks();
    return fpSteamInit();
}

static void OnAttach()
{
    MH_Initialize();
    auto steamApiHwnd = GetModuleHandleW(L"steam_api64.dll");
    auto initAddr = GetProcAddress(steamApiHwnd, "SteamAPI_Init");
    MH_CreateHook(initAddr, &Hook_SteamAPI_Init, reinterpret_cast<LPVOID*>(&fpSteamInit));
    MH_EnableHook(initAddr);
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

static bool LoadConfig(HMODULE hModule) {
    int error = g_INI.ParseError();
    if (error == 0) {
        return true;
    } else if (error > 0) {
        return false;
    }

    char path[MAX_PATH] = {0};
    GetModuleFileNameA(hModule, path, MAX_PATH);
    if (char* lastSlash = strrchr(path, '\\')) *(lastSlash + 1) = '\0';
    g_INI = INIReader(std::string(path) + "mod_engine.ini");
    return !g_INI.ParseError();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);

            FILE *stream;
            if (!LoadConfig(hModule)) {
                freopen_s(&stream, "mod_engine.log", "w", stdout);
                std::cout << "Can't load 'mod_engine.ini'" << std::endl;
                std::cout << g_INI.ParseErrorMessage() << std::endl;
                return FALSE;
            }

            if (g_INI.GetBoolean("logs", "console", false)) {
                AllocConsole();
                freopen_s(&stream, "CONOUT$", "w", stdout);
            } else {
                freopen_s(&stream, "mod_engine.log", "w", stdout);
            }

            OnAttach();
            break;
        case DLL_PROCESS_DETACH:
            OnDetach();
            break;
    }
    return TRUE;
}
