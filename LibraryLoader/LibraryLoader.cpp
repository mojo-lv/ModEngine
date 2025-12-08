#include "pch.h"
#include "LibraryLoader.h"

void LoadDlls()
{
    const DWORD size = 2048;
    WCHAR section[size];
    if (GetPrivateProfileSectionW(L"dlls", section, size, L".\\modengine.ini")) {
        WCHAR* pCurrent = section;
        WCHAR* pEnd = nullptr;
        WCHAR dllPath[MAX_PATH];
        
        while (*pCurrent != L'\0') {
            if (*pCurrent == L'"') {
                *pCurrent = L'\\';
                pEnd = wcschr(pCurrent, L'"');
                if (pEnd && pEnd > pCurrent + 1) {
                    *pEnd = L'\0';
                    GetCurrentDirectoryW(MAX_PATH, dllPath);
                    lstrcatW(dllPath, pCurrent);
                    LoadLibraryW(dllPath);
                    *pEnd = L'"';
                }
            }

            pCurrent += wcslen(pCurrent) + 1;
        }
    }
}
