#include "pch.h"
#include "MemoryPatch/MemoryPatch.h"
#include "InputProcess.h"

constexpr uintptr_t HOOK_KEY_MAPPING_ADDR = 0x142600d50;
constexpr uintptr_t HOOK_NPC_ANIM_ADDR = 0x1407e385b;
constexpr uintptr_t HOOK_NPC_ANIM_CANCEL_ADDR = 0x140b5205e;

constexpr uint32_t INVALID_ANIM = 0xFFFFFFFF;

static std::map<std::pair<uint32_t, uint32_t>, uint32_t> keyRemap;

static std::unordered_map<uint32_t, std::vector<uint32_t>> animKeys;
static std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> animKeyNewAnims;

typedef void*(*t_KeyMapping)(void*, uint32_t, uint32_t, void*);
static t_KeyMapping fpKeyMapping = nullptr;

typedef uintptr_t(*t_sub_1407e3ea0)(uintptr_t);
static t_sub_1407e3ea0 fp_sub_1407e3ea0 = reinterpret_cast<t_sub_1407e3ea0>(0x1407e3ea0);

static bool g_logKeyRemap = false;
static fs::path g_animConfigPath;
static bool g_hotReload = false;
static int reloadDelay = 0;
static fs::file_time_type lastWriteTime;

static uint32_t lastAnim = INVALID_ANIM;
static uint32_t vKey = 0;
static int comboIndex = 0;
static std::vector<uint32_t>* comboAnimsPtr = nullptr;
static bool blockCombo = false;
static bool animApplied = false;

static uintptr_t g_Npc = 0;

extern INIReader g_INI;

static bool IsKeyDown(int key)
{
    if (key > 255) return false;
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

static void LoadAnimConfig()
{
    uint32_t key, animId, newAnimId;
    size_t valSize, start, pos;
    INIReader config(g_animConfigPath.string());
    for (const auto& configKey : config.Keys("")) {
        pos = configKey.find('_');
        if (pos == std::string::npos) {
            key = 256;
            animId = static_cast<uint32_t>(std::stoul(configKey.substr(0)));
        } else if (pos == 1) {
            key = std::toupper(static_cast<unsigned char>(configKey[0]));
            animId = static_cast<uint32_t>(std::stoul(configKey.substr(2)));
        } else {
            key = static_cast<uint32_t>(std::stoul(configKey.substr(0, pos)));
            animId = static_cast<uint32_t>(std::stoul(configKey.substr(pos + 1)));
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
                    newAnimId = static_cast<uint32_t>(std::stoul(valStr.substr(start, pos - start)));
                    animKeyNewAnims[std::make_pair(animId, key)].push_back(newAnimId);
                }
                start = pos + 1;
            }
            animKeys[animId].push_back(key);
        }
    }
}

void* HookedKeyMapping(void* arg1, uint32_t arg2, uint32_t arg3, void* arg4)
{
    uint32_t key = arg3;
    auto it = keyRemap.find({arg2, arg3});
    if (it != keyRemap.end()) {
        key = it->second;
        if (g_logKeyRemap) {
            std::cout << "[key_remap] " << std::hex << arg2 << " " << arg3 << " remap to 0x" << key << std::endl;
        }
    } else if (g_logKeyRemap) {
        std::cout << "[key_remap] " << std::hex << arg2 << " " << arg3 << std::endl;
    }

    return fpKeyMapping(arg1, arg2, key, arg4);
}

uintptr_t HookedNpcAnim(uintptr_t arg1, uint32_t arg2)
{
    g_Npc = fp_sub_1407e3ea0(arg1);
    uint32_t curAnim = *reinterpret_cast<uint32_t*>(g_Npc + 0xC8) % 10000;
    uint32_t* pAnim = reinterpret_cast<uint32_t*>(g_Npc + 0x170);

    if (arg2 == INVALID_ANIM) {
        animApplied = false;
        blockCombo = false;

        if ((lastAnim != INVALID_ANIM) && !IsKeyDown(vKey)) {
            lastAnim = INVALID_ANIM;
        } else if (g_hotReload && (++reloadDelay > 60)) {
            reloadDelay = 0;
            auto currentWriteTime = fs::last_write_time(g_animConfigPath);
            if (lastWriteTime != currentWriteTime) {
                lastWriteTime = currentWriteTime;
                animKeys.clear();
                animKeyNewAnims.clear();
                LoadAnimConfig();
                lastAnim = INVALID_ANIM;
            }
        }

        *pAnim = arg2;
        return g_Npc;
    }

    auto it = animKeys.find(arg2);
    if (it != animKeys.end()) {
        for (uint32_t val : it->second) {
            if ((val == 256) || (val > 256 && val == curAnim) || IsKeyDown(val)) {
                if (arg2 == lastAnim && vKey == val) {
                    if (!blockCombo) {
                        if (curAnim == (*comboAnimsPtr)[comboIndex]) {
                            comboIndex = (comboIndex + 1) % comboAnimsPtr->size();
                            blockCombo = true;
                        }
                    } else if (val > 255) {
                        if (animApplied) {
                            *pAnim = INVALID_ANIM;
                            return g_Npc;
                        } else if (curAnim == (*comboAnimsPtr)[comboIndex]) {
                            animApplied = true;
                        }
                    }

                    *pAnim = (*comboAnimsPtr)[comboIndex];
                    return g_Npc;
                }

                auto a = animKeyNewAnims.find({arg2, val});
                if (a != animKeyNewAnims.end()) {
                    lastAnim = arg2;
                    vKey = val;
                    comboIndex = 0;
                    comboAnimsPtr = &(a->second);
                    animApplied = false;
                    blockCombo = true;
                    *pAnim = (*comboAnimsPtr)[comboIndex];
                    return g_Npc;
                }
            }
        }
    }

    if (animApplied) {
        *pAnim = INVALID_ANIM;
        return g_Npc;
    } else if (curAnim == arg2) {
        animApplied = true;
    }

    lastAnim = INVALID_ANIM;
    *pAnim = arg2;
    return g_Npc;
}

uintptr_t HookedNpcAnimCancel(uintptr_t arg1, uintptr_t arg2, uint8_t arg3)
{
    uintptr_t result = *(uintptr_t*)(arg1 + 0x10);
    result = *(uintptr_t*)(result + 0x1ff8);
    result = *(uintptr_t*)(result + 0x80);

    if (arg3 == 0x17) {
        if (result == 0 || result != g_Npc)
            return 0;

        *(uint32_t*)(arg2 + 0x1e8) |= 2;
    }

    return result;
}

void EnableInputProcess()
{
    g_logKeyRemap = g_INI.GetBoolean("logs", "key_remap", false);
    std::string animConfig = g_INI.GetString("npc_anim_change", "npc_anim_config", "");
    bool hotReload = g_INI.GetBoolean("npc_anim_change", "enable_hot_reload", false);

    size_t pos;
    for (const auto& key : g_INI.Keys("key_remap")) {
        pos = key.find('_');
        if (pos != std::string::npos) {
            uint32_t firstVal = static_cast<uint32_t>(std::stoul(key.substr(0, pos), nullptr, 16));
            uint32_t secondVal = static_cast<uint32_t>(std::stoul(key.substr(pos + 1), nullptr, 16));
            keyRemap[std::make_pair(firstVal, secondVal)] = g_INI.GetUnsigned("key_remap", key, 0);
        }
    }

    fs::path curPath = fs::current_path();
    if (!animConfig.empty() && fs::exists(curPath / animConfig)) {
        g_animConfigPath = curPath / animConfig;
        g_hotReload = hotReload;
        lastWriteTime = fs::last_write_time(g_animConfigPath);
        LoadAnimConfig();
    }

    if (g_logKeyRemap || !keyRemap.empty()) {
        if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_KEY_MAPPING_ADDR), &HookedKeyMapping,
                            reinterpret_cast<LPVOID*>(&fpKeyMapping)) == MH_OK) {
            MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_KEY_MAPPING_ADDR));
        }
    }

    if (g_hotReload || !animKeys.empty()) {
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
