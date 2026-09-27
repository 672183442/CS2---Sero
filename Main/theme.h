#pragma once
#include  "..\imgui_d11\imgui.h"

namespace Theme {

    struct Palette {
        ImU32 bg;
        ImU32 card;
        ImU32 cardBorder;
        ImU32 dotGrid;
        ImU32 text;
        ImU32 textDim;
        ImU32 accent;
        ImU32 accentHover;
        ImU32 accentSoft;
        ImU32 track;
        ImU32 danger;
        ImU32 dangerSoft;
        ImU32 success;
        ImU32 navBar;
        ImU32 shadow;
    };

    Palette Lerp(float t);
    void ApplyStyle(const Palette& p);
    ImVec4 U32ToVec4(ImU32 c);

    // ★ 新增：把 Palette 所有颜色的 alpha 乘上 alpha（用于菜单淡入淡出）
    Palette ScaleAlpha(const Palette& p, float alpha);

} // namespace Theme