#pragma once
#include "..\imgui_d11\imgui.h"
#include <string>

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

    // ESP
    bool bMainESP = false;
    bool bBoxESP = false;
    bool bHealthESP = false;
    bool bArmorESP = false;
    bool bRayESP = false;
    bool bWeaponESP = false;
    bool bDistanceESP = false;
    bool bC4ESP = false;
    bool bGrenadeESP = false;
    bool bGrenadeCarried = true;
    bool bGrenadeThrown = true;
    bool bGrenadeDebug = false;
    bool bRemoveFriends = true;
    bool  bRadarRing = false;    // 雷达环
    float fRadarRadius = 120.0f;   // 环半径（像素）
   
    // Aim
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
    int aimHitbox = 0;   // 0=头 1=脖 2=胸 3=肚子
    // Other
    bool bAntiFlash = true;
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
    bool bBunnyHop = false;
    // Colors
    ImVec4 AccentColor;
    ImVec4 AimLockColor;
    ImVec4 BoxColorVisible;
    ImVec4 BoxColorInvisible;
    ImVec4 RayColorVisible;
    ImVec4 RayColorInvisible;
    float RayThickness = 1.5f;

    // Window
    float WindowAlpha = 0.93f;
    float WindowRound = 9.0f;
    float FontScale = 1.0f;
    float ItemSpacing = 8.0f;
    float FramePaddingX = 6.0f;
    float FramePaddingY = 5.0f;
    // Remote
    std::string versionInfo;
    std::string versionNumber;
    std::string announcement;

    PerformanceConfig performance;

    MenuData()
    {
        AccentColor = ImVec4(0.98f, 0.07f, 0.22f, 1.0f);
        AimLockColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);
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

// HUD 通知（右下角弹窗）
void AddHUDNotification(const char* text, const char* subtext = "", ImU32 color = IM_COL32(100, 200, 100, 255));
void RenderHUD();