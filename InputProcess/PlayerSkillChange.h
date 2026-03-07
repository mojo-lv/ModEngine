#pragma once

#define COMBAT_ART_MIN 5000
#define COMBAT_ART_MAX 10000

#define PROSTHETIC_MIN 70000
#define PROSTHETIC_MAX 100000

#define EMPTY_SLOT 256

enum InputFlags : uint64_t {
    UseCombatArt = 0x100000001,
    UseProsthetic = 0x40040002,
    SwitchProsthetic = 0x400
};

struct PlayerSkillConfig {
    static constexpr uint8_t DELAY_MAX = 60;
    fs::path path;
    fs::file_time_type lastWriteTime;
    uint8_t reloadDelay = 0;
    bool reload = false;
};

void EnablePlayerSkillChange();
