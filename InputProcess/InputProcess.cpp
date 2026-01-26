#include "pch.h"
#include "ThirdParty/minHook/include/MinHook.h"
#include "InputProcess.h"
#include <map>

constexpr uintptr_t HOOK_KEY_MAPPING_ADDR = 0x142600d50;

static std::map<std::pair<size_t, size_t>, size_t> keyRemap;

typedef size_t(*t_KeyMapping)(void*, size_t, size_t, size_t);
static t_KeyMapping fpKeyMapping = nullptr;

extern bool g_log_key_remap;

size_t HookedKeyMapping(void* arg1, size_t arg2, size_t arg3, size_t arg4)
{
    auto it = keyRemap.find({arg2, arg3});
    if(it != keyRemap.end()) {
        if (g_log_key_remap) {
            std::cout << "[KeyMapping] " << std::hex << arg2 << " " << arg3 << " remap to " << it->second << std::endl;
        }
        return fpKeyMapping(arg1, arg2, it->second, arg4);
    }

    if (g_log_key_remap) {
        std::cout << "[KeyMapping] " << std::hex << arg2 << " " << arg3 << std::endl;
    }
    return fpKeyMapping(arg1, arg2, arg3, arg4);
}

void EnableInputProcess()
{
    WCHAR section[MAX_SECTION_SIZE];
    if (GetPrivateProfileSectionW(L"key_remap", section, MAX_SECTION_SIZE, L".\\mod_engine.ini")) {
        for (const WCHAR* pCurrent = section; *pCurrent; pCurrent += wcslen(pCurrent) + 1) {
            std::wstring_view line(pCurrent);

            size_t equalsPos = line.find(L'=');
            if (equalsPos != std::wstring_view::npos) {
                std::wstring_view keyStr = line.substr(0, equalsPos);
                std::wstring valStr(line.substr(equalsPos + 1));

                size_t underscorePos = keyStr.find(L'_');
                if (underscorePos != std::wstring_view::npos) {
                    std::wstring firstPart(keyStr.substr(0, underscorePos));
                    std::wstring secondPart(keyStr.substr(underscorePos + 1));

                    size_t firstVal = std::stoul(firstPart, nullptr, 16);
                    size_t secondVal = std::stoul(secondPart, nullptr, 16);
                    size_t mappedVal = std::stoul(valStr, nullptr, 16);

                    keyRemap[std::make_pair(firstVal, secondVal)] = mappedVal;
                }
            }
        }
    }

    if (!keyRemap.empty()) {
        if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_KEY_MAPPING_ADDR), &HookedKeyMapping,
                            reinterpret_cast<LPVOID*>(&fpKeyMapping)) == MH_OK) {
            MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_KEY_MAPPING_ADDR));
        }
    }
}
