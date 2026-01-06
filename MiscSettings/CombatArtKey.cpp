#include "pch.h"
#include "CombatArtKey.h"
#include "MinHook/MinHook.h"

#define PROCESS_INPUTS 0x140B2C190

static t_ProcessInputs fpProcessInputs = nullptr;
static int combat_art_key;
static int g_cool_down = 0;
static bool g_cool_down_enable = false;
static bool g_block = false;

size_t HookedProcessInputs(uintptr_t p1, void* p2)
{
    uint64_t* current_keys_raw = reinterpret_cast<uint64_t*>(p1 + 0x10);
    if ((*current_keys_raw & 5) == 5) {
        *current_keys_raw -= 4;
        g_block = true;
    } else if (g_block) {
        if ((*current_keys_raw & 4) == 4) {
            *current_keys_raw -= 4;
        } else {
            g_block = false;
        }
    }

    if (g_cool_down > 0) {
        g_cool_down--;
        if (g_cool_down_enable) {
            return fpProcessInputs(p1, p2);
        }
    }

    if (GetAsyncKeyState(combat_art_key) & 0x8000) {
        *current_keys_raw |= 5;
        uint64_t* prev_keys_raw = reinterpret_cast<uint64_t*>(p1 + 0x18);
        if ((*prev_keys_raw & 5) == 0) {
            g_cool_down = 45;
            g_cool_down_enable = false;
        }
    } else {
        g_cool_down_enable = true;
    }

    return fpProcessInputs(p1, p2);
}

void SetCombatArtKey()
{
    UINT key = GetPrivateProfileIntW(L"misc", L"combat_art_key", 0, L".\\mod_engine.ini");
    if (key != 0) {
        combat_art_key = key;
        MH_CreateHook(reinterpret_cast<LPVOID>(PROCESS_INPUTS), &HookedProcessInputs, 
                        reinterpret_cast<LPVOID*>(&fpProcessInputs));
    }
}
