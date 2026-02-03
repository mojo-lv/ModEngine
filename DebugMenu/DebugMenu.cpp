#include "pch.h"
#include "MemoryPatch/MemoryPatch.h"
#include "DebugMenu.h"

constexpr uintptr_t HOOK_DEBUG_MENU_ADDR = 0x14262d186;

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

    UINT rgb = GetPrivateProfileIntW(L"debug_menu", L"color", 0, L".\\mod_engine.ini");
    if (rgb != 0) {
        UINT r = (rgb >> 16) & 0xFF;
        UINT g = (rgb >> 8) & 0xFF;
        UINT b = rgb & 0xFF;
        UINT a = 0xFF;
        // 0xAABBGGRR
        g_fontConfig.color = (a << 24) | (b << 16) | (g << 8) | r;
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_DEBUG_MENU_ADDR), &HookedDebugMenu, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_DEBUG_MENU_ADDR));
        PatchDebugMenu(HOOK_DEBUG_MENU_ADDR);
    }
}
