#pragma once

struct NpcAnimState {
    static constexpr uint32_t INVALID_ANIM = 0xFFFFFFFF;
    uint32_t lastAnim = INVALID_ANIM;
    bool inherit = false;
};

struct NpcAnimConfig {
    static constexpr uint8_t DELAY_MAX = 60;
    fs::path path;
    fs::file_time_type lastWriteTime;
    uint8_t reloadDelay = 0;
    bool reload = false;
};

void EnableNpcAnimChange();
