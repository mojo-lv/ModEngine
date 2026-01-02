// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here
#include <windows.h>
#include <string>
#include <vector>
#include <iostream>
#include <filesystem>  // C++17
#include <algorithm>
#include <cwctype>

#define MAX_SECTION_SIZE 2048
#define ENABLE_LOGGING 0

namespace fs = std::filesystem;

#endif //PCH_H
