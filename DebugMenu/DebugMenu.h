#pragma once

struct FontConfig {
    std::string path;
    float size = 18.0f;
    UINT color = 0xFFFFFFFF;
};

struct MenuEntry {
    float fX;
    float fY;
    std::string text;
};

extern std::vector<MenuEntry> g_menuList;
extern FontConfig g_fontConfig;
void EnableDebugMenu();
