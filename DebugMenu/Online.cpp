#include "pch.h"
#include "MemoryPatch/MemoryPatch.h"
#include "Online.h"

typedef int64_t(*t_sub_1410731f0)(uintptr_t);
static t_sub_1410731f0 fp_sub_1410731f0 = nullptr;

typedef int64_t(*t_lobbyResult)(void*, uint64_t*);
static t_lobbyResult fpCreateLobbyResult = nullptr;

int64_t HookedCreateLobbyResult(void* arg1, uint64_t* arg2)
{
    if (arg2[1]) {
        std::cout << "[online] lobby_id: " << arg2[1] << std::endl;
    } else {
        std::cout << "[online] create lobby failed" << std::endl;
    }
    return fpCreateLobbyResult(arg1, arg2);
}

int64_t hook_sub_1410731f0(uintptr_t arg1)
{
    if (*(uint32_t*)(arg1 + 0x184) == 3) {
        // State : Host
        return 0;
    }
    return fp_sub_1410731f0(arg1);
}

void EnableOnline(int64_t lobbyId)
{
    PatchOnline(lobbyId);

    if (lobbyId == 0) {
        MH_CreateHook(reinterpret_cast<LPVOID>(0x141c0cbe0), &HookedCreateLobbyResult,
                    reinterpret_cast<LPVOID*>(&fpCreateLobbyResult));
        MH_CreateHook(reinterpret_cast<LPVOID>(0x1410731f0), &hook_sub_1410731f0,
                    reinterpret_cast<LPVOID*>(&fp_sub_1410731f0));
    }
}
