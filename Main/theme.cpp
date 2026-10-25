#include "theme.h"

namespace Theme {

    ImVec4 U32ToVec4(ImU32 c) {
        return ImGui::ColorConvertU32ToFloat4(c);
    }

    static ImU32 LerpU32(ImU32 a, ImU32 b, float t) {
        ImVec4 ca = ImGui::ColorConvertU32ToFloat4(a);
        ImVec4 cb = ImGui::ColorConvertU32ToFloat4(b);
        ImVec4 r(ca.x + (cb.x - ca.x) * t,
            ca.y + (cb.y - ca.y) * t,
            ca.z + (cb.z - ca.z) * t,
            ca.w + (cb.w - ca.w) * t);
        return ImGui::ColorConvertFloat4ToU32(r);
    }

    static Palette Light() {
        Palette p;
        p.bg = IM_COL32(0xEA, 0xE8, 0xE1, 0xFF);
        p.card = IM_COL32(0xFB, 0xFA, 0xF7, 0xFF);
        p.cardBorder = IM_COL32(0x00, 0x00, 0x00, 0x12);
        p.dotGrid = IM_COL32(0x1A, 0x1A, 0x2E, 0x14);
        p.text = IM_COL32(0x1B, 0x1B, 0x22, 0xFF);
        p.textDim = IM_COL32(0x1B, 0x1B, 0x22, 0x80);
        p.accent = IM_COL32(0x6C, 0x63, 0xFF, 0xFF);
        p.accentHover = IM_COL32(0x58, 0x50, 0xE6, 0xFF);
        p.accentSoft = IM_COL32(0x6C, 0x63, 0xFF, 0x22);
        p.track = IM_COL32(0x6C, 0x63, 0xFF, 0x33);
        p.danger = IM_COL32(0xE5, 0x3E, 0x3E, 0xFF);
        p.dangerSoft = IM_COL32(0xE5, 0x3E, 0x3E, 0x18);
        p.success = IM_COL32(0x2E, 0xBD, 0x6B, 0xFF);
        p.navBar = IM_COL32(0xFF, 0xFF, 0xFF, 0x9E);
        p.shadow = IM_COL32(0x1A, 0x1A, 0x2E, 0x22);
        return p;
    }

    static Palette Dark() {
        Palette p;
        p.bg = IM_COL32(0x0A, 0x0A, 0x0C, 0xFF);
        p.card = IM_COL32(0x15, 0x15, 0x1B, 0xFF);
        p.cardBorder = IM_COL32(0xFF, 0xFF, 0xFF, 0x14);
        p.dotGrid = IM_COL32(0xFF, 0xFF, 0xFF, 0x12);
        p.text = IM_COL32(0xF4, 0xF4, 0xF6, 0xFF);
        p.textDim = IM_COL32(0xF4, 0xF4, 0xF6, 0x72);
        p.accent = IM_COL32(0x7C, 0x74, 0xFF, 0xFF);
        p.accentHover = IM_COL32(0x8F, 0x88, 0xFF, 0xFF);
        p.accentSoft = IM_COL32(0x7C, 0x74, 0xFF, 0x2E);
        p.track = IM_COL32(0x7C, 0x74, 0xFF, 0x33);
        p.danger = IM_COL32(0xFF, 0x5A, 0x5A, 0xFF);
        p.dangerSoft = IM_COL32(0xFF, 0x5A, 0x5A, 0x1E);
        p.success = IM_COL32(0x3A, 0xD6, 0x7E, 0xFF);
        p.navBar = IM_COL32(0x2A, 0x2A, 0x38, 0xB4);
        p.shadow = IM_COL32(0x00, 0x00, 0x00, 0x66);
        return p;
    }

    Palette Lerp(float t) {
        Palette a = Light(), b = Dark(), r;
        r.bg = LerpU32(a.bg, b.bg, t);
        r.card = LerpU32(a.card, b.card, t);
        r.cardBorder = LerpU32(a.cardBorder, b.cardBorder, t);
        r.dotGrid = LerpU32(a.dotGrid, b.dotGrid, t);
        r.text = LerpU32(a.text, b.text, t);
        r.textDim = LerpU32(a.textDim, b.textDim, t);
        r.accent = LerpU32(a.accent, b.accent, t);
        r.accentHover = LerpU32(a.accentHover, b.accentHover, t);
        r.accentSoft = LerpU32(a.accentSoft, b.accentSoft, t);
        r.track = LerpU32(a.track, b.track, t);
        r.danger = LerpU32(a.danger, b.danger, t);
        r.dangerSoft = LerpU32(a.dangerSoft, b.dangerSoft, t);
        r.success = LerpU32(a.success, b.success, t);
        r.navBar = LerpU32(a.navBar, b.navBar, t);
        r.shadow = LerpU32(a.shadow, b.shadow, t);
        return r;
    }


    void ApplyStyle(const Palette& p) {
        ImGuiStyle& s = ImGui::GetStyle();
        s.WindowRounding = 20.0f;
        s.ChildRounding = 16.0f;
        s.FrameRounding = 12.0f;
        s.PopupRounding = 12.0f;
        s.GrabRounding = 12.0f;
        s.TabRounding = 10.0f;
        s.ScrollbarRounding = 12.0f;
        s.FramePadding = ImVec2(14, 10);
        s.ItemSpacing = ImVec2(12, 12);
        s.WindowPadding = ImVec2(24, 20);
        s.GrabMinSize = 18.0f;
        s.ScrollbarSize = 10.0f;
        s.WindowBorderSize = 0.0f;
        s.FrameBorderSize = 0.0f;
        s.PopupBorderSize = 0.0f;

        ImVec4* c = s.Colors;
        c[ImGuiCol_WindowBg] = U32ToVec4(p.card);
        c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_PopupBg] = U32ToVec4(p.card);
        c[ImGuiCol_Border] = U32ToVec4(p.cardBorder);
        c[ImGuiCol_Text] = U32ToVec4(p.text);
        c[ImGuiCol_TextDisabled] = U32ToVec4(p.textDim);
        c[ImGuiCol_FrameBg] = U32ToVec4(p.track);
        c[ImGuiCol_FrameBgHovered] = U32ToVec4(p.accentSoft);
        c[ImGuiCol_FrameBgActive] = U32ToVec4(p.accentSoft);
        c[ImGuiCol_CheckMark] = ImVec4(1, 1, 1, 1);
        c[ImGuiCol_SliderGrab] = U32ToVec4(p.accent);
        c[ImGuiCol_SliderGrabActive] = U32ToVec4(p.accentHover);
        c[ImGuiCol_Button] = U32ToVec4(p.accent);
        c[ImGuiCol_ButtonHovered] = U32ToVec4(p.accentHover);
        c[ImGuiCol_ButtonActive] = U32ToVec4(p.accentHover);
        c[ImGuiCol_Header] = U32ToVec4(p.accentSoft);
        c[ImGuiCol_HeaderHovered] = U32ToVec4(p.accentSoft);
        c[ImGuiCol_HeaderActive] = U32ToVec4(p.accentSoft);
        c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_ScrollbarGrab] = U32ToVec4(p.track);
        c[ImGuiCol_ScrollbarGrabHovered] = U32ToVec4(p.accentSoft);
        c[ImGuiCol_Separator] = U32ToVec4(p.cardBorder);
        c[ImGuiCol_Tab] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_TabHovered] = U32ToVec4(p.accentSoft);
        c[ImGuiCol_TabActive] = U32ToVec4(p.accentSoft);
    }

} // namespace Theme