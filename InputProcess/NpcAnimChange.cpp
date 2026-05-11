#include "pch.h"
#include "MemoryPatch/MemoryPatch.h"
#include "NpcAnimChange.h"

constexpr uintptr_t HOOK_NPC_ANIM_ADDR = 0x1407e385b;
constexpr uintptr_t HOOK_NPC_ANIM_CANCEL_ADDR = 0x140b5205e;
constexpr uintptr_t HOOK_NPC_LOOK_ADDR = 0x1407daabc;

typedef uintptr_t(*t_fma)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static t_fma fp_sub_1407dac80 = reinterpret_cast<t_fma>(0x1407dac80);
static t_fma fp_sub_1407daf30 = nullptr;

typedef uintptr_t(*t_sub_140b45440)(uintptr_t);
static t_sub_140b45440 fp_sub_140b45440 = nullptr;

static std::map<std::pair<uint32_t, uint32_t>, uint32_t> animMap;
static std::unordered_map<uint32_t, uint32_t> directAnimMap;

#define NPC_MAX 3
static uintptr_t npcList[NPC_MAX];
static int npcIndex = 0;
static uintptr_t npcPlayer = 0;

static float animSpeed = 0;
static float animTurnSpeed = 200;
static bool logAnim = false;

static NpcAnimState animState;
static NpcAnimConfig animConfig;

extern INIReader g_INI;

static bool IsNpcCtrl(uintptr_t npc)
{
    if (npcList[npcIndex] == 0) return false;

    for (int i = 0; i < NPC_MAX; i++) {
        if (npcList[i] == npc) {
            return true;
        }
    }
    return false;
}

static void LoadAnimConfig()
{
    uint32_t animId, curAnimId, newAnimId;
    INIReader config(animConfig.path.string());
    for (const auto& configKey : config.Keys("")) {
        std::string valStr = config.GetString("", configKey, "0");
        newAnimId = std::stoul(valStr);

        size_t pos = configKey.find('_');
        if (pos == std::string::npos) {
            animId = std::stoul(configKey);
            directAnimMap[animId] = newAnimId;
        } else {
            animId = std::stoul(configKey.substr(0, pos));
            curAnimId = std::stoul(configKey.substr(pos + 1));
            animMap[{animId, curAnimId}] = newAnimId;
        }
    }
}

uintptr_t HookedNpcAnim(uintptr_t arg1, uint32_t arg2)
{
    animState.np = 0;
    uintptr_t npc = *(uintptr_t*)(arg1 + 0x300);
    if (npcPlayer != npc) {
        if (!IsNpcCtrl(npc)) {
            npcIndex = (npcIndex + 1) % NPC_MAX;
            npcList[npcIndex] = npc;
        }
        npcPlayer = npc;
        animState.lastAnim = NpcAnimState::INVALID_ANIM;
        animState.inherit = false;
    }

    uintptr_t base_ptr = *(uintptr_t*)(npc + 0x1ff8);
    uintptr_t result = *(uintptr_t*)(base_ptr + 0x80);
    uint32_t* pAnim = (uint32_t*)(result + 0x170);

    uintptr_t animPtr = *(uintptr_t*)(base_ptr + 0x10);
    uint32_t curIndex = *(uint32_t*)(animPtr + 0xf0);
    uint32_t curAnim = *(uint32_t*)(animPtr + curIndex * 0x14 + 0x20) % 1000000;

    if (arg2 == NpcAnimState::INVALID_ANIM) {
        if (*pAnim != NpcAnimState::INVALID_ANIM) {
            if (animState.inherit) {
                if (*pAnim == curAnim) {
                    *pAnim = NpcAnimState::INVALID_ANIM;
                }
            } else {
                *pAnim = NpcAnimState::INVALID_ANIM;
            }
        } else if (animConfig.reload && (animConfig.reloadDelay++ > animConfig.DELAY_MAX)) {
            animConfig.reloadDelay = 0;
            auto currentWriteTime = fs::last_write_time(animConfig.path);
            if (animConfig.lastWriteTime != currentWriteTime) {
                animConfig.lastWriteTime = currentWriteTime;
                animMap.clear();
                directAnimMap.clear();
                LoadAnimConfig();
            }
        }
    } else if (arg2 == animState.lastAnim) {
        if (*pAnim != NpcAnimState::INVALID_ANIM) {
            if (*pAnim == curAnim) {
                *pAnim = NpcAnimState::INVALID_ANIM;
            }
        }
    } else {
        auto it = animMap.find({arg2, curAnim});
        if (it != animMap.end()) {
            animState.inherit = true;
            *pAnim = it->second;
        } else {
            animState.inherit = false;
            auto it = directAnimMap.find(arg2);
            if (it != directAnimMap.end()) {
                *pAnim = it->second;
            } else {
                *pAnim = arg2;
            }
        }

        if (logAnim) {
            std::cout << "[npc_anim_change] "
                << "keyAnim: " << arg2
                << ", curAnim: " << curAnim
                << ", newAnim: " << *pAnim << std::endl;
        }
    }

    animState.lastAnim = arg2;
    return result;
}

uintptr_t HookedNpcAnimCancel(uintptr_t arg1, uintptr_t arg2, uint8_t arg3)
{
    uintptr_t npc = *(uintptr_t*)(arg1 + 0x10);
    uintptr_t result = *(uintptr_t*)(*(uintptr_t*)(npc + 0x1ff8) + 0x80);

    if (arg3 == 0x17) {
        if (result != 0 && IsNpcCtrl(npc)) {
            *(uint32_t*)(arg2 + 0x1e8) |= 2;
            return result;
        }
        return 0;
    }

    return result;
}

float* HookNpcLook(uintptr_t arg1, float* arg2, uintptr_t arg3)
{
    *arg2 = *(float*)(arg1 + 0x308);
    if ((*arg2 == 0) && IsNpcCtrl(arg3)) {
        if ((npcPlayer == arg3) && (animState.np++ > NpcAnimState::NP_MAX)) {
            npcPlayer = 0;
            *(uint8_t*)(arg3 + 0x1070) = 0;
        }

        uintptr_t animPtr = *(uintptr_t*)(*(uintptr_t*)(arg3 + 0x1ff8) + 0x10);
        uint32_t curIndex = *(uint32_t*)(animPtr + 0xf0);
        uint32_t curAnim = *(uint32_t*)(animPtr + curIndex * 0x14 + 0x20) % 1000000;
        if (curAnim > 9999) *arg2 = animTurnSpeed;
    }

    *(arg2 + 1) = *arg2;
    return arg2;
}

uintptr_t hook_sub_1407daf30(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4)
{
    uintptr_t npc = *(uintptr_t*)(*(uintptr_t*)(arg1 + 0x20) + 0x10);
    if (IsNpcCtrl(npc)) {
        return fp_sub_1407dac80(arg1, arg2, arg3, arg4);
    }
    return fp_sub_1407daf30(arg1, arg2, arg3, arg4);
}

uintptr_t hook_sub_140b45440(uintptr_t arg1)
{
    if (IsNpcCtrl(*(uintptr_t*)(arg1 + 8))) {
        float speed = *(float*)(arg1 + 0xd00);
        *(float*)(arg1 + 0xd00) = animSpeed;
        uintptr_t result = fp_sub_140b45440(arg1);
        *(float*)(arg1 + 0xd00) = speed;
        return result;
    }
    return fp_sub_140b45440(arg1);
}

void EnableNpcAnimChange()
{
    logAnim = g_INI.GetBoolean("logs", "npc_anim_change", false);
    animSpeed = g_INI.GetReal("npc_anim_change", "anim_speed", 0);
    std::string configPath = g_INI.GetString("npc_anim_change", "config_path", "");
    bool reload = g_INI.GetBoolean("npc_anim_change", "config_runtime_reload", false);

    fs::path curPath = fs::current_path();
    if (!configPath.empty() && fs::exists(curPath / configPath)) {
        animConfig.path = curPath / configPath;
        animConfig.lastWriteTime = fs::last_write_time(animConfig.path);
        animConfig.reload = reload;
        LoadAnimConfig();
    } else {
        animConfig.reload = false;
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_ADDR), &HookedNpcAnim, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_ADDR));
        PatchNpcAnimHook(HOOK_NPC_ANIM_ADDR);
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_CANCEL_ADDR), &HookedNpcAnimCancel, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_CANCEL_ADDR));
        PatchNpcAnimCancelHook(HOOK_NPC_ANIM_CANCEL_ADDR);
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_NPC_LOOK_ADDR), &HookNpcLook, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_NPC_LOOK_ADDR));
        PatchNpcLooKHook(HOOK_NPC_LOOK_ADDR);
        MH_CreateHook(reinterpret_cast<LPVOID>(0x1407daf30), &hook_sub_1407daf30, 
                    reinterpret_cast<LPVOID*>(&fp_sub_1407daf30));
    }

    if (animSpeed > 0) {
        animTurnSpeed = animTurnSpeed * animSpeed;
        MH_CreateHook(reinterpret_cast<LPVOID>(0x140b45440), &hook_sub_140b45440, 
                    reinterpret_cast<LPVOID*>(&fp_sub_140b45440));
    }
}
