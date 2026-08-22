#include "pch.h"
#include "MemoryPatch/MemoryPatch.h"
#include "NpcAnimChange.h"

constexpr uintptr_t HOOK_NPC_ANIM_ADDR = 0x1407e385b;
constexpr uintptr_t HOOK_NPC_ANIM_CANCEL_ADDR = 0x140b5205e;
constexpr uintptr_t HOOK_NPC_TURN_ADDR = 0x1407daabc;

static uintptr_t* const pWorldChrMan = reinterpret_cast<uintptr_t*>(0x143d7a1e0);
static uintptr_t* const pNPCPlayer = reinterpret_cast<uintptr_t*>(0x143d7a388);

typedef uintptr_t(*t_fma)(uintptr_t, uintptr_t, uintptr_t, uintptr_t);
static t_fma fp_sub_1407dac80 = reinterpret_cast<t_fma>(0x1407dac80);
static t_fma fp_sub_1407daf30 = nullptr;

typedef uintptr_t(*t_sub_140b45440)(uintptr_t);
static t_sub_140b45440 fp_sub_140b45440 = nullptr;

static std::map<std::pair<uint32_t, uint32_t>, uint32_t> animMap;
static std::unordered_map<uint32_t, uint32_t> directAnimMap;

static float turnSpeed = 200;
static float playSpeed = 0;
static bool enablePlaySpeed = false;
static bool logAnim = false;

static NpcAnimState animState;
static NpcAnimConfig animConfig;

extern INIReader g_INI;

void LoadAnimConfig(const fs::path& path, uint32_t characterId)
{
    static INIReader config(path.string());
    if (characterId == 0) return;
    if (!path.empty()) config = INIReader(path.string());
    if (config.ParseError()) return;

    std::string section = "";
    for (const auto& configSection : config.Sections()) {
        if (configSection.find(std::to_string(characterId)) != std::string::npos) {
            section = configSection;
            break;
        }
    }

    animMap.clear();
    directAnimMap.clear();
    size_t start, pos, valSize;
    uint32_t keyAnim, curAnim, newAnim;
    for (const auto& configKey : config.Keys(section)) {
        std::string valStr = config.GetString(section, configKey, "");
        if (!valStr.empty()) {
            pos = configKey.find('_');
            if (pos == std::string::npos) {
                keyAnim = std::stoul(configKey);
                curAnim = 0;
            } else {
                keyAnim = std::stoul(configKey.substr(0, pos));
                curAnim = std::stoul(configKey.substr(pos + 1));
            }

            start = 0;
            valSize = valStr.size();
            while (start < valSize) {
                pos = valStr.find('_', start);
                if (pos == std::string::npos) {
                    pos = valSize;
                }
                if (pos > start) {
                    newAnim = std::stoul(valStr.substr(start, pos - start));
                    if (curAnim) {
                        animMap.try_emplace({keyAnim, curAnim}, newAnim);
                    } else {
                        directAnimMap.try_emplace(keyAnim, newAnim);
                    }
                    curAnim = newAnim;
                }
                start = pos + 1;
            }
        }
    }
}

uintptr_t HookedNpcAnim(uintptr_t arg1, uint32_t arg2)
{
    uintptr_t npc = *(uintptr_t*)(arg1 + 0x300);
    if (animState.npc != npc) {
        animState.npc = npc;
        animState.lastAnim = NpcAnimState::INVALID_ANIM;
        animState.inherit = false;
    }

    uintptr_t base_ptr = *(uintptr_t*)(npc + 0x1ff8);
    uintptr_t result = *(uintptr_t*)(base_ptr + 0x80);
    uint32_t* pAnim = (uint32_t*)(result + 0x170);

    uint32_t curAnim = 0;
    uintptr_t animPtr = *(uintptr_t*)(base_ptr + 0x10);
    for (int i = *(uint32_t*)(animPtr + 0xf0); i >= 0; --i) {
        curAnim = *(uint32_t*)(animPtr + i * 0x14 + 0x20) % 1000000;
        if ((curAnim < 9000) || (curAnim > 9999)) break;
    }


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
                LoadAnimConfig(animConfig.path, *(uint32_t*)(npc + 0x68));
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

static bool NpcNoGoodsConsume(uintptr_t arg1)
{
    if ((*(uint8_t*)(arg1 + 0x1f42) >> 4) & 1) {
        return *(uintptr_t*)(*pWorldChrMan + 0x88) != arg1;
    }
    return false;
}

uintptr_t HookedNpcAnimCancel(uintptr_t arg1, uintptr_t arg2, uint8_t arg3)
{
    uintptr_t npc = *(uintptr_t*)(arg1 + 0x10);
    uintptr_t result = *(uintptr_t*)(*(uintptr_t*)(npc + 0x1ff8) + 0x80);

    if (arg3 == 0x17) {
        if (result != 0 && NpcNoGoodsConsume(npc)) {
            *(uint32_t*)(arg2 + 0x1e8) |= 2;
            return result;
        }
        return 0;
    }

    return result;
}

static bool NpcNoResourceItemConsume(uintptr_t arg1)
{
    if (*(uint8_t*)(arg1 + 0x1f43) & 1) {
        return *(uintptr_t*)(*pWorldChrMan + 0x88) != arg1;
    }
    return false;
}

float* HookedNpcTurn(uintptr_t arg1, float* arg2, uintptr_t arg3)
{
    *arg2 = *(float*)(arg1 + 0x308);
    if ((*arg2 == 0) && NpcNoResourceItemConsume(arg3)) {
        uintptr_t animPtr = *(uintptr_t*)(*(uintptr_t*)(arg3 + 0x1ff8) + 0x10);
        uint32_t curIndex = *(uint32_t*)(animPtr + 0xf0);
        uint32_t curAnim = *(uint32_t*)(animPtr + curIndex * 0x14 + 0x20) % 1000000;
        if ((curAnim < 3000) || (curAnim > 3999)) *arg2 = turnSpeed;
    }

    *(arg2 + 1) = *arg2;
    return arg2;
}

uintptr_t hook_sub_1407daf30(uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4)
{
    uintptr_t npc = *(uintptr_t*)(*(uintptr_t*)(arg1 + 0x20) + 0x10);
    if (NpcNoResourceItemConsume(npc)) {
        return fp_sub_1407dac80(arg1, arg2, arg3, arg4);
    }
    return fp_sub_1407daf30(arg1, arg2, arg3, arg4);
}

uintptr_t hook_sub_140b45440(uintptr_t arg1)
{
    uintptr_t npcPlayer = *(uintptr_t*)(*pNPCPlayer + 0x160);
    if (npcPlayer) {
        uintptr_t npc = *(uintptr_t*)(arg1 + 8);
        uintptr_t base = *(uintptr_t*)(*(uintptr_t*)(npc + 0x1ff8) + 0x18);
        uint32_t& hp = *(uint32_t*)(base + 0x130);
        if ((hp == 1) && (*(uint32_t*)(base + 0x148) == 0)) {
            int32_t& redDot = *(int32_t*)(base + 0x25c);
            if (redDot > 0) {
                redDot--;
                hp = (redDot > 0) ? *(uint32_t*)(base + 0x134) : 0;
            }
        }

        if (npc == npcPlayer) {
            if (enablePlaySpeed) *(float*)(arg1 + 0xd00) = playSpeed;

            uintptr_t playerBase = *(uintptr_t*)(*(uintptr_t*)(*(uintptr_t*)(
                                        *pWorldChrMan + 0x88) + 0x1ff8) + 0x18);
            if (hp) {
                uint32_t playerHp = hp * (*(uint32_t*)(playerBase + 0x134)) / (*(uint32_t*)(base + 0x134));
                if (playerHp) {
                    *(uint32_t*)(playerBase + 0x130) = playerHp;
                    return fp_sub_140b45440(arg1);
                }
            }
            *(uint32_t*)(playerBase + 0x130) = 1;
        }
    }

    return fp_sub_140b45440(arg1);
}

void EnableNpcAnimChange()
{
    std::string configPath = g_INI.GetString("npc_anim_change", "npc_anim_config", "");
    animConfig.reload = g_INI.GetBoolean("npc_anim_change", "npc_anim_reload", false);
    logAnim = g_INI.GetBoolean("logs", "npc_anim_change", false);
    playSpeed = g_INI.GetReal("npc_anim_change", "play_speed", 0);

    if (playSpeed > 0) {
        turnSpeed = turnSpeed * playSpeed;
        enablePlaySpeed = true;
    }

    fs::path curPath = fs::current_path();
    if (!configPath.empty() && fs::exists(curPath / configPath)) {
        animConfig.path = curPath / configPath;
        animConfig.lastWriteTime = fs::last_write_time(animConfig.path);
        LoadAnimConfig(animConfig.path, 0);
        if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_ADDR), &HookedNpcAnim, NULL) == MH_OK) {
            MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_ADDR));
            PatchNpcAnimHook(HOOK_NPC_ANIM_ADDR);
        }
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_CANCEL_ADDR), &HookedNpcAnimCancel, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_CANCEL_ADDR));
        PatchNpcAnimCancelHook(HOOK_NPC_ANIM_CANCEL_ADDR);
    }

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_NPC_TURN_ADDR), &HookedNpcTurn, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_NPC_TURN_ADDR));
        PatchNpcTurnHook(HOOK_NPC_TURN_ADDR);
        MH_CreateHook(reinterpret_cast<LPVOID>(0x1407daf30), &hook_sub_1407daf30, 
                    reinterpret_cast<LPVOID*>(&fp_sub_1407daf30));
    }

    MH_CreateHook(reinterpret_cast<LPVOID>(0x140b45440), &hook_sub_140b45440, 
                    reinterpret_cast<LPVOID*>(&fp_sub_140b45440));
}
