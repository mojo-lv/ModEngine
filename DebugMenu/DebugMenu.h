#pragma once

struct FontConfig {
    std::string path;
    float spacing = 0.0f;
    float size = 20.0f;
    UINT color = 0xFFFFFFFF;
};

struct MenuEntry {
    std::string text;
    float fX;
    float fY;
};

struct DbgCamState {
    uint8_t* playerMaskPtr = nullptr;
    uintptr_t npc = 0;
    uint32_t lastCamMode = 0;
    uint8_t lastMask = 0;
};

extern std::vector<MenuEntry> g_menuList;
extern int g_menuSelectedIndex;
extern FontConfig g_fontConfig;
extern bool g_log_debug_menu;

void EnableDebugMenu(const INIReader& ini, const fs::path& curPath);
