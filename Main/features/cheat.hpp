#pragma once
#include <vector>
#include <mutex>
#include <string>
#include "..\..\imgui_d11\imgui.h"
#include "../utils/math/vector.hpp"
#include "../core/memory/memory.hpp"
#include "../core/runtime_offsets.hpp"
#include "../ImGui_Menu.h"

extern uint32_t WIDTH, HEIGHT, WINDOW_W, WINDOW_H;

struct viewMatrix { float m[16]; };

struct EnemyInfo {
    uintptr_t pawn = 0;
    vec3 position{};
    vec3 headPosition{};
    int32_t health = 100;
    int32_t armor = 0;
    float distance = 0.0f;
    std::string weaponName;
    bool isSpotted = true;
    bool hasC4 = false;
    bool isFriend = false;
    std::vector<std::string> carriedGrenades;   // ★ 新增：该玩家携带的投掷物显示名列表
};

struct LocalPlayerCache {
    uintptr_t pawn = 0;
    vec3 position{};
    vec3 eyePosition{};
    vec2 viewAngle{};
    int32_t spottedBit = -1;
    bool isValid = false;
};

struct C4Info {
    bool exists = false;
    bool isPlanted = false;
    bool isDropped = false;
    bool isCarried = false;
    uintptr_t carrierPawn = 0;
    vec3 position{};
    bool isDefusing = false;
    int bombSite = 0;
};

struct GrenadeInfo {
    uintptr_t entity = 0;
    vec3 position{};
    vec3 lastPosition{};
    vec3 landingPosition{};
    int type = 0;
    bool isActive = true;
    bool landingRecorded = false;
    float disappearTimer = 0.0f;
    bool isThrown = false;
};

struct DroppedWeaponInfo {
    uintptr_t entity = 0;
    vec3 position{};
    std::string weaponName;
    uint16_t itemDef = 0;
    bool isC4 = false;
};

namespace Cheat
{
    extern std::vector<EnemyInfo> enemies;
    extern viewMatrix vm;
    extern LocalPlayerCache localPlayer;
    extern std::mutex dataMutex;
    extern bool hasAimTarget;
    extern vec3 aimTargetPosition;
    extern C4Info c4Info;
    extern std::vector<GrenadeInfo> grenades;
    extern std::vector<DroppedWeaponInfo> droppedWeapons;

    bool Init(uintptr_t clientBase);
    void UpdateEntities();
    void UpdateGrenades();
    void UpdateGroundItems();
    void RunAimbot();
    void DrawESP(ImDrawList* drawList);

    bool InitSilentAimHook();
    void CleanupSilentAimHook();
    void UpdateSilentAimCache();
    void RunThirdPerson();
    bool InitThirdPersonHook();
    void CleanupThirdPersonHook();

    bool WorldToScreen(const vec3& world, vec2& screen, const float m[16]);
    double PlayerDistance(const vec3& a, const vec3& b);
    float NormalizeAngle(float angle);
    vec2 CalcAngle(const vec3& src, const vec3& dst);
    float GetFOV(const vec2& viewAngle, const vec2& aimAngle);
}