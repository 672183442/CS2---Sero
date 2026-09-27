#pragma once
#include  "..\imgui_d11\imgui.h"
#include "theme.h"
#include "effects.h"

struct MenuData;

namespace UI {

    struct AppState {
        // 主题
        bool  dark = false;
        float themeT = 0.0f;

        // ★ 菜单显示/隐藏动画：0 = 完全隐藏（一个点），1 = 完全显示
        float menuScale = 1.0f;

        // 底部导航（7 页）
        int   page = 0;
        float navIndic = 0.0f;
        float navVel = 0.0f;
        int   titlePage = 0;
        float titleFade = 1.0f;
        float caesarScramble = 0.0f;
        float caesarTimer = 0.0f;

        // 背景特效开关
        bool  showBeams = true;
        bool  showDots = true;

        Fx::BeamRain     beams;
        Fx::ParticleBurst burst;

        // 业务数据指针（绑定 &g_MenuData）
        MenuData* menu = nullptr;
    };

    void Draw(AppState& st, const Theme::Palette& p, float dt, ImFont* bigFont);

} // namespace UI