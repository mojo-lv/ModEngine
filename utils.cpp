#include "pch.h"
#include "utils.h"

std::string WideCharToUTF8(const wchar_t* wstr)
{
    if (!wstr || *wstr == 0) return "";

    int wlen = static_cast<int>(wcslen(wstr));
    int size_needed = WideCharToMultiByte(
        CP_UTF8, 0, wstr, wlen,
        nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return "";

    std::string result(size_needed, '\0');
    int converted_size = WideCharToMultiByte(
        CP_UTF8, 0, wstr, wlen,
        result.data(), size_needed, nullptr, nullptr);
    if (converted_size <= 0) return "";

    return result;
}
