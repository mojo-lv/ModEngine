#include "pch.h"
#include "MyTest.h"
#include "MinHook/MinHook.h"

#define PROCESS_INPUTS 0x140B2C190

static t_ProcessInputs fpProcessInputs = nullptr;
static int hold_key = VK_SHIFT;

size_t HookedProcessInputs(byte* p1, void* p2)
{
    uint64_t* prev_keys_raw = reinterpret_cast<uint64_t*>(p1 + 0x18);
    uint64_t* current_keys_raw = reinterpret_cast<uint64_t*>(p1 + 0x10);

    if (GetAsyncKeyState(hold_key) & 0x8000) {
        if ((*prev_keys_raw == 1074003970) || (*prev_keys_raw == 1074003974)) {
            // UseProsthetic
            *current_keys_raw |= 1074003970;
        } else if (*prev_keys_raw == 4294967301) {
            // CombatArt
            *current_keys_raw |= 4294967301;
        }
    }

    return fpProcessInputs(p1, p2);
}

void Test()
{
    MH_CreateHook(reinterpret_cast<LPVOID>(PROCESS_INPUTS), &HookedProcessInputs, 
                      reinterpret_cast<LPVOID*>(&fpProcessInputs));
}
