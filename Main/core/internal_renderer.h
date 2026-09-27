#pragma once
#include "../../imgui_d11/imgui.h"
#include <cstdint>

namespace renderer
{
    inline ImDrawList* drawList = nullptr;

    inline void Line(float x1, float y1, float x2, float y2, ImU32 color, float thickness = 1.0f)
    {
        if (drawList) drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color, thickness);
    }

    inline void Box(float x, float y, float w, float h, ImU32 color, float thickness = 1.0f)
    {
        if (drawList) drawList->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), color, 0.0f, 0, thickness);
    }

    inline void Text(float x, float y, const char* text, ImU32 color)
    {
        if (drawList) drawList->AddText(ImVec2(x, y), color, text);
    }
}