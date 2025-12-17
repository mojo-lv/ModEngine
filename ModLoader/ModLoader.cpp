#include "pch.h"
#include "ModLoader.h"
#include "MinHook/MinHook.h"
#include <unordered_map>

// 405556415441554883ec284d8be0
#define GET_SEKIRO_PATH_ADDR 0x1401c76d0

namespace fs = std::filesystem;

static t_GetSekiroPath fpGetSekiroPath = nullptr;
static t_CreateFileW fpCreateFileW = nullptr;

static std::unordered_map<std::wstring, std::wstring> rel_to_index;
static std::unordered_map<std::wstring, std::wstring> index_to_mod;

static size_t g_cur_len;

static bool ScanModsDir()
{
    fs::path currentDir = fs::current_path();
    fs::path modsDir = currentDir / "mods";
    if (!fs::exists(modsDir) || !fs::is_directory(modsDir)) {
        return false;
    }

    g_cur_len = currentDir.wstring().length();

    std::vector<fs::directory_entry> entries;
    for (const auto& entry : fs::directory_iterator(modsDir)) {
        if (entry.is_directory()) {
            entries.push_back(entry);
        }
    }

    if (entries.empty()) {
        return false;
    }

    std::sort(entries.begin(), entries.end(), 
        [](const fs::directory_entry& a, const fs::directory_entry& b) {
            return a.path().filename() < b.path().filename();
        });

    int index = 0;
    WCHAR indexBuffer[3];
    for (const auto& entry : entries) {
        swprintf(indexBuffer, _countof(indexBuffer), L"%02x", (index++) & 0xFF);
        std::wstring indexStr(indexBuffer);
        index_to_mod.emplace(indexStr, entry.path().wstring());
        
        for (const auto& p : fs::recursive_directory_iterator(entry.path())) {
            if (!p.is_directory()) {
                std::wstring rel = p.path().generic_wstring().substr(entry.path().generic_wstring().length() + 1);
                std::transform(rel.begin(), rel.end(), rel.begin(), std::towlower);
                rel_to_index.try_emplace(rel, indexStr);
            }
        }
    }

    if (rel_to_index.empty()) {
        return false;
    }
    return true;
}

static void ReplacePrefixWithIndex(WCHAR* p)
{
    auto index = rel_to_index.find(std::wstring(p + 7));
    if (index != rel_to_index.end()) {
        *p = L'.';
        *(p + 1) = L'/';
        *(p + 2) = L'<';
        *(p + 3) = index->second[0];
        *(p + 4) = index->second[1];
        *(p + 5) = L'>';
    }
}

void* HookedGetSekiroPath(SekiroPath* p1, void* p2, void* p3, void* p4, void* p5, void* p6)
{
    fpGetSekiroPath(p1, p2, p3, p4, p5, p6);
    WCHAR* path = p1->path;
    UINT64 len = p1->length;
    if (len > 7 && path[6] == L'/' && path[5] == L':' && path[0] == L'd') {
        // data1:/
        ReplacePrefixWithIndex(path);
    }

    return p1;
}

HANDLE WINAPI HookedCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile)
{
    if (lpFileName) {
        std::wstring_view path_view(lpFileName);
        if ((path_view.length() > g_cur_len + 5) && (path_view[g_cur_len + 1] == L'<') && (path_view[g_cur_len + 4] == L'>')) {
            std::wstring index_key(path_view.substr(g_cur_len + 2, 2));
            auto it = index_to_mod.find(index_key);
            if (it != index_to_mod.end()) {
                std::wstring new_path = it->second;
                new_path.append(path_view.substr(g_cur_len + 5));
                return fpCreateFileW(new_path.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
                    dwFlagsAndAttributes, hTemplateFile);
            }
        }
    }

    return fpCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
        dwFlagsAndAttributes, hTemplateFile);
}

void LoadMods()
{
    if (!ScanModsDir()) {
        return;
    }

    MH_Initialize();

    if (MH_CreateHook(reinterpret_cast<LPVOID>(GET_SEKIRO_PATH_ADDR), &HookedGetSekiroPath, 
                      reinterpret_cast<LPVOID*>(&fpGetSekiroPath)) != MH_OK) {
        return;
    }

    if (MH_CreateHookApi(L"kernel32", "CreateFileW", &HookedCreateFileW, 
                      reinterpret_cast<LPVOID*>(&fpCreateFileW)) != MH_OK) {
        return;
    }

    MH_EnableHook(reinterpret_cast<LPVOID>(GET_SEKIRO_PATH_ADDR));
    MH_EnableHook((LPVOID)GetProcAddress(GetModuleHandleW(L"kernel32"), "CreateFileW"));
}
