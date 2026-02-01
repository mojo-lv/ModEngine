#include "pch.h"
#include "utils.h"

std::wstring GetModuleNameFromAddress(void* address) {
    HMODULE hModule = NULL;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)address,
        &hModule))
    {
        wchar_t path[MAX_PATH];
        if (GetModuleFileNameW(hModule, path, MAX_PATH)) {
            return std::wstring(path);
        }
    }
    return L"Unknown Module";
}

void ReadAndPrintBytes(uintptr_t address, size_t size) {
    unsigned char* p = (unsigned char*)address;

    std::cout << "Address: 0x" << std::hex << address << " | Bytes: ";
    for (size_t i = 0; i < size; ++i) {
        printf("%02X ", p[i]);
    }
    std::cout << std::endl;
}

void PrintStackTrace() {
    void* stack[8] = {};
    USHORT frames = RtlCaptureStackBackTrace(0, 8, stack, NULL);
    printf("Stack:\n");
    for (USHORT i = 0; i < frames; ++i) {
        printf("\tFrame[%d]: %p, %ls\n", i, stack[i], GetModuleNameFromAddress(stack[i]).c_str());
        ReadAndPrintBytes((uintptr_t)stack[i], 10);
    }
}
