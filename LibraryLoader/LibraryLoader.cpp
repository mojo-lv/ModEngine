#include "pch.h"
#include "LibraryLoader.h"

void LoadDlls()
{
    const DWORD size = 2048;
    WCHAR section[size];
    if (GetPrivateProfileSectionW(L"dlls", section, size, L".\\modengine.ini")) {
        WCHAR* pCurrent = section;
        while (*pCurrent != L'\0') {
            if (*pCurrent == L'"') {
                *pCurrent = L'\\';
                WCHAR* pEnd = wcschr(pCurrent, L'"');
                if (pEnd) {
                    *pEnd = L'\0';
                    WCHAR dllPath[MAX_PATH];
                    GetCurrentDirectoryW(MAX_PATH, dllPath);
                    lstrcatW(dllPath, pCurrent);
                    LoadLibraryW(dllPath);
                }
            }

            pCurrent += wcslen(pCurrent) + 1;
        }
    }
}
