#include "pch.h"
#include "ThirdParty/minHook/include/MinHook.h"
#include "MemoryPatch/MemoryPatch.h"
#include "InputProcess.h"
#include <map>

constexpr uintptr_t HOOK_KEY_MAPPING_ADDR = 0x142600d50;
constexpr uintptr_t HOOK_NPC_ANIM_ADDR = 0x1407e385b;

static std::map<std::pair<size_t, size_t>, size_t> keyRemap;

static std::unordered_map<uint32_t, std::vector<uint32_t>> animKeys;
static std::map<std::pair<uint32_t, uint32_t>, uint32_t> animKeyNewAnims;

typedef size_t(*t_KeyMapping)(void*, size_t, size_t, size_t);
static t_KeyMapping fpKeyMapping = nullptr;

typedef uintptr_t(*t_sub_1407e3ea0)(void*);
static t_sub_1407e3ea0 fp_sub_1407e3ea0 = reinterpret_cast<t_sub_1407e3ea0>(0x1407e3ea0);

extern bool g_log_key_remap;

size_t HookedKeyMapping(void* arg1, size_t arg2, size_t arg3, size_t arg4)
{
    size_t key = arg3;
    auto it = keyRemap.find({arg2, arg3});
    if (it != keyRemap.end()) {
        key = it->second;
        if (g_log_key_remap) {
            std::cout << "[KeyMapping] " << std::hex << arg2 << " " << arg3 << " remap to " << key << std::endl;
        }
    } else if (g_log_key_remap) {
        std::cout << "[KeyMapping] " << std::hex << arg2 << " " << arg3 << std::endl;
    }

    return fpKeyMapping(arg1, arg2, key, arg4);
}

uintptr_t HookedNpcAnim(void* arg1, uint32_t arg2)
{
    uintptr_t result = fp_sub_1407e3ea0(arg1);
    uint32_t* pAnim = (uint32_t*)(result + 0x170);

    auto it = animKeys.find(arg2);
    if (it != animKeys.end()) {
        for (uint32_t val : it->second) {
            if ((GetAsyncKeyState(val) & 0x8000) != 0) {
                auto a = animKeyNewAnims.find({arg2, val});
                if (a != animKeyNewAnims.end()) {
                    *pAnim = a->second;
                    return result;
                }
            }
        }
    }

    *pAnim = arg2;
    return result;
}

void EnableInputProcess()
{
    WCHAR keyRemapSection[MAX_SECTION_SIZE];
    if (GetPrivateProfileSectionW(L"key_remap", keyRemapSection, MAX_SECTION_SIZE, L".\\mod_engine.ini")) {
        for (const WCHAR* pCurrent = keyRemapSection; *pCurrent; pCurrent += wcslen(pCurrent) + 1) {
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

    WCHAR npcAnimSection[MAX_SECTION_SIZE];
    if (GetPrivateProfileSectionW(L"npc_anim_change", npcAnimSection, MAX_SECTION_SIZE, L".\\mod_engine.ini")) {
        for (const WCHAR* pCurrent = npcAnimSection; *pCurrent; pCurrent += wcslen(pCurrent) + 1) {
            std::wstring_view line(pCurrent);

            size_t equalsPos = line.find(L'=');
            if (equalsPos != std::wstring_view::npos) {
                std::wstring_view keyStr = line.substr(0, equalsPos);
                std::wstring valStr(line.substr(equalsPos + 1));

                size_t underscorePos = keyStr.find(L'_');
                if (underscorePos != std::wstring_view::npos) {
                    std::wstring secondPart(keyStr.substr(underscorePos + 1));

                    uint32_t firstVal = static_cast<uint32_t>(*pCurrent);
                    uint32_t secondVal = static_cast<uint32_t>(std::stoul(secondPart));
                    uint32_t newVal = static_cast<uint32_t>(std::stoul(valStr));

                    animKeys[secondVal].push_back(firstVal);
                    animKeyNewAnims[std::make_pair(secondVal, firstVal)] = newVal;
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

    if (!animKeys.empty()) {
        if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_ADDR), &HookedNpcAnim, NULL) == MH_OK) {
            MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_ADDR));
            PatchNpcAnimHook(HOOK_NPC_ANIM_ADDR);
        }
    }
}
