#include "pch.h"
#include "KeyRemap.h"
#include "MemoryPatch/MemoryPatch.h"

constexpr uintptr_t HOOK_DBG_CAM_ADDR = 0x14083bebf;
constexpr uintptr_t D2A_MAPPING_ADDR = 0x142600bc0;
constexpr uintptr_t D2D_MAPPING_ADDR = 0x142600d50;

typedef int64_t(*t_D2AMapping)(
    uintptr_t arg1, int32_t arg2, int32_t arg3, float arg4,
    int32_t arg5, int64_t arg6);
static t_D2AMapping fp_D2AMapping = reinterpret_cast<t_D2AMapping>(D2A_MAPPING_ADDR);

typedef uintptr_t(*t_D2DMapping)(uintptr_t, uint32_t, uint32_t, uint32_t, uintptr_t);
static t_D2DMapping fp_D2DMapping = reinterpret_cast<t_D2DMapping>(D2D_MAPPING_ADDR);

typedef int64_t(*t_sub_141166360)(uintptr_t);
static t_sub_141166360 fp_sub_141166360 = nullptr;

static std::map<std::pair<uint32_t, uint32_t>, uint32_t> keyRemapData;
static bool logKeyRemap = false;
extern INIReader g_INI;

static uint32_t slowKey = 0;
static uint32_t fastKey = 0;
static uint32_t upKey = 0;
static uint32_t downKey = 0;

int64_t hook_sub_141166360(uintptr_t arg1)
{
    int64_t result = fp_sub_141166360(arg1);
    if (slowKey) fp_D2DMapping(arg1, 0x1c4, slowKey, 0, result);
    if (fastKey) fp_D2DMapping(arg1, 0x1c3, fastKey, 0, result);
    if (upKey) fp_D2AMapping(arg1, 0x315, upKey, 1.0f, 0, result);
    if (downKey) fp_D2AMapping(arg1, 0x316, downKey, 1.0f, 0, result);

    fp_D2AMapping(arg1, 0x314, 0x65, 1.0f, 0, result); // D
    fp_D2AMapping(arg1, 0x314, 0x63, -1.0f, 0, result); // A
    fp_D2AMapping(arg1, 0x313, 0x56, 1.0f, 0, result); // W
    fp_D2AMapping(arg1, 0x313, 0x64, -1.0f, 0, result); // S

    fp_D2AMapping(arg1, 0x317, 0xc1, 1.0f, 0, result); // right
    fp_D2AMapping(arg1, 0x317, 0xc0, -1.0f, 0, result); // left
    fp_D2AMapping(arg1, 0x318, 0xbe, 1.0f, 0, result); // up
    fp_D2AMapping(arg1, 0x318, 0xc3, -1.0f, 0, result); // down

    return result;
}

int64_t HookedDbgCam(uintptr_t arg1)
{
    static uint8_t* freezePtr = (uint8_t*)0x143d7acb2;
    static uint32_t lastCamMode = 0;
    static uint8_t mask0 = 0;
    static uint8_t* maskPtr = nullptr;

    uint32_t camMode = *(uint32_t*)(arg1 + 0xe0);
    if ((camMode != 0) && (lastCamMode == 0)) {
        maskPtr = (uint8_t*)(
            *(uintptr_t*)(
            *(uintptr_t*)(
            *(uintptr_t*)(
            *(uintptr_t*)0x143d7a1e0 + 0x88) + 0x50) + 0x10) + 0x1f40);
        mask0 = *maskPtr & 0xE0;
    }

    if ((camMode == 1) != (lastCamMode == 1)) {
        *freezePtr = (camMode == 1) ? 1 : 2;
    }

    if ((camMode == 2) != (lastCamMode == 2)) {
        *maskPtr &= 0x1F;
        *maskPtr |= (camMode == 2) ? 0xE0 : mask0;
    }

    lastCamMode = camMode;
    return camMode - 1;
}

uintptr_t hook_D2DMapping(uintptr_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uintptr_t arg5)
{
    uint32_t key = arg3;
    auto it = keyRemapData.find({arg2, arg3});
    if (it != keyRemapData.end()) {
        key = it->second;
        if (logKeyRemap) {
            std::cout << "[key_remap] " << std::hex << arg2 << " " << arg3 << " remap to 0x" << key << std::endl;
        }
    } else if (logKeyRemap) {
        std::cout << "[key_remap] " << std::hex << arg2 << " " << arg3 << std::endl;
    }

    return fp_D2DMapping(arg1, arg2, key, arg4, arg5);
}

void EnableKeyRemap()
{
    bool tabCam = g_INI.GetBoolean("free_camera", "tab", false);
    slowKey = g_INI.GetUnsigned("free_camera", "move_slow", 0);
    fastKey = g_INI.GetUnsigned("free_camera", "move_fast", 0);
    upKey = g_INI.GetUnsigned("free_camera", "move_up", 0);
    downKey = g_INI.GetUnsigned("free_camera", "move_down", 0);
    logKeyRemap = g_INI.GetBoolean("logs", "key_remap", false);

    size_t pos;
    for (const auto& key : g_INI.Keys("key_remap")) {
        pos = key.find('_');
        if (pos != std::string::npos) {
            uint32_t firstVal = static_cast<uint32_t>(std::stoul(key.substr(0, pos), nullptr, 16));
            uint32_t secondVal = static_cast<uint32_t>(std::stoul(key.substr(pos + 1), nullptr, 16));
            keyRemapData[std::make_pair(firstVal, secondVal)] = g_INI.GetUnsigned("key_remap", key, 0);
        }
    }

    if (logKeyRemap || !keyRemapData.empty()) {
        MH_CreateHook(reinterpret_cast<LPVOID>(D2D_MAPPING_ADDR), &hook_D2DMapping,
                        reinterpret_cast<LPVOID*>(&fp_D2DMapping));
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_DBG_CAM_ADDR), &HookedDbgCam, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_DBG_CAM_ADDR));
        PatchDbgCamHook(HOOK_DBG_CAM_ADDR, tabCam);
    }

    MH_CreateHook(reinterpret_cast<LPVOID>(0x141166360), &hook_sub_141166360,
                        reinterpret_cast<LPVOID*>(&fp_sub_141166360));
}
