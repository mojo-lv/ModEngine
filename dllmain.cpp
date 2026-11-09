#include "pch.h"
#include "FilesMod/FilesMod.h"
#include "MemoryPatch/MemoryPatch.h"
#include "DebugMenu/DebugMenu.h"
#include "DebugMenu/Graphics.h"
#include "D3D11Hook/D3D11Hook.h"
#include "InputProcess/KeyRemap.h"
#include "InputProcess/NpcAnimChange.h"
#include "InputProcess/PlayerSkillChange.h"
#include "Misc/Misc.h"

typedef HRESULT(WINAPI *t_DirectInput8Create)(
    HINSTANCE hinst,
    DWORD dwVersion,
    REFIID riidltf,
    LPVOID *ppvOut,
    LPUNKNOWN punkOuter
);
static t_DirectInput8Create fpDInput8Create = nullptr;

typedef int64_t(*t_SteamAPI_Init)();
static t_SteamAPI_Init fpSteamInit = nullptr;

static fs::path g_CurPath;
static std::vector<HMODULE> g_LoadedDLLs;
static INIReader g_INI("mod_engine.ini");

static void ApplyPostUnpackHooks()
{
    if (g_INI.GetBoolean("debug_menu", "enable", false)) EnableDebugMenu(g_INI, g_CurPath);
    if (g_INI.HasSection("files")) ApplyFilesMod(g_INI, g_CurPath);
    if (g_INI.HasSection("key_remap")) EnableKeyRemap(g_INI);
    if (g_INI.HasSection("npc_anim_change")) EnableNpcAnimChange(g_INI, g_CurPath);
    if (g_INI.HasSection("player_skill_change")) EnablePlayerSkillChange(g_INI, g_CurPath);
    if (g_INI.HasSection("memory")) ApplyMemoryPatch(g_INI);
    
    ApplyMisc(g_INI);
    MH_EnableHook(MH_ALL_HOOKS);
}

int64_t SteamAPI_Init()
{
    ApplyPostUnpackHooks();
    if (fpSteamInit) {
        return fpSteamInit();
    }
    return 0;
}

static void LoadDlls()
{
    wchar_t dllPath[MAX_PATH];
    GetSystemDirectoryW(dllPath, MAX_PATH);
    lstrcatW(dllPath, L"\\dinput8.dll");
    HMODULE hMod = LoadLibraryW(dllPath);
    if (hMod) {
        g_LoadedDLLs.push_back(hMod);
        fpDInput8Create = (t_DirectInput8Create)GetProcAddress(hMod, "DirectInput8Create");
    }

    std::string dlls = g_INI.GetString("files", "dlls", "");
    if (dlls.empty()) return;

    fs::path dllsDir(g_CurPath / dlls);
    if (!fs::exists(dllsDir) || !fs::is_directory(dllsDir)) return;

    for (const auto& p : fs::directory_iterator(dllsDir)) {
        if (!p.is_directory()) {
            wchar_t f = p.path().filename().wstring().front();
            if ((f != L'_') && (f != L'.')) {
                hMod = LoadLibraryW(p.path().wstring().c_str());
                if (hMod) g_LoadedDLLs.push_back(hMod);
            }
        }
    }
}

static void OnAttach()
{
    if (g_INI.GetBoolean("debug_menu", "enable", false)) {
        CreateThread(nullptr, 0, ApplyD3D11Hook, nullptr, 0, nullptr);
    }

    LoadDlls();

    LPVOID pTarget = nullptr;
    HMODULE hMod = GetModuleHandleW(L"steam_api64.dll");
    if (hMod) pTarget = GetProcAddress(hMod, "SteamAPI_Init");

    MH_Initialize();
    if (pTarget) {
        MH_CreateHook(pTarget, &SteamAPI_Init, reinterpret_cast<LPVOID*>(&fpSteamInit));
        MH_EnableHook(pTarget);
    } else {
        SteamAPI_Init();
    }
}

static void OnDetach()
{
    RestoreD3D11Hook();
    ShutdownImGui();

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    for (auto dll : g_LoadedDLLs) FreeLibrary(dll);
}

static int LoadConfig() {
    int error = g_INI.ParseError();
    if (error >= 0) {
        g_CurPath = fs::current_path();
        return error;
    }

    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (wchar_t* p = wcsrchr(path, L'\\')) *(p + 1) = '\0';
    g_CurPath = fs::path(path);
    g_INI = INIReader((g_CurPath / "mod_engine.ini").string());
    return g_INI.ParseError();
}

// The main export that is called by the game.
HRESULT WINAPI DirectInput8Create(
    HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut, LPUNKNOWN punkOuter)
{
    if (fpDInput8Create) {
        return fpDInput8Create(hinst, dwVersion, riidltf, ppvOut, punkOuter);
    }
    return E_FAIL;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);

            FILE *stream;
            if (LoadConfig()) {
                fs::path logPath(g_CurPath / "mod_engine.log");
                freopen_s(&stream, logPath.string().c_str(), "w", stdout);
                std::cout << "Can't load 'mod_engine.ini'" << std::endl;
                std::cout << g_INI.ParseErrorMessage() << std::endl;
                return FALSE;
            }

            if (g_INI.HasSection("logs")) {
                if (g_INI.GetBoolean("logs", "console", false)) {
                    AllocConsole();
                    freopen_s(&stream, "CONOUT$", "w", stdout);
                } else {
                    fs::path logPath(g_CurPath / "mod_engine.log");
                    if (fs::exists(logPath)) fs::resize_file(logPath, 0);
                    freopen_s(&stream, logPath.string().c_str(), "a", stdout);
                }
            }

            OnAttach();
            break;
        case DLL_PROCESS_DETACH:
            OnDetach();
            break;
    }
    return TRUE;
}
