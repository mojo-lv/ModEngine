#include "pch.h"
#include "MemoryPatch/MemoryPatch.h"
#include "DebugMenu.h"

constexpr uintptr_t HOOK_DEBUG_MENU_ADDR = 0x14262d186;
constexpr uintptr_t HOOK_DEBUG_NPC_ADDR = 0x140614f13;
constexpr uintptr_t HOOK_NPC_DAMAGE_ADDR = 0x140b6a13d;
constexpr uintptr_t HOOK_DBG_CAM_NPC_CTRL_ADDR = 0x14083bebf;
constexpr uintptr_t HOOK_DBG_CAM_FREE_T_ADDR = 0x14083b3fc;
constexpr uintptr_t HOOK_DBG_CAM_FREE_F_ADDR = 0x140a750e8;

static uintptr_t* const pNPCList = reinterpret_cast<uintptr_t*>(0x143d547d8);
static uintptr_t* const pWorldChrMan = reinterpret_cast<uintptr_t*>(0x143d7a1e0);
static uintptr_t* const pNPCPlayer = reinterpret_cast<uintptr_t*>(0x143d7a388);
static uint8_t* const freezePtr = reinterpret_cast<uint8_t*>(0x143d7acb2);

typedef uintptr_t(*t_addNPC)(uintptr_t, uintptr_t*);
static t_addNPC fpAddNPC = reinterpret_cast<t_addNPC>(0x140615c90);

static DbgCamState g_CamState;

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

void HookedDebugNpc()
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

uint16_t HookedNpcDamage(uint16_t arg1)
{
    if (arg1 == 0x101 || arg1 == 0) {
        return 0;
    }

    //std::cout << "[DebugMenu] Unknown Damage " << std::hex << arg1 << std::endl;
    return arg1;
}

int64_t HookedDbgCamNpcCtrl(uint32_t* arg1)
{
    uint32_t camMode = arg1[0x38];
    uintptr_t npc = *(uintptr_t*)(*pNPCPlayer + 0x160);

    if (g_CamState.npc != npc) {
        if (g_CamState.npc) *(uint8_t*)(g_CamState.npc + 0x1070) = 0;
        g_CamState.npc = npc;
    }

    if (npc) {
        uintptr_t& ctrlPtr = *(uintptr_t*)(*(uintptr_t*)(npc + 0x50) + 0x358);
        if (camMode) {
            ctrlPtr = 0;
        } else if (!ctrlPtr) {
            ctrlPtr = *(uintptr_t*)(*pNPCPlayer + 0x150);
        }
    }

    if ((camMode == 1) != (g_CamState.lastCamMode == 1)) {
        *freezePtr = (camMode == 1) ? 1 : 2;
    }

    if ((camMode == 2) != (g_CamState.lastCamMode == 2)) {
        if (camMode == 2) {
            g_CamState.playerMaskPtr = (uint8_t*)(*(uintptr_t*)(*pWorldChrMan + 0x88) + 0x1f40);
            g_CamState.lastMask = *g_CamState.playerMaskPtr & 0xE0;
        } else {
            *g_CamState.playerMaskPtr = (*g_CamState.playerMaskPtr & 0x1F) | g_CamState.lastMask;
        }
    } else if (camMode == 2) {
        *g_CamState.playerMaskPtr |= 0xE0; // No Move/Attack/Hit
        if (npc) {
            g_CamState.lastMask |= 0xC0;
        } else {
            g_CamState.lastMask &= 0x20;
        }
    }

    g_CamState.lastCamMode = camMode;
    return camMode - 1;
}

bool HookedDbgCamFree(void* arg1, uint32_t* arg2)
{
    uint32_t camMode = arg2[0x38];
    if (camMode == 0) return false;
    if (camMode == 3 && *(uintptr_t*)(*pNPCPlayer + 0x160)) return false;
    return true;
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

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_DEBUG_NPC_ADDR), &HookedDebugNpc, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_DEBUG_NPC_ADDR));
        PatchDebugNpcHook(HOOK_DEBUG_NPC_ADDR);
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_NPC_DAMAGE_ADDR), &HookedNpcDamage, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_NPC_DAMAGE_ADDR));
        PatchNpcDamageHook(HOOK_NPC_DAMAGE_ADDR);
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_DBG_CAM_NPC_CTRL_ADDR), &HookedDbgCamNpcCtrl, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_DBG_CAM_NPC_CTRL_ADDR));
        PatchDbgCamNpcCtrlHook(HOOK_DBG_CAM_NPC_CTRL_ADDR);
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_DBG_CAM_FREE_T_ADDR), &HookedDbgCamFree, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_DBG_CAM_FREE_T_ADDR));
        PatchDbgCamFreeHook(HOOK_DBG_CAM_FREE_T_ADDR, true);
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_DBG_CAM_FREE_F_ADDR), &HookedDbgCamFree, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_DBG_CAM_FREE_F_ADDR));
        PatchDbgCamFreeHook(HOOK_DBG_CAM_FREE_F_ADDR, false);
    }
}
