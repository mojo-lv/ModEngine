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

struct DbgCamState {
    uint8_t* playerMaskPtr = nullptr;
    uintptr_t npc = 0;
    uintptr_t lastNpc = 0;
    uint32_t lastCamMode = 0;
    uint8_t lastMask = 0;
};

extern std::vector<MenuEntry> g_menuList;
extern int g_menuSelectedIndex;
extern FontConfig g_fontConfig;
extern bool g_log_debug_menu;
void EnableDebugMenu();
