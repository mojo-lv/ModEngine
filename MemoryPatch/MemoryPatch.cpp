#include "pch.h"
#include "MemoryPatch.h"

static std::vector<BYTE> HexSpaceStrToBytes(std::wstring_view str)
{
    std::vector<BYTE> bytes;
    size_t start = 0;
    while (start < str.length()) {
        size_t end = str.find(L' ', start);
        if (end == std::wstring_view::npos) {
            end = str.length();
        }

        if (end > start) {
            std::wstring segment(str.substr(start, end - start));
            bytes.push_back(static_cast<BYTE>(std::stoull(segment, nullptr, 16)));
        }

        start = end + 1;
    }
    return bytes;
}

static void PatchMemory(uintptr_t targetAddress, const std::vector<BYTE>& patchBytes)
{
    if (!patchBytes.empty()) {
        DWORD oldProtect;
        if (VirtualProtect((LPVOID)targetAddress, patchBytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy((LPVOID)targetAddress, patchBytes.data(), patchBytes.size());
            VirtualProtect((LPVOID)targetAddress, patchBytes.size(), oldProtect, &oldProtect); // Restore original protection
        }
    }
}

void ApplyMemoryPatch()
{
    uintptr_t baseAddress = (uintptr_t)GetModuleHandleW(nullptr);

    WCHAR section[MAX_SECTION_SIZE];
    if (GetPrivateProfileSectionW(L"memory", section, MAX_SECTION_SIZE, L".\\mod_engine.ini")) {
        for (const WCHAR* pCurrent = section; *pCurrent; pCurrent += wcslen(pCurrent) + 1) {
            std::wstring_view line(pCurrent);
            size_t equalsPos = line.find(L'=');
            if (equalsPos != std::wstring_view::npos) {
                std::wstring key(line.substr(0, equalsPos));
                std::wstring_view value_view = line.substr(equalsPos + 1);

                size_t offset = std::stoull(key, nullptr, 16);
                PatchMemory(baseAddress + offset, HexSpaceStrToBytes(value_view));
            }
        }
    }
}

void PatchSaveFileCheck()
{
    std::vector<BYTE> bytes = {0x90, 0x90};
    PatchMemory(0x141B3C5AF, bytes);
    bytes = {0xEB};
    PatchMemory(0x140DFAB11, bytes);
    PatchMemory(0x140DFCC32, bytes);
}

void PatchDebugMenu(uintptr_t hookAddress)
{
    std::vector<BYTE> callBytes = {0xE8};
    PatchMemory(hookAddress, callBytes);

    std::vector<BYTE> retBytes = {0xC3};
    PatchMemory(0x142650400, retBytes);
    PatchMemory(0x14261E660, retBytes);
    PatchMemory(0x142619690, retBytes);

    std::vector<BYTE> movAl01Bytes = {0xB0, 0x01};
    PatchMemory(0x1409791C0, movAl01Bytes);
    PatchMemory(0x1409791D0, movAl01Bytes);
    PatchMemory(0x1409791E0, movAl01Bytes);
    PatchMemory(0x141187590, movAl01Bytes);
};
