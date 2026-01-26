#pragma once

std::wstring GetModuleNameFromAddress(void* address);
void ReadAndPrintBytes(uintptr_t address, size_t size);
void PrintStackTrace();
