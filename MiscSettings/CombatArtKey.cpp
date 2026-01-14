#include "pch.h"
#include "ThirdParty/minhook/include/MinHook.h"
#include "CombatArtKey.h"

#define PROCESS_INPUTS 0x140B2C190

typedef size_t(*t_ProcessInputs)(uintptr_t, void*);
static t_ProcessInputs fpProcessInputs = nullptr;

enum InputFlags {
    Attack = 1,
    Block = 4,
    CombatArt = 5,
};

const int COMBAT_ART_COOLDOWN = 45;

static int combat_art_key;
static int g_cool_down = 0;
static bool g_cool_down_enable = false;
static bool g_block = false;

size_t HookedProcessInputs(uintptr_t p1, void* p2)
{
    uint64_t* current_keys_raw = reinterpret_cast<uint64_t*>(p1 + 0x10);
    if ((*current_keys_raw & CombatArt) == CombatArt) {
        *current_keys_raw -= Block;
        g_block = true;
    } else if (g_block) {
        if ((*current_keys_raw & Block) == Block) {
            *current_keys_raw -= Block;
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
        *current_keys_raw |= CombatArt;
        uint64_t* prev_keys_raw = reinterpret_cast<uint64_t*>(p1 + 0x18);
        if ((*prev_keys_raw & CombatArt) == 0) {
            g_cool_down = COMBAT_ART_COOLDOWN;
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
