#include "pch.h"
#include <filesystem>  // C++17
#include <unordered_map>
#include <algorithm>
#include <cwctype>
#include "ModLoader.h"
#include "MinHook/MinHook.h"

// 405556415441554883ec284d8be0
#define GET_SEKIRO_PATH_ADDR 0x1401c76d0

namespace fs = std::filesystem;

static t_GetSekiroPath fpGetSekiroPath = nullptr;
static t_CreateFileW fpCreateFileW = nullptr;

static std::unordered_map<std::wstring, std::wstring> rel_to_index;
static std::unordered_map<std::wstring, std::wstring> index_to_dir;

static void ScanModDir(const WCHAR* modRel, int index) {
    WCHAR buffer[4];
    swprintf(buffer, 4, L"_%02x", index);
    std::wstring wstr_index(buffer);

    WCHAR dir[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, dir);
    fs::path curDirPath(dir);

    lstrcatW(dir, modRel);
    fs::path pDir(dir);
    fs::path modDirPath = pDir.lexically_normal();

    fs::path pModRel(modRel);
    fs::path modRelPath = pModRel.lexically_normal();
    if (!fs::exists(modDirPath) || !fs::is_directory(modDirPath)) {
        return;
    }

    index_to_dir.emplace(wstr_index, modRelPath.wstring());

    int len = modDirPath.generic_wstring().length();
    if (modDirPath.generic_wstring().back() == L'/') {
        len--;
    }
    for (auto const& p : fs::recursive_directory_iterator(modDirPath)) {
        if (!p.is_directory()) {
            std::wstring rel = p.path().generic_wstring().substr(len + 1);
            std::transform(rel.begin(), rel.end(), rel.begin(), std::towlower);
            rel_to_index.try_emplace(rel, wstr_index);
        }
    }
}

static void ReplacePrefixWithIndex(WCHAR* string)
{
    WCHAR* rel = wcschr(string, L'/');
    if (rel && rel > string + 4) {
        WCHAR* rel2 = wcsrchr(string, L'\\');
        if (rel2) rel = rel2;
        auto index = rel_to_index.find(std::wstring(rel + 1));

        if (index != rel_to_index.end()) {
            int i = 0;
            WCHAR* p = string;
            while (p < rel + 1) {
                if (i == 0) {
                    *p = L'.';
                } else if (i > 1 && i < 5) {
                    *p = index->second[i - 2];
                } else {
                    *p = L'/';
                }
                i++;
                p++;
            }
        }
    }
}

void* HookedGetSekiroPath(SekiroPath* p1, void* p2, void* p3, void* p4, void* p5, void* p6)
{
    fpGetSekiroPath(p1, p2, p3, p4, p5, p6);
    WCHAR* path = p1->path;
    UINT64 len = p1->length;
    if (len > 6 && path[len - 1] != L'/') {
        ReplacePrefixWithIndex(path);
    }

    return p1;
}

HANDLE WINAPI HookedCreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile)
{
    if (lpFileName) {
        WCHAR dir[MAX_PATH];
        GetCurrentDirectoryW(MAX_PATH, dir);
        std::wstring filePath(lpFileName);
        std::wstring curDir(dir);
        size_t dir_len = curDir.length();
        size_t path_len = filePath.length();
        if ((path_len > dir_len + 1) && (filePath[dir_len + 1] == L'_')) {
            if ((filePath[dir_len] == L'\\') && (filePath.back() != L'\\')) {
                auto it = index_to_dir.find(filePath.substr(dir_len + 1, 3));
                if (it != index_to_dir.end()) {
                    filePath.replace(dir_len + 1, 3, it->second);
                    return fpCreateFileW(filePath.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
                        dwFlagsAndAttributes, hTemplateFile);
                }
            }
        }
    }

    return fpCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
        dwFlagsAndAttributes, hTemplateFile);
}

void LoadMods()
{
    const DWORD size = 2048;
    WCHAR section[size];
    if (GetPrivateProfileSectionW(L"mods", section, size, L".\\modengine.ini")) {
        WCHAR* pCurrent = section;
        WCHAR* pEnd = nullptr;

        int dirIndex = 0;
        while ((*pCurrent != L'\0') && (dirIndex < 256)) {
            if (*pCurrent == L'"') {
                *pCurrent = L'\\';
                pEnd = wcschr(pCurrent, L'"');
                if (pEnd && pEnd > pCurrent + 1) {
                    *pEnd = L'\0';
                    ScanModDir(pCurrent, dirIndex);
                    *pEnd = L'"';
                    dirIndex++;
                }
            }

            pCurrent += wcslen(pCurrent) + 1;
        }
    }

    MH_Initialize();
    if (MH_CreateHook(reinterpret_cast<LPVOID>(GET_SEKIRO_PATH_ADDR), &HookedGetSekiroPath, 
                      reinterpret_cast<LPVOID*>(&fpGetSekiroPath)) == MH_OK) {
        MH_EnableHook(reinterpret_cast<LPVOID>(GET_SEKIRO_PATH_ADDR));
    }

    if (MH_CreateHookApi(L"kernel32", "CreateFileW", &HookedCreateFileW, 
                      reinterpret_cast<LPVOID*>(&fpCreateFileW)) == MH_OK) {
        MH_EnableHook((LPVOID)GetProcAddress(GetModuleHandleW(L"kernel32"), "CreateFileW"));
    }
}
