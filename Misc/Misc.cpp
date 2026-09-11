#include "pch.h"
#include "Misc.h"

static uintptr_t* const pWorldChrMan = reinterpret_cast<uintptr_t*>(0x143d7a1e0);
static uintptr_t* const pActPanMan = reinterpret_cast<uintptr_t*>(0x143d78048);
static uintptr_t* const pNPCPlayer = reinterpret_cast<uintptr_t*>(0x143d7a388);

typedef size_t(*t_sub_14115ccc0)(wchar_t*, size_t);
static t_sub_14115ccc0 fp_sub_14115ccc0 = nullptr;

typedef float*(*t_sub_140731030)(uintptr_t, void*);
static t_sub_140731030 fp_sub_140731030 = nullptr;

static std::unordered_map<std::wstring, size_t> va_size;

size_t hook_sub_14115ccc0(wchar_t* arg1, size_t arg2)
{
    std::wstring key(arg1);
    auto it = va_size.find(key);
    if (it != va_size.end()) {
        return it->second;
    }

    return fp_sub_14115ccc0(arg1, arg2);
}

float* hook_sub_140731030(uintptr_t arg1, void* arg2)
{
    static uint8_t* crouch = (uint8_t*)(*(uintptr_t*)(*(uintptr_t*)(*(uintptr_t*)(
                                *pWorldChrMan + 0x88) + 0x1ff8) + 0xb8) + 0x320);

    float* result = fp_sub_140731030(arg1, arg2);
    if (*(uintptr_t*)(*pNPCPlayer + 0x160)) return result;

    uintptr_t actPtr = *pActPanMan;
    if (actPtr) {
        actPtr = *(uintptr_t*)(actPtr + 0x10);
        if (actPtr) {
            float height = *(float*)(actPtr + 4) + *(float*)(arg1 + 0xc4) - 1.3f;
            if ((*crouch == 1) || (result[1] < height)) {
                result[1] = height;
            }
            return result;
        }
    }

    crouch = (uint8_t*)(*(uintptr_t*)(*(uintptr_t*)(*(uintptr_t*)(
                *pWorldChrMan + 0x88) + 0x1ff8) + 0xb8) + 0x320);
    return result;
}

void ApplyMisc(const INIReader& ini)
{
    size_t size;
    for (const auto& key : ini.Keys("virtual_alloc_size")) {
        size = ini.GetUnsigned64("virtual_alloc_size", key, 0);
        if (size != 0) {
            va_size[std::wstring(key.begin(), key.end())] = size;
        }
    }

    if (!va_size.empty()) {
        MH_CreateHook(reinterpret_cast<LPVOID>(0x14115ccc0), &hook_sub_14115ccc0, 
                        reinterpret_cast<LPVOID*>(&fp_sub_14115ccc0));
    }

    MH_CreateHook(reinterpret_cast<LPVOID>(0x140731030), &hook_sub_140731030, 
                    reinterpret_cast<LPVOID*>(&fp_sub_140731030));
}
