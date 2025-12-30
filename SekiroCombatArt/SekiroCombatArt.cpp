#include "pch.h"
#include "SekiroCombatArt.h"
#include "MinHook/MinHook.h"

#define PROCESS_INPUTS 0x140B2C190

static t_ProcessInputs fpProcessInputs = nullptr;
static int combat_art_key;
static bool g_block = false;

size_t HookedProcessInputs(byte* p1, void* p2)
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

    if (GetAsyncKeyState(combat_art_key) & 0x8000) {
        *current_keys_raw |= 5;
    }

    return fpProcessInputs(p1, p2);
}

void SetComatArtKey()
{
    UINT key = GetPrivateProfileIntW(L"misc", L"combat_art_key", 0, L".\\mod_engine.ini");
    if (key != 0) {
        combat_art_key = key;
        MH_CreateHook(reinterpret_cast<LPVOID>(PROCESS_INPUTS), &HookedProcessInputs, 
                        reinterpret_cast<LPVOID*>(&fpProcessInputs));
    }
}
