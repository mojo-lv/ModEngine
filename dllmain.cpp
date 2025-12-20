#include "pch.h"
#include "MemoryPatcher/MemoryPatcher.h"
#include "LibraryLoader/LibraryLoader.h"
#include "ModLoader/ModLoader.h"
#include "MyTest/MyTest.h"

static void Initialize()
{
    if (GetPrivateProfileIntW(L"enable", L"memory", 0, L".\\modengine.ini") == 1) {
        PatchMemory();
    }

    if (GetPrivateProfileIntW(L"enable", L"dlls", 0, L".\\modengine.ini") == 1) {
        LoadDlls();
    }

    if (GetPrivateProfileIntW(L"enable", L"mods", 0, L".\\modengine.ini") == 1) {
        LoadMods();
    }

    if (GetPrivateProfileIntW(L"enable", L"test", 0, L".\\modengine.ini") == 1) {
        Test();
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            Initialize();
            break;
        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
