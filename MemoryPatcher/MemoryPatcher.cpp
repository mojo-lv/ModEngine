#include "pch.h"
#include "MemoryPatcher.h"

static std::vector<BYTE> HexSpaceStrToBytes(std::wstring_view str) {
    std::vector<BYTE> bytes;
    size_t start = 0;
    while (start < str.length()) {
        size_t end = str.find(L' ', start);
        if (end == std::wstring_view::npos) {
            end = str.length();
        }

        if (end > start) {
            std::wstring_view segment = str.substr(start, end - start);
            bytes.push_back(static_cast<BYTE>(std::stoull(std::wstring(segment), nullptr, 16)));
        }

        start = end + 1;
    }
    return bytes;
}

static void ApplyMemoryPatch(size_t targetOffset, const std::vector<BYTE>& patchBytes)
{
    if (patchBytes.empty()) {
        return;
    }

    DWORD_PTR targetAddress = (DWORD_PTR)GetModuleHandle(NULL) + targetOffset;

    DWORD oldProtect;
    if (VirtualProtect((LPVOID)targetAddress, patchBytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        memcpy((LPVOID)targetAddress, patchBytes.data(), patchBytes.size());
        DWORD tempProtect;
		VirtualProtect((LPVOID)targetAddress, patchBytes.size(), oldProtect, &tempProtect); // Restore original protection
    }
}

void PatchMemory()
{
    const DWORD size = 2048;
    WCHAR section[size];
    if (GetPrivateProfileSectionW(L"memory", section, size, L".\\modengine.ini")) {
        WCHAR* pCurrent = section;
        WCHAR* pEquals = nullptr;
        
        while (*pCurrent != L'\0') {
            if (*pCurrent == L'+') {
                pCurrent++;
                pEquals = wcschr(pCurrent, L'=');
                if (pEquals && pEquals > pCurrent) {
                    std::wstring key(pCurrent, pEquals - pCurrent);
                    std::wstring_view value(pEquals + 1);

                    size_t offset = std::stoull(key, nullptr, 16);
                    ApplyMemoryPatch(offset, HexSpaceStrToBytes(value));
                }
            }

            pCurrent += wcslen(pCurrent) + 1;
        }
    }
}
