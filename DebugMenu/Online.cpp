#include "pch.h"
#include "MemoryPatch/MemoryPatch.h"
#include "Online.h"

constexpr uintptr_t HOOK_LOCK_ON_PLAYER_ADDR = 0x1409c6634;

static uintptr_t* const pWorldChrMan = reinterpret_cast<uintptr_t*>(0x143d7a1e0);
static uintptr_t* const pNPCPlayer = reinterpret_cast<uintptr_t*>(0x143d7a388);
static uintptr_t* const pSession = reinterpret_cast<uintptr_t*>(0x143d67c90);

typedef uintptr_t(*t_getChrIns)(uintptr_t);
static t_getChrIns fpGetChrIns = reinterpret_cast<t_getChrIns>(0x1409c31f0);

typedef int64_t(*t_lobbyResult)(void*, uint64_t*);
static t_lobbyResult fpCreateLobbyResult = nullptr;

typedef int32_t(*t_sub_1410731f0)(uintptr_t);
static t_sub_1410731f0 fp_sub_1410731f0 = nullptr;

typedef int32_t(*t_sub_1409f0230)(uintptr_t);
static t_sub_1409f0230 fp_sub_1409f0230 = nullptr;

typedef bool(*t_sub_140694c70)(uintptr_t, uintptr_t);
static t_sub_140694c70 fp_sub_140694c70 = nullptr;

typedef bool(*t_get_state)();
static t_get_state fp_sub_140a91290 = nullptr;
static t_get_state fp_sub_140a90030 = reinterpret_cast<t_get_state>(0x140a90030);

int64_t HookedCreateLobbyResult(void* arg1, uint64_t* arg2)
{
    if (arg2[1]) {
        std::cout << std::dec << "[online] lobby_id: " << arg2[1] << std::endl;
    } else {
        std::cout << "[online] create lobby failed" << std::endl;
    }
    return fpCreateLobbyResult(arg1, arg2);
}

bool HookedLockOnPlayer(uintptr_t arg1)
{
    if (*(uint8_t*)(arg1 + 0x72) & 1) return true;

    if (*(uint8_t*)(arg1 + 0x72) & 2) {
        uintptr_t chr = fpGetChrIns(arg1);
        if (!chr) return true;

        uint16_t net = *(uint16_t*)(chr + 0x78);
        if (!net) return true;

        if (*(uintptr_t*)(*pWorldChrMan + 0x518 + (net >> 11) * 8)
            != *pWorldChrMan + 0x40) return true;
    }

    return false;
}

int32_t hook_sub_1410731f0(uintptr_t arg1)
{
    if (*(uint32_t*)(arg1 + 0x184) == 3) {
        // State : Host
        return 0;
    }
    return fp_sub_1410731f0(arg1);
}

int32_t hook_sub_1409f0230(uintptr_t arg1)
{
    if (*(uintptr_t*)(*pNPCPlayer + 0x160) == arg1) {
        // Highest priority
        return 0x7FFF;
    }
    return fp_sub_1409f0230(arg1);
}

bool hook_sub_140694c70(uintptr_t arg1, uintptr_t arg2)
{
    if (arg1 == 0x143afd560 && *(uint32_t*)(*pSession + 0xdc) == 5) {
        // Skip BlockClear2
        return false;
    }
    return fp_sub_140694c70(arg1, arg2);
}

bool hook_sub_140a91290()
{
    if (*(uint32_t*)(*pSession + 0xdc) == 4) {
        return fp_sub_140a90030();
    }
    return fp_sub_140a91290();
}

void EnableOnline(uint64_t lobbyId)
{
    PatchOnline(lobbyId);

    MH_CreateHook(reinterpret_cast<LPVOID>(0x1409f0230), &hook_sub_1409f0230,
                    reinterpret_cast<LPVOID*>(&fp_sub_1409f0230));

    if (MH_CreateHook(reinterpret_cast<LPVOID>(HOOK_LOCK_ON_PLAYER_ADDR), &HookedLockOnPlayer, NULL) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(HOOK_LOCK_ON_PLAYER_ADDR));
        PatchLockOnPlayerHook(HOOK_LOCK_ON_PLAYER_ADDR);
    }

    if (lobbyId == 0) {
        MH_CreateHook(reinterpret_cast<LPVOID>(0x141c0cbe0), &HookedCreateLobbyResult,
                    reinterpret_cast<LPVOID*>(&fpCreateLobbyResult));
        MH_CreateHook(reinterpret_cast<LPVOID>(0x1410731f0), &hook_sub_1410731f0,
                    reinterpret_cast<LPVOID*>(&fp_sub_1410731f0));
        MH_CreateHook(reinterpret_cast<LPVOID>(0x140694c70), &hook_sub_140694c70,
                    reinterpret_cast<LPVOID*>(&fp_sub_140694c70));
    } else {
        MH_CreateHook(reinterpret_cast<LPVOID>(0x140a91290), &hook_sub_140a91290,
                    reinterpret_cast<LPVOID*>(&fp_sub_140a91290));
    }
}
