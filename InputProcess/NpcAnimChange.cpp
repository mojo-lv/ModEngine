#include "pch.h"
#include "MemoryPatch/MemoryPatch.h"
#include "NpcAnimChange.h"

constexpr uintptr_t HOOK_NPC_ANIM_ADDR = 0x1407e385b;
constexpr uintptr_t HOOK_NPC_ANIM_CANCEL_ADDR = 0x140b5205e;

typedef uintptr_t(*t_sub_1407e3ea0)(uintptr_t);
static t_sub_1407e3ea0 fp_sub_1407e3ea0 = reinterpret_cast<t_sub_1407e3ea0>(0x1407e3ea0);

static std::unordered_map<uint32_t, std::vector<uint32_t>> animKeys;
static std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> animKeyNewAnims;

static NpcAnimState animState;
static NpcAnimConfig animConfig;

extern INIReader g_INI;

static bool IsKeyDown(int key)
{
    if (key >= 256) return false;
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

static void LoadAnimConfig()
{
    uint32_t key, animId, newAnimId;
    size_t valSize, start, pos;
    INIReader config(animConfig.path.string());
    for (const auto& configKey : config.Keys("")) {
        pos = configKey.find('_');
        if (pos == std::string::npos) {
            key = 256;
            animId = std::stoul(configKey.substr(0));
        } else if (pos == 1) {
            key = std::toupper(static_cast<unsigned char>(configKey[0]));
            animId = std::stoul(configKey.substr(2));
        } else {
            key = std::stoul(configKey.substr(0, pos));
            animId = std::stoul(configKey.substr(pos + 1));
        }

        std::string valStr = config.GetString("", configKey, "");
        if (!valStr.empty()) {
            valSize = valStr.size();
            start = 0;
            while (start < valSize) {
                pos = valStr.find('_', start);
                if (pos == std::string::npos) {
                    pos = valSize;
                }
                if (pos > start) {
                    newAnimId = std::stoul(valStr.substr(start, pos - start));
                    if (newAnimId == 0) newAnimId = NpcAnimState::INVALID_ANIM;
                    animKeyNewAnims[std::make_pair(animId, key)].push_back(newAnimId);
                }
                start = pos + 1;
            }
            animKeys[animId].push_back(key);
        }
    }
}

uintptr_t HookedNpcAnim(uintptr_t arg1, uint32_t arg2)
{
    animState.npc = fp_sub_1407e3ea0(arg1);
    uint32_t curAnim = *reinterpret_cast<uint32_t*>(animState.npc + 0xC8) % 10000;
    uint32_t* pAnim = reinterpret_cast<uint32_t*>(animState.npc + 0x170);
    // std::cout << curAnim << " " << *pAnim << " " << arg2 << std::endl;

    if (arg2 == NpcAnimState::INVALID_ANIM) {
        animState.blockCombo = false;
        animState.animApplied = false;

        if ((animState.lastAnim != NpcAnimState::INVALID_ANIM) && !IsKeyDown(animState.vKey)) {
            animState.lastAnim = NpcAnimState::INVALID_ANIM;
        } else if (animConfig.reload && (animConfig.reloadDelay++ > animConfig.DELAY_MAX)) {
            animConfig.reloadDelay = 0;
            auto currentWriteTime = fs::last_write_time(animConfig.path);
            if (animConfig.lastWriteTime != currentWriteTime) {
                animConfig.lastWriteTime = currentWriteTime;
                animKeys.clear();
                animKeyNewAnims.clear();
                LoadAnimConfig();
                animState.lastAnim = NpcAnimState::INVALID_ANIM;
            }
        }

        *pAnim = arg2;
        return animState.npc;
    }

    auto it = animKeys.find(arg2);
    if (it != animKeys.end()) {
        for (uint32_t val : it->second) {
            if ((val == 256) || (val > 256 && val == curAnim) || IsKeyDown(val)) {
                if (arg2 == animState.lastAnim && animState.vKey == 256 && val > 256) continue;

                if (arg2 == animState.lastAnim && animState.vKey == val) {
                    if (!animState.blockCombo) {
                        if (curAnim == (*animState.animsPtr)[animState.animIndex]) {
                            animState.animIndex = (animState.animIndex + 1) % animState.animsPtr->size();
                            animState.blockCombo = true;
                        }
                    } else if (val >= 256) {
                        if (animState.animApplied) {
                            *pAnim = NpcAnimState::INVALID_ANIM;
                            return animState.npc;
                        } else if (std::find(animState.animsPtr->begin(), animState.animsPtr->end(),
                                                curAnim) != animState.animsPtr->end()) {
                            animState.animApplied = true;
                        }
                    }

                    *pAnim = (*animState.animsPtr)[animState.animIndex];
                    return animState.npc;
                }

                auto a = animKeyNewAnims.find({arg2, val});
                if (a != animKeyNewAnims.end()) {
                    animState.animsPtr = &(a->second);
                    animState.animIndex = 0;
                    animState.lastAnim = arg2;
                    animState.vKey = val;
                    animState.blockCombo = true;
                    animState.animApplied = false;
                    *pAnim = animState.animsPtr->front();
                    return animState.npc;
                }
            }
        }
    }

    if (animState.animApplied) {
        *pAnim = NpcAnimState::INVALID_ANIM;
        return animState.npc;
    } else if (curAnim == arg2) {
        animState.animApplied = true;
    }

    if (curAnim != 9999) {
        animState.lastAnim = NpcAnimState::INVALID_ANIM;
        *pAnim = arg2;
    }

    return animState.npc;
}

uintptr_t HookedNpcAnimCancel(uintptr_t arg1, uintptr_t arg2, uint8_t arg3)
{
    uintptr_t result = *(uintptr_t*)(arg1 + 0x10);
    result = *(uintptr_t*)(result + 0x1ff8);
    result = *(uintptr_t*)(result + 0x80);

    if (arg3 == 0x17) {
        if (result == 0 || result != animState.npc)
            return 0;

        *(uint32_t*)(arg2 + 0x1e8) |= 2;
    }

    return result;
}

void EnableNpcAnimChange()
{
    std::string configPath = g_INI.GetString("npc_anim_change", "config_path", "");
    bool reload = g_INI.GetBoolean("npc_anim_change", "config_runtime_reload", false);

    fs::path curPath = fs::current_path();
    if (!configPath.empty() && fs::exists(curPath / configPath)) {
        animConfig.path = curPath / configPath;
        animConfig.lastWriteTime = fs::last_write_time(animConfig.path);
        animConfig.reload = reload;
        LoadAnimConfig();

        if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_ADDR), &HookedNpcAnim, NULL) == MH_OK) {
            MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_ADDR));
            PatchNpcAnimHook(HOOK_NPC_ANIM_ADDR);
        }

        if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_CANCEL_ADDR), &HookedNpcAnimCancel, NULL) == MH_OK) {
            MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_CANCEL_ADDR));
            PatchNpcAnimCancelHook(HOOK_NPC_ANIM_CANCEL_ADDR);
        }
    }
}
