#include "pch.h"
#include "MemoryPatch/MemoryPatch.h"
#include "InputProcess.h"
#include <map>

constexpr uintptr_t HOOK_KEY_MAPPING_ADDR = 0x142600d50;
constexpr uintptr_t HOOK_NPC_ANIM_ADDR = 0x1407e385b;

constexpr uint32_t INVALID_ANIM = 0xFFFFFFFF;

static std::map<std::pair<uint32_t, uint32_t>, uint32_t> keyRemap;

static std::unordered_map<uint32_t, std::vector<uint32_t>> animKeys;
static std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> animKeyNewAnims;

typedef void*(*t_KeyMapping)(void*, uint32_t, uint32_t, void*);
static t_KeyMapping fpKeyMapping = nullptr;

typedef uintptr_t(*t_sub_1407e3ea0)(void*);
static t_sub_1407e3ea0 fp_sub_1407e3ea0 = reinterpret_cast<t_sub_1407e3ea0>(0x1407e3ea0);

static bool g_log_key_remap = false;

static uint32_t lastAnim = INVALID_ANIM;
static int vKey = 0;
static int comboIndex = 0;
static std::vector<uint32_t>* comboAnimsPtr = nullptr;
static bool blockCombo = false;
static uint32_t deflectAnim = INVALID_ANIM;
static uint32_t deflectNewAnim = INVALID_ANIM;

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

    if (arg2 == INVALID_ANIM) {
        blockCombo = false;
        if ((lastAnim != INVALID_ANIM) && ((GetAsyncKeyState(vKey) & 0x8000) == 0)) {
            lastAnim = INVALID_ANIM;
        }
        *pAnim = INVALID_ANIM;
        return result;
    }

    if (arg2 == deflectAnim) {
        lastAnim = INVALID_ANIM;
        *pAnim = deflectNewAnim;
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

    lastAnim = INVALID_ANIM;
    *pAnim = arg2;
    return result;
}

void EnableInputProcess()
{
    if (GetPrivateProfileIntW(L"logs", L"key_remap", 0, L".\\mod_engine.ini") != 0) {
        g_log_key_remap= true;
    }

    WCHAR buffer[MAX_SECTION_SIZE];
    if (GetPrivateProfileSectionW(L"key_remap", buffer, MAX_SECTION_SIZE, L".\\mod_engine.ini")) {
        for (const WCHAR* pCurrent = buffer; *pCurrent; pCurrent += wcslen(pCurrent) + 1) {
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

    GetPrivateProfileStringW(L"npc_anim_change", L"npc_deflect", L"", buffer, MAX_SECTION_SIZE, L".\\mod_engine.ini");
    if (lstrlenW(buffer) > 0) {
        std::wstring str(buffer);
        size_t equalsPos = str.find(L'_');
        if (equalsPos != std::wstring::npos) {
            deflectAnim = static_cast<uint32_t>(std::stoul(str.substr(0, equalsPos)));
            deflectNewAnim = static_cast<uint32_t>(std::stoul(str.substr(equalsPos + 1)));
        }
    }

    GetPrivateProfileStringW(L"npc_anim_change", L"npc_anim_config", L"", buffer, MAX_SECTION_SIZE, L".\\mod_engine.ini");
    if (lstrlenW(buffer) > 0) {
        fs::path configPath = fs::current_path() / buffer;
        if (GetPrivateProfileSectionW(L"npc_anim_config", buffer, MAX_SECTION_SIZE, configPath.wstring().c_str())) {
            for (const WCHAR* pCurrent = buffer; *pCurrent; pCurrent += wcslen(pCurrent) + 1) {
                std::wstring_view line(pCurrent);

                size_t equalsPos = line.find(L'=');
                if (equalsPos != std::wstring_view::npos) {
                    std::wstring_view keyStr = line.substr(0, equalsPos);
                    std::wstring_view valStr = line.substr(equalsPos + 1);

                    size_t underscorePos = keyStr.find(L'_');
                    if (underscorePos != std::wstring_view::npos) {
                        std::wstring secondPart(keyStr.substr(underscorePos + 1));
                        uint32_t key = static_cast<uint32_t>(*pCurrent);
                        uint32_t animId = static_cast<uint32_t>(std::stoul(secondPart));

                        size_t start = 0;
                        while (start < valStr.size()) {
                            size_t pos = valStr.find(L'_', start);
                            if (pos == std::wstring_view::npos) {
                                pos = valStr.size();
                            }
                            std::wstring_view token = valStr.substr(start, pos - start);
                            uint32_t newAnimId = static_cast<uint32_t>(std::stoul(std::wstring(token)));
                            animKeyNewAnims[std::make_pair(animId, key)].push_back(newAnimId);
                            start = pos + 1;
                        }
                        animKeys[animId].push_back(key);
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

    if ((deflectAnim != INVALID_ANIM) || !animKeys.empty()) {
        if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_ADDR), &HookedNpcAnim, NULL) == MH_OK) {
            MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_NPC_ANIM_ADDR));
            PatchNpcAnimHook(HOOK_NPC_ANIM_ADDR);
        }
    }
}
