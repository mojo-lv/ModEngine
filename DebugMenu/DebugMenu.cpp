#include "pch.h"
#include "ThirdParty/minHook/include/MinHook.h"
#include "MemoryPatch/MemoryPatch.h"
#include "DebugMenu.h"

#define OFFSET_HOOK_DEBUG_MENU 0x262d186

std::vector<MenuEntry> g_menuList;
FontConfig g_fontConfig;

static std::string WideCharToUTF8(const wchar_t* wstr)
{
    if (wstr != nullptr) {
        int wlen = static_cast<int>(wcslen(wstr));
        if (wlen != 0) {
            int size_needed = WideCharToMultiByte(
                CP_UTF8, 0, wstr, wlen,
                nullptr, 0, nullptr, nullptr);

            if (size_needed > 0) {
                std::string result(size_needed, 0);
                int converted_size = WideCharToMultiByte(
                    CP_UTF8, 0, wstr, wlen,
                    &result[0], size_needed, nullptr, nullptr);

                if (converted_size == size_needed) return result;
            }
        }
    }

    return "";
}

void HookedDebugMenu(void* qUnkClass, float* pLocation, wchar_t* pwString)
{
    MenuEntry entry;
    entry.fX = pLocation[0];
    entry.fY = pLocation[1];
    entry.text = WideCharToUTF8(pwString);
    g_menuList.push_back(entry);
}

void EnableDebugMenu()
{
    fs::path curPath = fs::current_path();
    WCHAR fontPath[MAX_PATH];
    GetPrivateProfileStringW(L"debug_menu", L"font_path", L"", fontPath, MAX_PATH, L".\\mod_engine.ini");
    if ((lstrlenW(fontPath) > 0) && fs::exists(curPath / fontPath)) {
        g_fontConfig.path = (curPath / fontPath).u8string();
    }

    UINT fontSize = GetPrivateProfileIntW(L"debug_menu", L"font_size", 0, L".\\mod_engine.ini");
    if (fontSize != 0) {
        g_fontConfig.size = (float)fontSize;
    }

    uintptr_t hookAddress = (uintptr_t)GetModuleHandle(NULL) + OFFSET_HOOK_DEBUG_MENU;
    if (MH_CreateHook(reinterpret_cast<LPVOID>(hookAddress), &HookedDebugMenu, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(hookAddress));
        PatchDebugMenu(hookAddress);
    }
}
