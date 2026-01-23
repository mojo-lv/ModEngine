#include "pch.h"
#include "ThirdParty/minHook/include/MinHook.h"
#include "MemoryPatch/MemoryPatch.h"
#include "DebugMenu.h"

constexpr uintptr_t HOOK_KEY_MAPPING_ADDR = 0x142600d50;
constexpr uintptr_t HOOK_DEBUG_MENU_ADDR = 0x14262d186;

std::vector<MenuEntry> g_menuList;
FontConfig g_fontConfig;

static const std::unordered_map<size_t, size_t> unknown_to_key = {
    {0x36, 0x74}, // up         V
    {0x37, 0x75}, // down       B
    {0x38, 0x76}, // left       N
    {0x39, 0x77}, // right      M
    {0x3b, 0x47}, // X          1
    {0x3c, 0x57}, // B          E
    {0x3d, 0x55}, // A          Q
    {0x3e, 0x48}, // Y          2
    {0x3f, 0},    // LB         mouse left
    {0x40, 1},    // RB         mouse right
    {0x41, 0x71}, // LT         Z
    {0x42, 0x72}  // RT         X
};

typedef size_t(*t_KeyMapping)(void*, size_t, size_t, size_t);
static t_KeyMapping fpKeyMapping = nullptr;

size_t HookedKeyMapping(void* arg1, size_t arg2, size_t arg3, size_t arg4)
{
    if (arg2 > 0x25e && arg2 < 0x26b) {
        auto it = unknown_to_key.find(arg3);
        if(it != unknown_to_key.end()) {
            return fpKeyMapping(arg1, arg2, it->second, arg4);
        }
    }

    return fpKeyMapping(arg1, arg2, arg3, arg4);
}

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

    if (GetPrivateProfileIntW(L"debug_menu", L"key_remap", 0, L".\\mod_engine.ini") != 0) {
        if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_KEY_MAPPING_ADDR), &HookedKeyMapping,
                            reinterpret_cast<LPVOID*>(&fpKeyMapping)) == MH_OK) {
            MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_KEY_MAPPING_ADDR));
        }
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_DEBUG_MENU_ADDR), &HookedDebugMenu, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_DEBUG_MENU_ADDR));
        PatchDebugMenu(HOOK_DEBUG_MENU_ADDR);
    }
}
