#include "pch.h"
#include "MemoryPatch/MemoryPatch.h"
#include "Online.h"

static uint64_t lobbyId;

typedef int64_t(*t_lobbyResult)(void*, uint64_t*);
static t_lobbyResult fpCreateLobbyResult = nullptr;
static t_lobbyResult fpJoinLobbyResult = nullptr;

int64_t HookedCreateLobbyResult(void* arg1, uint64_t* arg2)
{
    if (arg2[1]) {
        std::cout << "[online] lobby_id: " << arg2[1] << std::endl;
    } else {
        std::cout << "[online] create lobby failed" << std::endl;
    }
    return fpCreateLobbyResult(arg1, arg2);
}

int64_t HookedJoinLobbyResult(void* arg1, uint64_t* arg2)
{
    std::cout << "[online] join lobby: " << arg2[0] << " " << arg2[1] << " " << arg2[2] << std::endl;
    return fpJoinLobbyResult(arg1, arg2);
}

void EnableOnline(const INIReader& ini)
{
    lobbyId = ini.GetUnsigned64("online", "lobby_id", 0);
    if (lobbyId) PatchOnlineClient(lobbyId);

    MH_CreateHook(reinterpret_cast<LPVOID>(0x141c0cbe0), &HookedCreateLobbyResult,
                    reinterpret_cast<LPVOID*>(&fpCreateLobbyResult));
    MH_CreateHook(reinterpret_cast<LPVOID>(0x141c0cc40), &HookedJoinLobbyResult,
                    reinterpret_cast<LPVOID*>(&fpJoinLobbyResult));
}
