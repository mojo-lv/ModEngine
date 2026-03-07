#include "pch.h"
#include "KeyRemap.h"

typedef void*(*t_sub_142600d50)(void*, uint32_t, uint32_t, void*);
static t_sub_142600d50 fp_sub_142600d50 = nullptr;

static std::map<std::pair<uint32_t, uint32_t>, uint32_t> keyRemapData;

static bool logKeyRemap = false;

extern INIReader g_INI;

void* hook_sub_142600d50(void* arg1, uint32_t arg2, uint32_t arg3, void* arg4)
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

    return fp_sub_142600d50(arg1, arg2, key, arg4);
}

void EnableKeyRemap()
{
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
        MH_CreateHook(reinterpret_cast<LPVOID>(0x142600d50), &hook_sub_142600d50,
                        reinterpret_cast<LPVOID*>(&fp_sub_142600d50));
    }
}
