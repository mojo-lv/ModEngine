#include "pch.h"
#include "KeyRemap.h"

constexpr uintptr_t HOOK_D2D_MAPPING_ADDR = 0x142600d50;
constexpr uintptr_t HOOK_A2A_MAPPING_ADDR = 0x142600e40;
constexpr uintptr_t D2A_MAPPING_ADDR = 0x142600bc0;

typedef int64_t(*t_D2AMapping)(
    uintptr_t arg1, int32_t arg2, int32_t arg3, float arg4,
    int32_t arg5, int64_t* arg6);
static t_D2AMapping fp_D2AMapping = reinterpret_cast<t_D2AMapping>(D2A_MAPPING_ADDR);

typedef int64_t(*t_D2DMapping)(uintptr_t, uint32_t, uint32_t, uint32_t, uintptr_t);
static t_D2DMapping fp_D2DMapping = nullptr;

typedef int64_t(*t_A2AMapping)(uintptr_t, uint32_t, uint32_t, uint32_t);
static t_A2AMapping fp_A2AMapping = nullptr;

static std::map<std::pair<uint32_t, uint32_t>, uint32_t> D2DRemapData;
static std::map<std::pair<uint32_t, uint32_t>,
                std::pair<uint32_t, uint32_t>> A2ARemapData;

static bool logKeyRemap = false;
extern INIReader g_INI;

int64_t hook_D2DMapping(uintptr_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uintptr_t arg5)
{
    auto it = D2DRemapData.find({arg2, arg3});
    if (it != D2DRemapData.end()) {
        uint32_t key = it->second;
        if (logKeyRemap) {
            std::cout << "[key_remap] D2D: " << std::hex << arg2 << "_" << arg3
                    << " remap to " << key << std::endl;
        }
        return fp_D2DMapping(arg1, arg2, key, arg4, arg5);
    }

    if (logKeyRemap) {
        std::cout << "[key_remap] D2D: " << std::hex << arg2 << "_" << arg3 << std::endl;
    }
    return fp_D2DMapping(arg1, arg2, arg3, arg4, arg5);
}

int64_t hook_A2AMapping(uintptr_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4)
{
    auto it = A2ARemapData.find({arg2, arg3});
    if (it != A2ARemapData.end()) {
        std::pair<uint32_t, uint32_t> keyPair = it->second;
        if (logKeyRemap) {
            std::cout << "[key_remap] A2A: " << std::hex << arg2 << "_" << arg3
                    << " remap to " << keyPair.first << "_" << keyPair.second << std::endl;
        }
        int64_t tmp = 0;
        fp_D2AMapping(arg1, arg2, keyPair.first, 1.0f, 0, &tmp);
        return fp_D2AMapping(arg1, arg2, keyPair.second, -1.0f, 0, &tmp);
    }

    if (logKeyRemap) {
        std::cout << "[key_remap] A2A: " << std::hex << arg2 << "_" << arg3 << std::endl;
    }
    return fp_A2AMapping(arg1, arg2, arg3, arg4);
}

void EnableKeyRemap()
{
    logKeyRemap = g_INI.GetBoolean("logs", "key_remap", false);

    size_t pos;
    uint32_t firstKey, secondKey, firstVal, secondVal;
    for (const auto& key : g_INI.Keys("key_remap")) {
        pos = key.find('_');
        if (pos != std::string::npos) {
            firstKey = static_cast<uint32_t>(std::stoul(key.substr(0, pos), nullptr, 16));
            secondKey = static_cast<uint32_t>(std::stoul(key.substr(pos + 1), nullptr, 16));
            
            std::string valStr = g_INI.GetString("key_remap", key, "");
            firstVal = static_cast<uint32_t>(std::stoul(valStr, nullptr, 16));
            pos = valStr.find('_');
            if (pos != std::string::npos) {
                secondVal = static_cast<uint32_t>(std::stoul(valStr.substr(pos + 1), nullptr, 16));
                A2ARemapData[std::make_pair(firstKey, secondKey)] = std::make_pair(firstVal, secondVal);
            } else {
                D2DRemapData[std::make_pair(firstKey, secondKey)] = firstVal;
            }
        }
    }

    if (logKeyRemap || !D2DRemapData.empty()) {
        MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_D2D_MAPPING_ADDR), &hook_D2DMapping,
                        reinterpret_cast<LPVOID*>(&fp_D2DMapping));
    }

    if (logKeyRemap || !A2ARemapData.empty()) {
        MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_A2A_MAPPING_ADDR), &hook_A2AMapping,
                        reinterpret_cast<LPVOID*>(&fp_A2AMapping));
    }
}
