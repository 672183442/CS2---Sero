#pragma once
#include "..\imgui_d11\imgui.h"
#include "utils/lang.hpp"
#include <string>

// 性能优化配置结构
struct PerformanceConfig {
    int updateInterval = 3;
    float maxDrawDistance = 8000.0f;
    float c4MaxDistance = 5000.0f;
    bool enableDistanceFilter = true;
    bool enableDynamicUpdate = true;
    bool enableLOD = true;
    float lodDistance1 = 3000.0f;
    float lodDistance2 = 6000.0f;
    bool enableWeaponCache = true;
};

struct MenuData
{
    int language = 0;
    bool bMainESP = false;
    bool bBoxESP = false;
    bool bHealthESP = false;
    bool bArmorESP = false;
    bool bRayESP = false;
    bool bWeaponESP = false;
    bool bDistanceESP = false;
    bool bC4ESP = false;
    bool bGrenadeESP = false;
    bool bGrenadeCarried = true;   // ★ 新增：绘制敌人携带的投掷物
    bool bGrenadeThrown = true;    // ★ 新增：绘制丢出的投掷物
    bool bGrenadeDebug = false;    // ★ 新增：调试模式（设置里开关）

    bool bAutoAim = false;
    float fAimSmooth = 1.0f;
    float fAimSpeed = 1.0f;
    bool bAimScope = false;
    float fAimFOV = 30.0f;
    bool bSilentAim = false;
    bool bShowAimLock = false;
    int aimKey = 1;
    bool bVisibleAim = false;
    bool bAimFriends = false;

    bool bAntiFlash = true;
    bool bRemoveFriends = true;

    ImVec4 AccentColor;
    ImVec4 AimLockColor;
    float WindowAlpha;
    float WindowRound;
    float FontScale;
    float ItemSpacing;
    float FramePaddingX;
    float FramePaddingY;
    float RayThickness;
    ImVec4 BoxColorVisible;
    ImVec4 BoxColorInvisible;
    ImVec4 RayColorVisible;
    ImVec4 RayColorInvisible;
    bool bFOVChanger = false;
    float fFOVValue = 90.0f;
    bool bThirdPerson = false;
    float fThirdPersonDist = 180.0f;
    int thirdPersonKey = 0;
    int thirdPersonStyle = 0;
    bool bThirdPersonNoInterp = false;
    bool bGlowESP = false;
    bool bGlowSeparateColors = false;
    ImVec4 GlowColor;
    ImVec4 GlowColorVisible;
    ImVec4 GlowColorInvisible;
    std::string versionInfo;
    std::string versionNumber;
    std::string announcement;

    PerformanceConfig performance;

    MenuData()
    {
        AccentColor = ImVec4(0.98f, 0.07f, 0.22f, 1.0f);
        AimLockColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        WindowAlpha = 0.93f;
        WindowRound = 9.0f;
        FontScale = 1.0f;
        ItemSpacing = 8.0f;
        FramePaddingX = 6.0f;
        FramePaddingY = 5.0f;
        RayThickness = 1.5f;
        BoxColorVisible = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        BoxColorInvisible = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        RayColorVisible = ImVec4(0.0f, 1.0f, 0.0f, 0.8f);
        RayColorInvisible = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
        GlowColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        GlowColorVisible = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
        GlowColorInvisible = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    }
};

extern MenuData g_MenuData;
void SetupModernImGuiStyle();
void RenderMainMenu();
void RenderHUD();