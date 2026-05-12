#include "pch.h"
#include "MemoryPatch/MemoryPatch.h"
#include "DebugMenu.h"

constexpr uintptr_t HOOK_DEBUG_MENU_ADDR = 0x14262d186;
constexpr uintptr_t HOOK_DEBUG_NPC_ADDR = 0x140614f13;
constexpr uintptr_t HOOK_NPC_DAMAGE_ADDR = 0x140b6a13d;
constexpr uintptr_t HOOK_DBG_CAM_ADDR = 0x14083bebf;

typedef uintptr_t(*t_addNPC)(uintptr_t, uintptr_t*);
static t_addNPC fpAddNPC = reinterpret_cast<t_addNPC>(0x140615c90);
static uintptr_t* pNPCList = reinterpret_cast<uintptr_t*>(0x143d547d8);
static uintptr_t* pWorldChrMan = reinterpret_cast<uintptr_t*>(0x143d7a1e0);
static uint8_t* freezePtr = reinterpret_cast<uint8_t*>(0x143d7acb2);

std::vector<MenuEntry> g_menuList;
int g_menuSelectedIndex = -1;
FontConfig g_fontConfig;
bool g_log_debug_menu = false;

extern INIReader g_INI;

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
    if (pwString != nullptr) {
        if ((pwString[0] == 0x25C6 || pwString[0] == 0x25C7) && pwString[1] == L'\0') {
            g_menuSelectedIndex = g_menuList.size();
        } else if ((pwString[0] == 0x25A0 || pwString[0] == 0x25A1) && pwString[1] == L'[') {
            g_menuSelectedIndex = g_menuList.size() - 1;
        }
    }
}

void HookedDebugNPC()
{
    uintptr_t* begin = *(uintptr_t**)(*pWorldChrMan + 0x3130);
    uintptr_t* end = *(uintptr_t**)(*pWorldChrMan + 0x3138);

    if (begin == nullptr || end == nullptr || begin >= end) return;

    for (size_t i = 0; i < end - begin; i++) {
        if (*(uint8_t*)(*(begin + i) + 0x1a15) & 1) {
            fpAddNPC(*pNPCList, begin + i);
        }
    }
}

uint16_t HookedNPCDamage(uint16_t arg1)
{
    if (arg1 == 0x101 || arg1 == 0) {
        return 0;
    }

    //std::cout << "[DebugMenu] Unknown Damage " << std::hex << arg1 << std::endl;
    return arg1;
}

int64_t HookedDbgCam(uintptr_t arg1)
{
    static uint32_t lastCamMode = 0;
    static uint8_t lastMask = 0;
    static uint8_t* maskPtr = nullptr;

    uint32_t camMode = *(uint32_t*)(arg1 + 0xe0);
    if ((camMode != 0) && (lastCamMode == 0)) {
        maskPtr = (uint8_t*)(*(uintptr_t*)(*(uintptr_t*)(*(uintptr_t*)(
                    *pWorldChrMan + 0x88) + 0x50) + 0x10) + 0x1f40);
    }

    if ((camMode == 1) != (lastCamMode == 1)) {
        *freezePtr = (camMode == 1) ? 1 : 2;
    }

    if ((camMode == 2) != (lastCamMode == 2)) {
        if (camMode == 2) {
            lastMask = *maskPtr & 0xE0;
            *maskPtr |= 0xE0; // No Move/Attack/Hit
        } else {
            *maskPtr = (*maskPtr & 0x1F) | lastMask;
        }
    } else if (camMode == 2) {
        uint8_t maskBits = *maskPtr & 0xE0;
        if (maskBits != 0xE0) {
            lastMask = 0;
            *maskPtr |= 0xE0;
        }
    }

    lastCamMode = camMode;
    return camMode - 1;
}

void EnableDebugMenu()
{
    g_log_debug_menu = g_INI.GetBoolean("logs", "debug_menu", false);
    std::string fontPathStr = g_INI.GetString("debug_menu", "font_path", "");
    ULONG fontSize = g_INI.GetUnsigned("debug_menu", "font_size", 0);
    ULONG color = g_INI.GetUnsigned("debug_menu", "color", 0);

    fs::path fontPath = fs::current_path() / fontPathStr;
    if (!fontPathStr.empty() && fs::exists(fontPath)) {
        g_fontConfig.path = fontPath.string();
    }

    if (fontSize != 0) {
        g_fontConfig.size = (float)fontSize;
    }

    if (color != 0) {
        UINT r = (color >> 16) & 0xFF;
        UINT g = (color >> 8) & 0xFF;
        UINT b = color & 0xFF;
        UINT a = 0xFF;
        // 0xAABBGGRR
        g_fontConfig.color = (a << 24) | (b << 16) | (g << 8) | r;
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_DEBUG_MENU_ADDR), &HookedDebugMenu, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_DEBUG_MENU_ADDR));
        PatchDebugMenuHook(HOOK_DEBUG_MENU_ADDR);
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_DEBUG_NPC_ADDR), &HookedDebugNPC, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_DEBUG_NPC_ADDR));
        PatchDebugNPCHook(HOOK_DEBUG_NPC_ADDR);
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_NPC_DAMAGE_ADDR), &HookedNPCDamage, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_NPC_DAMAGE_ADDR));
        PatchNPCDamageHook(HOOK_NPC_DAMAGE_ADDR);
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_DBG_CAM_ADDR), &HookedDbgCam, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_DBG_CAM_ADDR));
        PatchDbgCamHook(HOOK_DBG_CAM_ADDR);
    }
}
