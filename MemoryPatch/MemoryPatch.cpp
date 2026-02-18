#include "pch.h"
#include "MemoryPatch.h"

extern INIReader g_INI;

static void PatchMemory(uintptr_t targetAddress, const std::vector<uint8_t>& patchBytes)
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

    size_t offset, start, pos, valSize;
    uint8_t byte;
    for (const auto& key : g_INI.Keys("memory")) {
        std::string valStr = g_INI.GetString("memory", key, "");
        if (!valStr.empty()) {
            std::vector<uint8_t> bytes;
            valSize = valStr.size();
            start = 0;
            while (start < valSize) {
                pos = valStr.find(' ', start);
                if (pos == std::string::npos) {
                    pos = valSize;
                }
                if (pos > start) {
                    byte = static_cast<uint8_t>(std::stoul(valStr.substr(start, pos - start), nullptr, 16));
                    bytes.push_back(byte);
                }
                start = pos + 1;
            }

            offset = std::stoull(key, nullptr, 16);
            PatchMemory(baseAddress + offset, bytes);
        }
    }
}

void PatchSaveFileCheck()
{
    std::vector<uint8_t> bytes = {0x90, 0x90};
    PatchMemory(0x141B3C5AF, bytes);
    bytes = {0xEB};
    PatchMemory(0x140DFAB11, bytes);
    PatchMemory(0x140DFCC32, bytes);
}

void PatchDebugMenuHook(uintptr_t hookAddress)
{
    std::vector<uint8_t> callBytes = {0xE8};
    PatchMemory(hookAddress, callBytes);

    std::vector<uint8_t> retBytes = {0xC3};
    PatchMemory(0x142650400, retBytes);
    PatchMemory(0x14261E660, retBytes);
    PatchMemory(0x142619690, retBytes);

    std::vector<uint8_t> movAl01Bytes = {0xB0, 0x01};
    PatchMemory(0x1409791C0, movAl01Bytes);
    PatchMemory(0x1409791D0, movAl01Bytes);
    PatchMemory(0x1409791E0, movAl01Bytes);
    PatchMemory(0x141187590, movAl01Bytes);
};

void PatchDebugNPCHook(uintptr_t hookAddress)
{
    // call
    std::vector<uint8_t> bytes = {0xE8};
    PatchMemory(hookAddress, bytes);

    // jmp short 0x140614f3f
    bytes = {0xEB, 0x25};
    PatchMemory(hookAddress + 5, bytes);
}

void PatchNPCDamageHook(uintptr_t hookAddress)
{
    // movzx ecx, word [rcx+rbx*2]; call
    std::vector<uint8_t> bytes = {0x0F, 0xB7, 0x0C, 0x59, 0xE8};
    PatchMemory(hookAddress - 4, bytes);

    // cmp ah, 0x0; nop
    bytes = {0x80, 0xFC, 0x00, 0x90};
    PatchMemory(hookAddress + 5, bytes);
}

void PatchNpcAnimHook(uintptr_t hookAddress)
{
    /*  mov edx, 0xffffffff
        cmp edi, 0xb
        ja short 0x1407e385b
        jmp short 0x1407e384e  */
    std::vector<uint8_t> bytes = {0xba, 0xff, 0xff, 0xff, 0xff,
                                0x83, 0xff, 0x0b,
                                0x77, 0x17,
                                0xeb, 0x08};
    PatchMemory(hookAddress - 33, bytes);

    // mov edx, dword [rbx+rax*4+0x90]; call
    bytes = {0x8B, 0x94, 0x83, 0x90, 0x00, 0x00, 0x00, 0xE8};
    PatchMemory(hookAddress - 7, bytes);

    // jmp short 0x1407e3866
    bytes = {0xeb, 0x04};
    PatchMemory(hookAddress + 5, bytes);
}

void PatchNpcAnimCancelHook(uintptr_t hookAddress)
{
    // mov r8b, 0x17; jmp short 0x140b52058
    std::vector<uint8_t> bytes = {0x41, 0xb0, 0x17, 0xeb, 0x07};
    PatchMemory(0x140b5204c, bytes);

    /*  mov r8b, 0x4e
        mov rdx, rdi
        mov rcx, rsi
        call         */
    bytes = {0x41, 0xb0, 0x4e,
            0x48, 0x89, 0xfa,
            0x48, 0x89, 0xf1,
            0xe8};
    PatchMemory(hookAddress - 9, bytes);

    // mov rcx, rax; nop
    bytes = {0x48, 0x89, 0xc1, 0x90};
    PatchMemory(hookAddress + 5, bytes);
}
