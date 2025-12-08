#pragma once

#include <windows.h>

typedef struct {
    void *reserved1;
    WCHAR *path;
    void *reserved2;
    UINT64 length;
    UINT64 capacity;
} SekiroPath;

typedef void*(*t_GetSekiroPath)(
    SekiroPath*, void*, void*, void*, void*, void*
);

typedef HANDLE(WINAPI *t_CreateFileW)(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
);

void LoadMods();
