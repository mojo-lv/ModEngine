#include "pch.h"
#include "Misc.h"

static uintptr_t* const pWorldChrMan = reinterpret_cast<uintptr_t*>(0x143d7a1e0);
static uintptr_t* const pActPanMan = reinterpret_cast<uintptr_t*>(0x143d78048);
static uintptr_t* const pNPCPlayer = reinterpret_cast<uintptr_t*>(0x143d7a388);
static uintptr_t* const pPlayerState = reinterpret_cast<uintptr_t*>(0x143d59938);

typedef size_t(*t_sub_14115ccc0)(wchar_t*, size_t);
static t_sub_14115ccc0 fp_sub_14115ccc0 = nullptr;

typedef int64_t(*t_sub_1410d3120)(uintptr_t*, uint32_t);
static t_sub_1410d3120 fp_sub_1410d3120 = nullptr;

typedef int64_t(*t_sub_1409e9d10)(uintptr_t, uint32_t, void*, void*);
static t_sub_1409e9d10 fp_sub_1409e9d10 = nullptr;

typedef float*(*t_sub_140731030)(uintptr_t, void*);
static t_sub_140731030 fp_sub_140731030 = nullptr;

static std::unordered_map<std::wstring, size_t> va_size;
static std::unordered_map<uint32_t, uint16_t> ca_model;
static std::unordered_map<uint32_t, uint32_t> npc_se;

size_t hook_sub_14115ccc0(wchar_t* arg1, size_t arg2)
{
    std::wstring key(arg1);
    auto it = va_size.find(key);
    if (it != va_size.end()) {
        return it->second;
    }

    return fp_sub_14115ccc0(arg1, arg2);
}

int64_t hook_sub_1410d3120(uintptr_t* arg1, uint32_t arg2)
{
    MH_DisableHook(reinterpret_cast<LPVOID>(0x1410d3120));

    for (const auto& [ca, model] : ca_model) {
        fp_sub_1410d3120(arg1, ca);
        if (arg1[1] != 0) {
            *(uint16_t*)(arg1[1] + 0xb8) = model;
        }
    }
    return fp_sub_1410d3120(arg1, arg2);
}

int64_t hook_sub_1409e9d10(uintptr_t arg1, uint32_t arg2, void* arg3, void* arg4)
{
    if (*(uintptr_t*)(*pNPCPlayer + 0x160) && *(uintptr_t*)(*pWorldChrMan + 0x88) != arg1) {
        auto it = npc_se.find(arg2);
        if (it != npc_se.end()) {
            arg2 = it->second;
        }
    }
    return fp_sub_1409e9d10(arg1, arg2, arg3, arg4);
}

float* hook_sub_140731030(uintptr_t arg1, void* arg2)
{
    static float lastHeight, lastSpeed;
    static bool lastCrouch = false;

    float* result = fp_sub_140731030(arg1, arg2);
    if (*(uintptr_t*)(*pNPCPlayer + 0x160)) return result;

    // state 2: Crouch, state 41: Deathblow
    uint32_t state = *(uint32_t*)(*pPlayerState + 0x3ec);
    if (!lastCrouch && (state != 2) && (state != 41)) return result;

    uintptr_t actPtr = *pActPanMan;
    if (actPtr) {
        actPtr = *(uintptr_t*)(actPtr + 0x10);
        if (actPtr) {
            float height = *(float*)(actPtr + 4) + *(float*)(arg1 + 0xc4) - 1.3f;
            if (state == 2) {
                result[1] = height;
                lastHeight = height;
                lastSpeed = 0.0f;
                lastCrouch = true;
            } else if (lastCrouch) {
                float speed = height - lastHeight;
                if (speed > lastSpeed) lastSpeed = speed;
                lastHeight += lastSpeed;

                if (result[1] > lastHeight) {
                    result[1] = lastHeight;
                } else {
                    lastCrouch = false;
                }
            } else if (state == 41) {
                if (result[1] < height) {
                    result[1] = height;
                }
            }
            return result;
        }
    }

    lastCrouch = false;
    return result;
}

void ApplyMisc(const INIReader& ini)
{
    for (const auto& key : ini.Keys("virtual_alloc_size")) {
        size_t size = ini.GetUnsigned64("virtual_alloc_size", key, 0);
        if (size != 0) {
            va_size[std::wstring(key.begin(), key.end())] = size;
        }
    }

    for (const auto& key : ini.Keys("combat_art_model")) {
        uint32_t ca = std::stoul(key, nullptr);
        uint16_t model = static_cast<uint16_t>(ini.GetUnsigned("combat_art_model", key, 0));
        if (model != 0) {
            ca_model[ca] = model;
        }
    }

    for (const auto& key : ini.Keys("npc_special_effect")) {
        uint32_t old_se = std::stoul(key, nullptr);
        uint32_t new_se = ini.GetUnsigned("npc_special_effect", key, 0);
        if (new_se != 0) {
            npc_se[old_se] = new_se;
        }
    }

    if (!va_size.empty()) {
        MH_CreateHook(reinterpret_cast<LPVOID>(0x14115ccc0), &hook_sub_14115ccc0, 
                        reinterpret_cast<LPVOID*>(&fp_sub_14115ccc0));
    }

    if (!ca_model.empty()) {
        MH_CreateHook(reinterpret_cast<LPVOID>(0x1410d3120), &hook_sub_1410d3120, 
                    reinterpret_cast<LPVOID*>(&fp_sub_1410d3120));
    }

    if (!npc_se.empty()) {
        MH_CreateHook(reinterpret_cast<LPVOID>(0x1409e9d10), &hook_sub_1409e9d10, 
                        reinterpret_cast<LPVOID*>(&fp_sub_1409e9d10));
    }

    MH_CreateHook(reinterpret_cast<LPVOID>(0x140731030), &hook_sub_140731030, 
                    reinterpret_cast<LPVOID*>(&fp_sub_140731030));
}
