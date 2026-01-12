#include "pch.h"
#include "FilesMod.h"
#include "MinHook/MinHook.h"
#include "MemoryPatch/MemoryPatch.h"
#include <unordered_map>

#define OFFSET_HOOK_GET_SEKIRO_VA_SIZE 0x115CCC0
#define OFFSET_HOOK_GET_SEKIRO_PATH 0x1C76D0

static t_GetSekiroVASize fpGetSekiroVASize = nullptr;
static t_GetSekiroPath fpGetSekiroPath = nullptr;
static t_CreateFileW fpCreateFileW = nullptr;
static t_CopyFileW fpCopyFileW = nullptr;

static std::unordered_map<std::wstring, std::wstring> rel_to_index;
static std::unordered_map<std::wstring, std::wstring> index_to_mod;
static std::unordered_map<std::wstring, size_t> va_size;

static size_t g_cur_len;
static std::wstring g_save_path;
static std::wstring g_cutscene_path;
static const std::wstring g_cutscene_path_temp = L"./<cs>";

static bool ScanModsDir(fs::path modsDir)
{
    if (!fs::exists(modsDir) || !fs::is_directory(modsDir)) {
        return false;
    }

    std::vector<fs::directory_entry> entries;
    for (const auto& p : fs::directory_iterator(modsDir)) {
        if (p.is_directory() && (p.path().filename().wstring().front() != L'_')) {
            entries.push_back(p);
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
        index_to_mod.try_emplace(indexStr, entry.path().wstring());
        
        for (const auto& p : fs::recursive_directory_iterator(entry.path())) {
            if (!p.is_directory()) {
                std::wstring rel = p.path().generic_wstring().substr(entry.path().generic_wstring().length() + 1);
                std::transform(rel.begin(), rel.end(), rel.begin(), std::towlower);
                rel_to_index.try_emplace(rel, indexStr);
            }
        }
    }

    return true;
}

static void ScanDllsDir(fs::path dllsDir)
{
    if (fs::exists(dllsDir) && fs::is_directory(dllsDir)) {
        for (const auto& p : fs::directory_iterator(dllsDir)) {
            if (!p.is_directory() && (p.path().filename().wstring().front() != L'_')) {
                g_LoadedDLLs.push_back(LoadLibraryW(p.path().wstring().c_str()));
            }
        }
    }
}

size_t HookedGetSekiroVASize(LPCWSTR arg1, size_t arg2)
{
    std::wstring key(arg1);
    auto it = va_size.find(key);
    if (it != va_size.end()) {
        return it->second;
    }

    return fpGetSekiroVASize(arg1, arg2);
}

SekiroPath* HookedGetSekiroPath(SekiroPath* p1, void* p2, void* p3, void* p4, void* p5, void* p6)
{
    fpGetSekiroPath(p1, p2, p3, p4, p5, p6);
    WCHAR* path = p1->path;
    UINT64 len = p1->length;
    if (len > 7 && path[6] == L'/' && path[5] == L':' && path[0] == L'd') {
        // data1:/
        auto index = rel_to_index.find(std::wstring(path + 7));
        if (index != rel_to_index.end()) {
            *path = L'.';
            *(path + 1) = L'/';
            *(path + 2) = L'<';
            *(path + 3) = index->second[0];
            *(path + 4) = index->second[1];
            *(path + 5) = L'>';
        } else if ((len == 43) && !g_cutscene_path.empty()) {
            // data1:/cutscene/s11_02_0020.cutscenebnd.dcx
            std::wstring_view path_view(path);
            if (path_view.compare(28, 15, L"cutscenebnd.dcx") == 0) {
                wcsncpy_s(path, len, g_cutscene_path_temp.c_str(), g_cutscene_path_temp.length());
            }
        }
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
            } else if ((path_view[g_cur_len + 2] == L'c') && (path_view[g_cur_len + 3] == L's')) {
                return fpCreateFileW(g_cutscene_path.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
                    dwFlagsAndAttributes, hTemplateFile);
            }
        } else if (!g_save_path.empty()) {
            if (path_view.length() > 9 && path_view.compare(path_view.length() - 9, 9, L"S0000.sl2") == 0) {
                return fpCreateFileW(g_save_path.c_str(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
                    dwFlagsAndAttributes, hTemplateFile);
            }
        }
    }

    return fpCreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition,
        dwFlagsAndAttributes, hTemplateFile);
}

BOOL WINAPI HookedCopyFileW(LPCWSTR lpExistingFileName, LPCWSTR lpNewFileName, BOOL bFailIfExists)
{
    std::wstring_view path_view(lpExistingFileName);
    if (path_view.length() > 9 && path_view.compare(path_view.length() - 9, 9, L"S0000.sl2") == 0) {
        return fpCopyFileW(g_save_path.c_str(), lpNewFileName, bFailIfExists);
    }

    return fpCopyFileW(lpExistingFileName, lpNewFileName, bFailIfExists);
}

void ApplyFilesMod()
{
    uintptr_t baseAddress = (uintptr_t)GetModuleHandle(NULL);
    fs::path curPath = fs::current_path();
    g_cur_len = curPath.wstring().length();

    WCHAR configPath[MAX_PATH];
    GetPrivateProfileStringW(L"files", L"dlls", L"", configPath, MAX_PATH, L".\\mod_engine.ini");
    if (lstrlenW(configPath) > 0) {
        ScanDllsDir(curPath / configPath);
    }

    GetPrivateProfileStringW(L"files", L"mods", L"", configPath, MAX_PATH, L".\\mod_engine.ini");
    if ((lstrlenW(configPath) > 0) && ScanModsDir(curPath / configPath)) {
        va_size.try_emplace(L"MO", 0x8400000);
        MH_CreateHook(reinterpret_cast<LPVOID>(baseAddress + OFFSET_HOOK_GET_SEKIRO_VA_SIZE), &HookedGetSekiroVASize, 
                        reinterpret_cast<LPVOID*>(&fpGetSekiroVASize));
        MH_CreateHook(reinterpret_cast<LPVOID>(baseAddress + OFFSET_HOOK_GET_SEKIRO_PATH), &HookedGetSekiroPath, 
                        reinterpret_cast<LPVOID*>(&fpGetSekiroPath));
        MH_CreateHookApi(L"kernel32", "CreateFileW", &HookedCreateFileW, 
                        reinterpret_cast<LPVOID*>(&fpCreateFileW));
    }

    GetPrivateProfileStringW(L"files", L"save", L"", configPath, MAX_PATH, L".\\mod_engine.ini");
    if ((lstrlenW(configPath) > 0) && fs::exists(curPath / configPath)) {
        g_save_path = (curPath / configPath).wstring();
        DisableSaveFileCheck();
        MH_CreateHookApi(L"kernel32", "CreateFileW", &HookedCreateFileW, 
                        reinterpret_cast<LPVOID*>(&fpCreateFileW));
        MH_CreateHookApi(L"kernel32", "CopyFileW", &HookedCopyFileW, 
                        reinterpret_cast<LPVOID*>(&fpCopyFileW));
    }

    GetPrivateProfileStringW(L"files", L"cutscene", L"", configPath, MAX_PATH, L".\\mod_engine.ini");
    if ((lstrlenW(configPath) > 0) && fs::exists(curPath / configPath)) {
        g_cutscene_path = (curPath / configPath).wstring();
        MH_CreateHook(reinterpret_cast<LPVOID>(baseAddress + OFFSET_HOOK_GET_SEKIRO_PATH), &HookedGetSekiroPath, 
                        reinterpret_cast<LPVOID*>(&fpGetSekiroPath));
        MH_CreateHookApi(L"kernel32", "CreateFileW", &HookedCreateFileW, 
                        reinterpret_cast<LPVOID*>(&fpCreateFileW));
    }

    WCHAR section[MAX_SECTION_SIZE];
    if (GetPrivateProfileSectionW(L"VirtualAlloc", section, MAX_SECTION_SIZE, L".\\mod_engine.ini")) {
        for (const WCHAR* pCurrent = section; *pCurrent; pCurrent += wcslen(pCurrent) + 1) {
            std::wstring_view line(pCurrent);
            size_t equalsPos = line.find(L'=');
            if (equalsPos != std::wstring_view::npos) {
                std::wstring key(line.substr(0, equalsPos));
                std::wstring value(line.substr(equalsPos + 1));

                size_t size = std::stoull(value, nullptr, 0);
                va_size.insert_or_assign(key, size);
            }
        }
    }
}
