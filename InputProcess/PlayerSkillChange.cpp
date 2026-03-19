#include "pch.h"
#include "PlayerSkillChange.h"

typedef uint64_t(*tSetSkillSlot)(uint32_t slot, uint32_t* skillEquipData, bool forceSet);
static tSetSkillSlot SetSkillSlot = reinterpret_cast<tSetSkillSlot>(0x140D592F0);

typedef uint32_t(*tGetEquipRealId)(uintptr_t equipBaseAddr, uint32_t* equipId);
static tGetEquipRealId GetEquipRealId = reinterpret_cast<tGetEquipRealId>(0x140C3D680);

typedef size_t(*t_sub_140B2C190)(uintptr_t, void*);
static t_sub_140B2C190 fp_sub_140B2C190 = nullptr;

typedef void(*t_sub_140dd9c60)(uint32_t* arg1);
static t_sub_140dd9c60 fp_sub_140dd9c60 = nullptr;

static std::vector<std::vector<std::pair<uint32_t, uint32_t>>> CombatArtData(3);
static std::vector<std::vector<std::pair<uint32_t, uint32_t>>> ProstheticData(3);

static uint32_t skillEquipData[17];

static PlayerSkillConfig skillConfig;
static bool logEquipSkill = false;

extern INIReader g_INI;

static void LoadSkillConfig()
{
    uint32_t index, vKey, skillId;
    size_t valSize, start, pos;
    INIReader config(skillConfig.path.string());
    for (const auto& configKey : config.Keys("")) {
        index = static_cast<unsigned char>(configKey[0]) - '1';
        vKey = std::toupper(static_cast<unsigned char>(configKey[2]));
        if (vKey < 'A') {
            vKey = std::stoul(configKey.substr(2));
        }

        std::string valStr = config.GetString("", configKey, "");
        if (!valStr.empty()) {
            valSize = valStr.size();
            start = 0;
            while (start < valSize) {
                pos = valStr.find('_', start);
                if (pos == std::string::npos) {
                    pos = valSize;
                }
                if (pos > start) {
                    skillId = std::stoul(valStr.substr(start, pos - start));
                    if (skillId >= COMBAT_ART_MIN && skillId < COMBAT_ART_MAX) {
                        if (index < CombatArtData.size()) {
                            CombatArtData[index].emplace_back(vKey, skillId);
                        }
                    } else if (skillId >= PROSTHETIC_MIN && skillId < PROSTHETIC_MAX) {
                        if (index < ProstheticData.size()) {
                            ProstheticData[index].emplace_back(vKey, skillId);
                        }
                    }
                }
                start = pos + 1;
            }
        }
    }
}

size_t hook_sub_140B2C190(uintptr_t p1, void* p2)
{
    static uintptr_t equipBaseAddr = 0;
    static uint32_t* skillBasePtr = nullptr;

    if (!skillBasePtr) {
        uintptr_t playerBaseAddr = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(0x143d5aac0) + 0x8);
        equipBaseAddr = *reinterpret_cast<uintptr_t*>(playerBaseAddr + 0x5B0) + 0x10;
        skillBasePtr = reinterpret_cast<uint32_t*>(playerBaseAddr + 0x278 + 0x24);

        for (int i : {0, 1, 2, 4}) {
            skillEquipData[14] = skillBasePtr[i];
            if (skillEquipData[14] != EMPTY_SLOT) {
                skillBasePtr[i] = EMPTY_SLOT;
                SetSkillSlot(i, skillEquipData, true);
                skillEquipData[i] = skillEquipData[14];
            }
        }
        return fp_sub_140B2C190(p1, p2);
    }

    uint64_t* prev_keys_raw = reinterpret_cast<uint64_t*>(p1 + 0x18);
    uint64_t* current_keys_raw = reinterpret_cast<uint64_t*>(p1 + 0x10);
    uint64_t just_pressed = *current_keys_raw & (~*prev_keys_raw);

    if (just_pressed == 0) {
        if (skillConfig.reload && (skillConfig.reloadDelay++ > skillConfig.DELAY_MAX)) {
            skillConfig.reloadDelay = 0;
            auto currentWriteTime = fs::last_write_time(skillConfig.path);
            if (skillConfig.lastWriteTime != currentWriteTime) {
                skillConfig.lastWriteTime = currentWriteTime;
                for (auto& v : CombatArtData) v.clear();
                for (auto& v : ProstheticData) v.clear();
                LoadSkillConfig();
            }
        }
        return fp_sub_140B2C190(p1, p2);
    }

    if (InputFlags::SwitchProsthetic == (just_pressed & InputFlags::SwitchProsthetic)) {
        if (skillBasePtr[32] < ProstheticData.size()) {
            uint32_t slot = skillBasePtr[32] << 1;
            skillEquipData[14] = skillBasePtr[slot];
            if (skillEquipData[14] != skillEquipData[slot]) {
                skillBasePtr[slot] = EMPTY_SLOT;
                SetSkillSlot(slot, skillEquipData, true);
                skillEquipData[slot] = skillEquipData[14];
            }
        }
        return fp_sub_140B2C190(p1, p2);
    }

    if (InputFlags::UseProsthetic == (just_pressed & InputFlags::UseProsthetic)) {
        if (skillBasePtr[32] < ProstheticData.size()) {
            for (const auto& [vKey, skillId] : ProstheticData[skillBasePtr[32]]) {
                if ((GetAsyncKeyState(vKey) & 0x8000) != 0) {
                    skillEquipData[16] = skillId;
                    skillEquipData[14] = GetEquipRealId(equipBaseAddr, skillEquipData + 16);

                    uint32_t slot = skillBasePtr[32] << 1;
                    if (skillEquipData[14] != skillEquipData[slot]) {
                        for (int i : {0, 2, 4}) {
                            skillEquipData[i + 5] = skillBasePtr[i];
                            skillBasePtr[i] = EMPTY_SLOT;
                        }
                        SetSkillSlot(slot, skillEquipData, true);
                        for (int i : {0, 2, 4}) {
                            skillBasePtr[i] = skillEquipData[i + 5];
                        }
                        skillEquipData[slot] = skillEquipData[14];
                        *current_keys_raw &= ~InputFlags::UseProsthetic;
                    }
                    return fp_sub_140B2C190(p1, p2);
                }
            }

            uint32_t slot = skillBasePtr[32] << 1;
            skillEquipData[14] = skillBasePtr[slot];
            if (skillEquipData[14] != skillEquipData[slot]) {
                skillBasePtr[slot] = EMPTY_SLOT;
                SetSkillSlot(slot, skillEquipData, true);
                skillEquipData[slot] = skillEquipData[14];
                *current_keys_raw &= ~InputFlags::UseProsthetic;
            }
        }
        return fp_sub_140B2C190(p1, p2);
    }

    if (InputFlags::UseCombatArt == (just_pressed & InputFlags::UseCombatArt)) {
        if (skillBasePtr[32] < CombatArtData.size()) {
            for (const auto& [vKey, skillId] : CombatArtData[skillBasePtr[32]]) {
                if ((GetAsyncKeyState(vKey) & 0x8000) != 0) {
                    skillEquipData[16] = skillId;
                    skillEquipData[14] = GetEquipRealId(equipBaseAddr, skillEquipData + 16);
                    if (skillEquipData[14] != skillEquipData[1]) {
                        skillEquipData[6] = skillBasePtr[1];
                        skillBasePtr[1] = EMPTY_SLOT;
                        SetSkillSlot(1, skillEquipData, true);
                        skillBasePtr[1] = skillEquipData[6];
                        skillEquipData[1] = skillEquipData[14];
                        *current_keys_raw &= ~InputFlags::UseCombatArt;
                    }
                    return fp_sub_140B2C190(p1, p2);
                }
            }

            skillEquipData[14] = skillBasePtr[1];
            if (skillEquipData[14] != skillEquipData[1]) {
                skillBasePtr[1] = EMPTY_SLOT;
                SetSkillSlot(1, skillEquipData, true);
                skillEquipData[1] = skillEquipData[14];
                *current_keys_raw &= ~InputFlags::UseCombatArt;
            }
        }
        return fp_sub_140B2C190(p1, p2);
    }

    return fp_sub_140B2C190(p1, p2);
}

void hook_sub_140dd9c60(uint32_t* arg1)
{
    switch(arg1[2]) {
        case 0:
        case 1:
        case 2:
        case 4:
            skillEquipData[arg1[2]] = arg1[18];
            if (logEquipSkill) {
                std::cout << "[equip_skill] " << arg1[20] << std::endl;
            }
            break;
    }

    return fp_sub_140dd9c60(arg1);
}

void EnablePlayerSkillChange()
{
    std::string configPath = g_INI.GetString("player_skill_change", "config_path", "");
    bool reload = g_INI.GetBoolean("player_skill_change", "config_runtime_reload", false);
    logEquipSkill = g_INI.GetBoolean("logs", "equip_skill", false);

    fs::path curPath = fs::current_path();
    if (!configPath.empty() && fs::exists(curPath / configPath)) {
        skillConfig.path = curPath / configPath;
        skillConfig.lastWriteTime = fs::last_write_time(skillConfig.path);
        skillConfig.reload = reload;
        LoadSkillConfig();

        MH_CreateHook(reinterpret_cast<LPVOID>(0x140B2C190), &hook_sub_140B2C190, 
                        reinterpret_cast<LPVOID*>(&fp_sub_140B2C190));

        MH_CreateHook(reinterpret_cast<LPVOID>(0x140dd9c60), &hook_sub_140dd9c60, 
                        reinterpret_cast<LPVOID*>(&fp_sub_140dd9c60));
    } else if (logEquipSkill) {
        MH_CreateHook(reinterpret_cast<LPVOID>(0x140dd9c60), &hook_sub_140dd9c60, 
                        reinterpret_cast<LPVOID*>(&fp_sub_140dd9c60));
    }
}
