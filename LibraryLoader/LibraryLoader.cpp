#include "pch.h"
#include "LibraryLoader.h"

namespace fs = std::filesystem;

void LoadDlls()
{
    fs::path currentDir = fs::current_path();
    fs::path dllsDir = currentDir / "dlls";
    if (!fs::exists(dllsDir) || !fs::is_directory(dllsDir)) {
        return;
    }

    std::vector<fs::directory_entry> entries;
    for (const auto& entry : fs::directory_iterator(dllsDir)) {
        if (!entry.is_directory()) {
            auto ext = entry.path().extension().wstring(); // e.g. L".dll"
            std::transform(ext.begin(), ext.end(), ext.begin(), std::towlower);
            if (ext == L".dll") {
                entries.push_back(entry);
            }
        }
    }

    if (entries.empty()) {
        return;
    }

    std::sort(entries.begin(), entries.end(), 
        [](const fs::directory_entry& a, const fs::directory_entry& b) {
            return a.path().filename() < b.path().filename();
        });

    for (const auto& entry : entries) {
        LoadLibraryW(entry.path().wstring().c_str());
    }
}
