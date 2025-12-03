#include "pch.h"
#include "MemoryPatcher.h"

static std::vector<BYTE> HexSpaceStrToBytes(const std::wstring& str) {
    std::vector<BYTE> bytes;

    std::wstring splitStr;
    BYTE byte;
    size_t start = 0, end;
    while ((end = str.find(L' ', start)) != std::wstring::npos) {
        if (end > start) {
            splitStr = str.substr(start, end - start);
            byte = static_cast<BYTE>(std::stoull(splitStr, nullptr, 16));
            bytes.push_back(byte);
        }
        start = end + 1;
    }
    if (start < str.length()) {
        byte = static_cast<BYTE>(std::stoull(str.substr(start), nullptr, 16));
        bytes.push_back(byte);
    }

    return bytes;
}

static void applyMemoryPatch(size_t targetOffset, std::vector<BYTE> patchBytes)
{
	DWORD_PTR targetAddress = (DWORD_PTR)GetModuleHandle(NULL) + targetOffset;

	DWORD oldProtect;
	if (VirtualProtect((LPVOID)targetAddress, patchBytes.size(), PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		memcpy((LPVOID)targetAddress, patchBytes.data(), patchBytes.size());
		DWORD tempProtect;
		VirtualProtect((LPVOID)targetAddress, patchBytes.size(), oldProtect, &tempProtect);
	}
}

void patchMemory()
{
    const DWORD size = 8192;
    WCHAR section[size];
    if (GetPrivateProfileSectionW(L"memory", section, size, L".\\modengine.ini")) {
        WCHAR* pCurrent = section;
        while (*pCurrent != L'\0') {
            if (*pCurrent == L'+') {
                pCurrent++;
                WCHAR* pEquals = wcschr(pCurrent, L'=');
                if (pEquals && pEquals > pCurrent) {
                    std::wstring key(pCurrent, pEquals - pCurrent);
                    std::wstring value(pEquals + 1);

                    size_t offset = std::stoull(key, nullptr, 16);
                    applyMemoryPatch(offset, HexSpaceStrToBytes(value));
                }
            }

            pCurrent += wcslen(pCurrent) + 1;
        }
    }
}
