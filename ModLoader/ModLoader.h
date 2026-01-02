#pragma once

typedef struct {
    void *reserved1;
    WCHAR *path;
    void *reserved2;
    UINT64 length;
    UINT64 capacity;
} SekiroPath;

typedef SekiroPath*(*t_GetSekiroPath)(
    SekiroPath*, void*, void*, void*, void*, void*
);

typedef size_t(*t_GetSekiroVASize)(LPCWSTR, size_t);

typedef HANDLE(WINAPI *t_CreateFileW)(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
);

typedef BOOL(WINAPI *t_CopyFileW)(
    LPCWSTR lpExistingFileName,
    LPCWSTR lpNewFileName,
    BOOL    bFailIfExists
);

extern std::vector<HMODULE> g_LoadedDLLs;

void LoadModFiles();
