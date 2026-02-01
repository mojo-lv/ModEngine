#include "pch.h"
#include "ThirdParty/minHook/include/MinHook.h"
#include "MemoryPatch/MemoryPatch.h"
#include "InputProcess.h"

constexpr uintptr_t HOOK_KEY_MAPPING_ADDR = 0x142600d50;
constexpr uintptr_t HOOK_NPC_ANIM_ADDR = 0x1407e385b;

struct KeyHash {
    size_t operator()(const std::pair<uint32_t, uint32_t>& k) const {
        size_t h1 = std::hash<uint32_t>()(k.first);
        size_t h2 = std::hash<uint32_t>()(k.second);
        return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
};

static std::unordered_map<std::pair<uint32_t, uint32_t>, uint32_t, KeyHash> keyRemap;

static std::unordered_map<uint32_t, uint32_t> forceAnims;
static std::unordered_map<uint32_t, std::vector<uint32_t>> animKeys;
static std::unordered_map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>, KeyHash> animKeyNewAnims;

typedef void*(*t_KeyMapping)(void*, uint32_t, uint32_t, void*);
static t_KeyMapping fpKeyMapping = nullptr;

typedef uintptr_t(*t_sub_1407e3ea0)(void*);
static t_sub_1407e3ea0 fp_sub_1407e3ea0 = reinterpret_cast<t_sub_1407e3ea0>(0x1407e3ea0);

extern bool g_log_key_remap;

static uint32_t lastAnim = UINT32_MAX;
static int vKey = 0;
static int comboIndex = 0;
static std::vector<uint32_t>* comboAnimsPtr = nullptr;
static bool blockCombo = false;

void* HookedKeyMapping(void* arg1, uint32_t arg2, uint32_t arg3, void* arg4)
{
    uint32_t key = arg3;
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
    uint32_t* pAnim = reinterpret_cast<uint32_t*>(result + 0x170);

    if (arg2 == UINT32_MAX) {
        blockCombo = false;
        if ((lastAnim != UINT32_MAX) && ((GetAsyncKeyState(vKey) & 0x8000) == 0)) {
            lastAnim = UINT32_MAX;
        }
        *pAnim = UINT32_MAX;
        return result;
    }

    auto f = forceAnims.find(arg2);
    if (f != forceAnims.end()) {
        lastAnim = UINT32_MAX;
        *pAnim = f->second;
        return result;
    }

    auto it = animKeys.find(arg2);
    if (it != animKeys.end()) {
        for (uint32_t val : it->second) {
            if ((GetAsyncKeyState(val) & 0x8000) != 0) {
                if (lastAnim == arg2 && vKey == val) {
                    if (!blockCombo) {
                        comboIndex = (comboIndex + 1) % comboAnimsPtr->size();
                        blockCombo = true;
                    }

                    *pAnim = (*comboAnimsPtr)[comboIndex];
                    return result;
                }

                auto a = animKeyNewAnims.find({arg2, val});
                if (a != animKeyNewAnims.end()) {
                    lastAnim = arg2;
                    vKey = val;
                    comboIndex = 0;
                    comboAnimsPtr = &(a->second);
                    blockCombo = true;
                    *pAnim = comboAnimsPtr->front();
                    return result;
                }
            }
        }
    }

    lastAnim = UINT32_MAX;
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

                    uint32_t firstVal = static_cast<uint32_t>(std::stoul(firstPart, nullptr, 16));
                    uint32_t secondVal = static_cast<uint32_t>(std::stoul(secondPart, nullptr, 16));
                    uint32_t mappedVal = static_cast<uint32_t>(std::stoul(valStr, nullptr, 16));

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
                std::wstring_view valStr = line.substr(equalsPos + 1);

                size_t underscorePos = keyStr.find(L'_');
                if (underscorePos != std::wstring_view::npos) {
                    if (underscorePos == 0) {
                        std::wstring secondPart(keyStr.substr(underscorePos + 2));
                        uint32_t secondVal = static_cast<uint32_t>(std::stoul(secondPart));
                        uint32_t newVal = static_cast<uint32_t>(std::stoul(std::wstring(valStr)));
                        forceAnims[secondVal] = newVal;
                    } else {
                        std::wstring secondPart(keyStr.substr(underscorePos + 1));
                        uint32_t firstVal = static_cast<uint32_t>(*pCurrent);
                        uint32_t secondVal = static_cast<uint32_t>(std::stoul(secondPart));

                        size_t start = 0;
                        while (start < valStr.size()) {
                            size_t pos = valStr.find(L'_', start);
                            if (pos == std::wstring_view::npos) {
                                pos = valStr.size();
                            }
                            std::wstring_view token = valStr.substr(start, pos - start);
                            uint32_t newVal = static_cast<uint32_t>(std::stoul(std::wstring(token)));
                            animKeyNewAnims[std::make_pair(secondVal, firstVal)].push_back(newVal);
                            start = pos + 1;
                        }
                        animKeys[secondVal].push_back(firstVal);
                    }
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

    if (!forceAnims.empty() || !animKeys.empty()) {
        if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_ADDR), &HookedNpcAnim, NULL) == MH_OK) {
            MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_ADDR));
            PatchNpcAnimHook(HOOK_NPC_ANIM_ADDR);
        }
    }
}
