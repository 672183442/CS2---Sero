#include "ui.h"
#include "..\imgui_d11\imgui_internal.h"
#include "menu.h"
#include "utils/lang.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>



namespace UI {

    // ---- 工具 ----
    static void PanelWithShadow(ImDrawList* dl, ImVec2 min, ImVec2 max,
        ImU32 fill, ImU32 border, ImU32 shadow, float rounding = 16.0f) {
        for (int i = 6; i >= 1; i--) {
            float o = (float)i * 2.0f;
            ImVec4 s = ImGui::ColorConvertU32ToFloat4(shadow);
            s.w *= (0.16f * (7 - i) / 6.0f);
            dl->AddRectFilled(ImVec2(min.x - o, min.y - o + 4), ImVec2(max.x + o, max.y + o + 6),
                ImGui::ColorConvertFloat4ToU32(s), rounding + o);
        }
        dl->AddRectFilled(min, max, fill, rounding);
        dl->AddRect(min, max, border, rounding, 0, 1.5f);
    }

    static void GlassPill(ImDrawList* dl, ImVec2 min, ImVec2 max,
        const Theme::Palette& p, float rounding) {
        for (int i = 7; i >= 1; i--) {
            float o = (float)i * 1.8f;
            ImVec4 s = ImGui::ColorConvertU32ToFloat4(p.shadow);
            s.w *= 0.10f * (8 - i) / 7.0f;
            dl->AddRectFilled(ImVec2(min.x - o, min.y - o + 3), ImVec2(max.x + o, max.y + o + 5),
                ImGui::ColorConvertFloat4ToU32(s), rounding + o);
        }
        
        dl->AddRectFilled(min, max, p.navBar, rounding);
        ImVec4 cardC = ImGui::ColorConvertU32ToFloat4(p.card);
        float lum = cardC.x * 0.299f + cardC.y * 0.587f + cardC.z * 0.114f;
        float hlA = 0.10f + 0.25f * lum;
        ImVec4 hl(1, 1, 1, hlA);
        dl->AddLine(ImVec2(min.x + rounding, min.y + 1.0f), ImVec2(max.x - rounding, min.y + 1.0f),
            ImGui::ColorConvertFloat4ToU32(hl), 1.0f);
        dl->AddRect(min, max, p.cardBorder, rounding, 0, 1.0f);
    }

    static float Spring(float cur, float target, float& vel, float dt, float stiff = 220.0f, float damp = 22.0f) {
        float force = (target - cur) * stiff - vel * damp;
        vel += force * dt;
        return cur + vel * dt;
    }

    static float Approach(float cur, float target, float dt, float speed = 10.0f) {
        float t = 1.0f - expf(-speed * dt);
        return cur + (target - cur) * t;
    }

    static bool ToggleSwitch(const char* id, bool* v, const Theme::Palette& p) {
        ImGui::PushID(id);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float h = 26.0f, w = 46.0f, r = h * 0.5f;
        ImGui::InvisibleButton("t", ImVec2(w, h));
        bool changed = false;
        if (ImGui::IsItemClicked()) { *v = !*v; changed = true; }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImGuiStorage* st = ImGui::GetStateStorage();
        ImGuiID kk = ImGui::GetID("k");
        float k = st->GetFloat(kk, *v ? 1.0f : 0.0f);
        k = Approach(k, *v ? 1.0f : 0.0f, ImGui::GetIO().DeltaTime, 16.0f);
        st->SetFloat(kk, k);
        ImU32 track = ImGui::ColorConvertFloat4ToU32(ImLerp(
            ImGui::ColorConvertU32ToFloat4(p.track),
            ImGui::ColorConvertU32ToFloat4(p.accent), k));
        dl->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + h), track, r);
        float cx = pos.x + r + (w - h) * k;
        dl->AddCircleFilled(ImVec2(cx, pos.y + r), r - 3.0f, IM_COL32_WHITE);
        ImGui::PopID();
        return changed;
    }

    static bool CheckChip(const char* id, bool* v, const Theme::Palette& p) {
        ImGui::PushID(id);
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float w = 40.0f, h = 26.0f, r = 9.0f;
        ImGui::InvisibleButton("c", ImVec2(w, h));
        bool changed = false;
        if (ImGui::IsItemClicked()) { *v = !*v; changed = true; }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float dt = ImGui::GetIO().DeltaTime;

        ImGuiStorage* stg = ImGui::GetStateStorage();
        ImGuiID phID = ImGui::GetID("ph");
        ImGuiID dispID = ImGui::GetID("disp");
        float target = *v ? 1.0f : 0.0f;
        float ph = stg->GetFloat(phID, -1.0f);
        float disp = stg->GetFloat(dispID, target);

        if (changed) ph = 0.0f;
        else if (ph < 0.0f && disp != target) ph = 0.0f;
        if (ph >= 0.0f) {
            ph += dt / 0.20f;
            if (ph >= 0.5f) disp = target;
            if (ph >= 1.0f) { ph = -1.0f; disp = target; }
        }
        stg->SetFloat(phID, ph);
        stg->SetFloat(dispID, disp);

        float scaleY = (ph < 0.0f) ? 1.0f : fabsf(cosf(ph * 3.14159265f));
        if (scaleY < 0.04f) scaleY = 0.04f;
        bool on = disp > 0.5f;

        ImVec2 c((pos.x + pos.x + w) * 0.5f, (pos.y + pos.y + h) * 0.5f);
        float hh = (h * 0.5f) * scaleY;
        ImVec2 mn(pos.x, c.y - hh), mx(pos.x + w, c.y + hh);

        float shade = 0.55f + 0.45f * scaleY;
        ImVec4 baseCol = on ? ImGui::ColorConvertU32ToFloat4(p.accent)
            : ImGui::ColorConvertU32ToFloat4(p.track);
        baseCol.x *= shade; baseCol.y *= shade; baseCol.z *= shade;
        float rr = r * scaleY; if (rr < 3.0f) rr = 3.0f;
        dl->AddRectFilled(mn, mx, ImGui::ColorConvertFloat4ToU32(baseCol), rr);
        ImVec4 hl(1, 1, 1, 0.16f * scaleY * (on ? 1.2f : 0.6f));
        dl->AddLine(ImVec2(mn.x + rr, mn.y + 1.5f), ImVec2(mx.x - rr, mn.y + 1.5f),
            ImGui::ColorConvertFloat4ToU32(hl), 1.0f);

        if (scaleY > 0.5f) {
            float ca = (scaleY - 0.5f) * 2.0f;
            if (on) {
                ImVec4 wc(1, 1, 1, ca);
                ImU32 wu = ImGui::ColorConvertFloat4ToU32(wc);
                dl->AddLine(ImVec2(c.x - 5, c.y + 0.5f), ImVec2(c.x - 1.5f, c.y + 4), wu, 2.0f);
                dl->AddLine(ImVec2(c.x - 1.5f, c.y + 4), ImVec2(c.x + 5.5f, c.y - 4), wu, 2.0f);
            }
            else {
                ImVec4 dc = Theme::U32ToVec4(p.textDim); dc.w *= ca;
                dl->AddLine(ImVec2(c.x - 5, c.y), ImVec2(c.x + 5, c.y),
                    ImGui::ColorConvertFloat4ToU32(dc), 2.0f);
            }
        }
        ImGui::PopID();
        return changed;
    }

    static void RowCheck(const char* label, bool* v, const Theme::Palette& p) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        float avail = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - 40.0f);
        CheckChip(label, v, p);
    }

    static bool FancySlider(const char* id, float* v, float vmin, float vmax,
        const char* vfmt, const Theme::Palette& p, float dispScale = 1.0f) {
        ImGui::PushID(id);
        float w = ImGui::GetContentRegionAvail().x;
        float h = 44.0f;
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("s", ImVec2(w, h));
        bool active = ImGui::IsItemActive();
        float trackY = pos.y + h - 10.0f;
        float x0 = pos.x + 10.0f, x1 = pos.x + w - 10.0f;
        float t = (*v - vmin) / (vmax - vmin);

        bool changed = false;
        if (active) {
            float mx = ImGui::GetIO().MousePos.x;
            float nt = (mx - x0) / (x1 - x0);
            nt = nt < 0 ? 0 : (nt > 1 ? 1 : nt);
            float nv = vmin + nt * (vmax - vmin);
            if (nv != *v) { *v = nv; changed = true; }
            t = nt;
        }
        ImDrawList* dl = ImGui::GetWindowDrawList();
        bool hovered = ImGui::IsItemHovered();
        ImGuiStorage* stg = ImGui::GetStateStorage();
        float dt = ImGui::GetIO().DeltaTime;

        ImGuiID dtk = ImGui::GetID("dispT");
        float dispT = stg->GetFloat(dtk, t);
        dispT = Approach(dispT, t, dt, 16.0f);
        if (fabsf(dispT - t) < 0.0005f) dispT = t;
        stg->SetFloat(dtk, dispT);
        float knobX = x0 + dispT * (x1 - x0);

        ImGuiID gk = ImGui::GetID("grow");
        float grow = stg->GetFloat(gk, 0.0f);
        float gTarget = active ? 1.0f : (hovered ? 0.55f : 0.0f);
        grow = Approach(grow, gTarget, dt, 18.0f);
        stg->SetFloat(gk, grow);
        float knobR = 9.0f + grow * 2.0f;
        float ringT = 1.5f + grow * 1.0f;

        dl->AddLine(ImVec2(x0, trackY), ImVec2(x1, trackY), p.track, 3.0f);
        dl->AddLine(ImVec2(x0, trackY), ImVec2(knobX, trackY), p.accent, 3.0f);
        if (grow > 0.01f) {
            ImVec4 halo = ImGui::ColorConvertU32ToFloat4(p.accent); halo.w = 0.15f * grow;
            dl->AddCircleFilled(ImVec2(knobX, trackY), knobR + 4.0f * grow,
                ImGui::ColorConvertFloat4ToU32(halo), 24);
        }
        ImVec4 sh(0, 0, 0, 0.18f);
        dl->AddCircleFilled(ImVec2(knobX, trackY + 1.5f), knobR + 1.0f, ImGui::ColorConvertFloat4ToU32(sh), 24);
        dl->AddCircleFilled(ImVec2(knobX, trackY), knobR, IM_COL32_WHITE, 24);
        dl->AddCircle(ImVec2(knobX, trackY), knobR, p.accent, 24, ringT);

        char buf[32]; snprintf(buf, sizeof(buf), vfmt, (*v) * dispScale);
        ImVec2 ts = ImGui::CalcTextSize(buf);
        float bw = ts.x + 18.0f, bh = ts.y + 8.0f;
        float bx = knobX - bw * 0.5f;
        if (bx < pos.x) bx = pos.x;
        if (bx + bw > pos.x + w) bx = pos.x + w - bw;
        float by = trackY - 16.0f - bh;
        dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), p.accentSoft, bh * 0.5f);
        dl->AddText(ImVec2(bx + 9.0f, by + 4.0f), p.accent, buf);

        ImGui::PopID();
        return changed;
    }

    static bool FancySliderInt(const char* id, int* v, int vmin, int vmax,
        const char* vfmt, const Theme::Palette& p) {
        float f = (float)*v;
        bool changed = FancySlider(id, &f, (float)vmin, (float)vmax, vfmt, p);
        *v = (int)(f + 0.5f);
        if (*v < vmin) *v = vmin;
        if (*v > vmax) *v = vmax;
        return changed;
    }

    static void RowDropdown(const char* label, int* idx, const char** items, int count,
        const Theme::Palette& p) {
        ImGui::PushID(label);
        float w = ImGui::GetContentRegionAvail().x;
        float h = 30.0f;
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("dd", ImVec2(w, h));
        bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) ImGui::OpenPopup("ddpop");
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float cy = pos.y + h * 0.5f;

        ImVec2 ls = ImGui::CalcTextSize(label);
        dl->AddText(ImVec2(pos.x, cy - ls.y * 0.5f), p.text, label);
        if (*idx < 0 || *idx >= count) *idx = 0;
        const char* cur = items[*idx];
        ImVec2 vs = ImGui::CalcTextSize(cur);
        float chevW = 16.0f;
        float vx = pos.x + w - chevW - vs.x - 6.0f;
        ImU32 vcol = hovered ? p.accent : p.textDim;
        dl->AddText(ImVec2(vx, cy - vs.y * 0.5f), vcol, cur);
        float ax = pos.x + w - chevW * 0.5f - 2.0f;
        dl->AddLine(ImVec2(ax - 4, cy - 2), ImVec2(ax, cy + 2), vcol, 1.6f);
        dl->AddLine(ImVec2(ax, cy + 2), ImVec2(ax + 4, cy - 2), vcol, 1.6f);

        ImGui::SetNextWindowPos(ImVec2(pos.x + w - 180, pos.y + h + 4));
        ImGui::SetNextWindowSize(ImVec2(180, 0));
        if (ImGui::BeginPopup("ddpop")) {
            for (int i = 0; i < count; i++) {
                bool sel = (i == *idx);
                if (ImGui::Selectable(items[i], sel)) *idx = i;
                if (sel) {
                    ImVec2 im = ImGui::GetItemRectMin(), iM = ImGui::GetItemRectMax();
                    float mcy = (im.y + iM.y) * 0.5f;
                    float mx = iM.x - 16;
                    ImGui::GetWindowDrawList()->AddLine(ImVec2(mx - 4, mcy), ImVec2(mx - 1, mcy + 3), p.accent, 1.8f);
                    ImGui::GetWindowDrawList()->AddLine(ImVec2(mx - 1, mcy + 3), ImVec2(mx + 4, mcy - 3), p.accent, 1.8f);
                }
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }

    static void SectionLabel(const char* txt, const Theme::Palette& p) {
        ImGui::PushStyleColor(ImGuiCol_Text, Theme::U32ToVec4(p.textDim));
        ImGui::TextUnformatted(txt);
        ImGui::PopStyleColor();
        ImGui::Dummy(ImVec2(0, 2));
    }

    static void RowToggle(const char* label, bool* v, const Theme::Palette& p) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);
        ImGui::SameLine();
        float avail = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - 46.0f);
        ToggleSwitch(label, v, p);
    }

    static void MultilineReadonly(const std::string& text, const char* emptyMsg, float height = 140.0f) {
        if (text.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", emptyMsg);
            return;
        }
        ImGui::InputTextMultiline("##MLR",
            const_cast<char*>(text.c_str()),
            text.size() + 1,
            ImVec2(-1, height),
            ImGuiInputTextFlags_ReadOnly);
    }

    // ================= Page: Home =================
    // ================= Page: Home =================
    static void PageHome(AppState& st, const Theme::Palette& p) {
        (void)st;
        SectionLabel(TR("VERSION INFO", "版本信息"), p);

        static const std::string versionText =
            "20260928\u53f7\u66f4\u65b0\n"
            "1.\u4fee\u590d20260927\u53f7\u66f4\u65b0\u7684\u6295\u63b7\u7269\u82e5\u73a9\u5bb6\u643a\u5e26\u591a\u4e2a\u91cd\u53e0\u95ee\u9898\n"
            "2.\u66f4\u6539UI\n"
            "3.\u53bb\u9664\u7f51\u7edc\u9a8c\u8bc1\u6539\u4e3a\u79bb\u7ebf";

        ImGui::InputTextMultiline("##VersionDisplay",
            const_cast<char*>(versionText.c_str()),
            versionText.size() + 1,
            ImVec2(-1, 110.0f),
            ImGuiInputTextFlags_ReadOnly);

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("ANNOUNCEMENT", "公告"), p);

        static const std::string announceText =
            "\u4f60\u8bf4\u7684\u6c38\u8fdc\uff0c\u4e0d\u5c5e\u4e8e\u6211\u3002\n"
            "\u660e\u660e\u53ea\u662f\u8d4c\u6c14\uff0c\u540e\u6765\u6210\u4e86\u6c38\u522b\u3002\n"
            "\u6211\u4e07\u822c\u73cd\u60dc\uff0c\u6700\u540e\u5374\u4e00\u65e0\u6240\u6709\u3002\n"
            "\u627f\u8bfa\u50cf\u96ea\u82b1\uff0c\u8f7b\u6613\u878d\u5316\u3002\n"
            "\u6545\u4e8b\u90fd\u8bb2\u70c2\u4e86\uff0c\u4ed6\u8fd8\u662f\u6211\u5fc3\u4e2d\u7684\u610f\u96be\u5e73\u3002\n"
            "\u6574\u4e2a\u57ce\u5e02\u90fd\u5165\u7761\u4e86\uff0c\u53ea\u5269\u6211\u548c\u6211\u7684\u5fc3\u601d\u591c\u4e0d\u80fd\u5b90\u3002\n"
            "\u6211\u50cf\u4e2a\u5077\u7a93\u8005\uff0c\u5077\u5077\u5173\u6ce8\u7740\u4e4b\u524d\u66fe\u5c5e\u4e8e\u6211\u7684\u4eba\u3002\n"
            "\u6211\u7684\u5fc3\u7075\u5df2\u7ecf\u88ab\u78be\u788e\u5f97\u65e0\u6cd5\u590d\u539f\u3002\n"
            "\u65e0\u6570\u4e2a\u9700\u8981\u4f60\u7684\u77ac\u95f4\u6211\u90fd\u95ed\u5634\u4e86\u3002";

        ImGui::InputTextMultiline("##AnnounceDisplay",
            const_cast<char*>(announceText.c_str()),
            announceText.size() + 1,
            ImVec2(-1, 200.0f),
            ImGuiInputTextFlags_ReadOnly);
    }
    // ================= Page: ESP =================
    static void PageESP(AppState& st, const Theme::Palette& p) {
        MenuData& m = *st.menu;
        SectionLabel(TR("ESP MASTER", "透视总开关"), p);
        RowToggle(TR("ESP Master Switch", "透视总开关"), &m.bMainESP, p);
        if (!m.bMainESP) {
            ImGui::Dummy(ImVec2(0, 8));
            ImGui::TextColored(Theme::U32ToVec4(p.textDim),
                "%s", TR("Enable the master switch to configure individual ESP features.",
                    "启用总开关后才能配置各细分透视功能。"));
            return;
        }

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("PLAYER OVERLAY", "玩家透视"), p);
        RowToggle(TR("Box ESP", "方框透视"), &m.bBoxESP, p);
        RowToggle(TR("Health ESP", "血量透视"), &m.bHealthESP, p);
        RowToggle(TR("Armor ESP", "护甲透视"), &m.bArmorESP, p);
        RowToggle(TR("Ray ESP", "射线透视"), &m.bRayESP, p);
        RowToggle(TR("Weapon ESP", "武器透视"), &m.bWeaponESP, p);
        RowToggle(TR("Distance ESP", "距离透视"), &m.bDistanceESP, p);

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("OBJECTIVE", "目标物"), p);
        RowToggle(TR("C4 ESP", "C4 透视"), &m.bC4ESP, p);

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("GRENADE", "投掷物"), p);
        RowToggle(TR("Grenade ESP", "投掷物透视"), &m.bGrenadeESP, p);
        if (m.bGrenadeESP) {
            ImGui::Indent(24.0f);
            RowCheck(TR("Show Carried", "绘制携带"), &m.bGrenadeCarried, p);
            RowCheck(TR("Show Thrown", "绘制丢出"), &m.bGrenadeThrown, p);
            ImGui::Unindent(24.0f);
        }

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("TEAM", "队伍"), p);
        RowToggle(TR("Remove friends", "排除队友"), &m.bRemoveFriends, p);
    }

    // ================= Page: Aim =================
    static void PageAim(AppState& st, const Theme::Palette& p) {
        MenuData& m = *st.menu;
        SectionLabel(TR("AIM ASSIST", "自瞄"), p);
        RowToggle(TR("Auto Aim", "自动瞄准"), &m.bAutoAim, p);
        RowToggle(TR("Silence Aim", "静默瞄准"), &m.bSilentAim, p);

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("SMOOTHING", "平滑度"), p);
        if (!m.bAutoAim) ImGui::BeginDisabled();
        ImGui::Text("%s", TR("Aim Smoothness", "瞄准平滑度"));
        FancySlider("aimSmooth", &m.fAimSmooth, 0.1f, 10.0f, "%.1f", p);
        ImGui::Text("%s", TR("Aim Speed", "瞄准速度"));
        FancySlider("aimSpeed", &m.fAimSpeed, 0.1f, 5.0f, "%.1f", p);
        if (!m.bAutoAim) ImGui::EndDisabled();

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("AIM KEY", "自瞄按键"), p);
        static const char* aimKeys[] = {
            "Ctrl", "Shift", "W", "Q", "E", "R",
            "Mouse L", "Mouse R", "Mouse4", "Mouse5", "Mouse M",
            "Z", "X", "C", "CapsLock"
        };
        RowDropdown(TR("Aim Key", "自瞄按键"), &m.aimKey, aimKeys, IM_ARRAYSIZE(aimKeys), p);

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("FILTERS", "过滤"), p);
        RowCheck(TR("Visible aiming", "仅瞄准可见目标"), &m.bVisibleAim, p);
        RowCheck(TR("Aim friends", "瞄准队友"), &m.bAimFriends, p);

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("SCOPE", "自瞄范围"), p);
        RowToggle(TR("Aim Scope", "显示自瞄范围"), &m.bAimScope, p);
        if (m.bAimScope) {
            ImGui::Text("%s", TR("Aim FOV", "自瞄视野"));
            FancySlider("aimFov", &m.fAimFOV, 1.0f, 60.0f, "%.1f deg", p);
        }
        if (m.bAutoAim) {
            RowToggle(TR("Currently locked", "显示自瞄锁定线"), &m.bShowAimLock, p);
        }
    }

    // ================= Page: Other =================
    static void PageOther(AppState& st, const Theme::Palette& p) {
        MenuData& m = *st.menu;

        SectionLabel(TR("VISUAL", "视觉"), p);
        RowToggle(TR("Anti-Flash", "防闪光"), &m.bAntiFlash, p);

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("CUSTOM FOV", "自定义视野"), p);
        RowToggle(TR("Custom FOV", "自定义视野"), &m.bFOVChanger, p);
        if (m.bFOVChanger) {
            ImGui::Text("%s", TR("FOV Value", "视野值"));
            FancySlider("fovVal", &m.fFOVValue, 60.0f, 120.0f, "%.0f", p);
        }

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("THIRD PERSON", "第三人称"), p);
        RowToggle(TR("Third Person", "第三人称"), &m.bThirdPerson, p);
        if (m.bThirdPerson) {
            ImGui::Text("%s", TR("Third Person Distance", "第三人称距离"));
            FancySlider("tpDist", &m.fThirdPersonDist, 50.0f, 500.0f, "%.0f", p);

            const char* tpStyles[] = { "Toggle", "Hold", "Always" };
            RowDropdown(TR("Third Person Mode", "第三人称模式"),
                &m.thirdPersonStyle, tpStyles, IM_ARRAYSIZE(tpStyles), p);

            if (m.thirdPersonStyle != 2) {
                static const char* tpKeys[] = {
                    "F5", "F6", "F7", "F8", "F9",
                    "Ctrl", "Shift", "V", "B", "N", "CapsLock"
                };
                RowDropdown(TR("Third Person Key", "第三人称按键"),
                    &m.thirdPersonKey, tpKeys, IM_ARRAYSIZE(tpKeys), p);
            }
            RowCheck(TR("Disable Transition", "禁用过渡动画"), &m.bThirdPersonNoInterp, p);
        }

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("PLAYER GLOW", "玩家热能"), p);
        RowToggle(TR("Glow ESP", "玩家热能"), &m.bGlowESP, p);
        if (m.bGlowESP) {
            ImGui::Indent(24.0f);
            RowCheck(TR("Separate Visible/Invisible", "区分可见"), &m.bGlowSeparateColors, p);
            if (m.bGlowSeparateColors) {
                ImGui::ColorEdit4(TR("Visible Color", "可视颜色"),
                    &m.GlowColorVisible.x, ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit4(TR("Invisible Color", "不可视颜色"),
                    &m.GlowColorInvisible.x, ImGuiColorEditFlags_NoInputs);
            }
            else {
                ImGui::ColorEdit4(TR("Glow Color", "发光颜色"),
                    &m.GlowColor.x, ImGuiColorEditFlags_NoInputs);
            }
            ImGui::Unindent(24.0f);
        }
    }

    // ================= Page: Performance =================
    static void PagePerformance(AppState& st, const Theme::Palette& p) {
        MenuData& m = *st.menu;

        SectionLabel(TR("UPDATE", "更新设置"), p);
        ImGui::Text("%s", TR("Update Interval (frames)", "更新间隔（帧数）"));
        FancySliderInt("updInt", &m.performance.updateInterval, 1, 10, "%.0f", p);
        RowCheck(TR("Dynamic Update", "动态更新"), &m.performance.enableDynamicUpdate, p);

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("DISTANCE FILTER", "距离过滤"), p);
        RowCheck(TR("Enable Distance Filter", "启用距离过滤"),
            &m.performance.enableDistanceFilter, p);
        if (m.performance.enableDistanceFilter) {
            ImGui::Text("%s", TR("Max Draw Distance", "最大绘制距离"));
            FancySlider("maxDist", &m.performance.maxDrawDistance, 1000.0f, 15000.0f, "%.0f", p);
            ImGui::Text("%s", TR("C4 Max Distance", "C4最大距离"));
            FancySlider("c4Dist", &m.performance.c4MaxDistance, 1000.0f, 10000.0f, "%.0f", p);
        }

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("LEVEL OF DETAIL", "细节层次"), p);
        RowCheck(TR("Enable LOD", "启用LOD"), &m.performance.enableLOD, p);
        if (m.performance.enableLOD) {
            ImGui::Text("%s", TR("LOD1 Distance", "LOD1距离"));
            FancySlider("lod1", &m.performance.lodDistance1, 1000.0f, 8000.0f, "%.0f", p);
            ImGui::Text("%s", TR("LOD2 Distance", "LOD2距离"));
            FancySlider("lod2", &m.performance.lodDistance2, 2000.0f, 12000.0f, "%.0f", p);
        }

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("CACHE", "缓存"), p);
        RowCheck(TR("Enable Weapon Cache", "启用武器缓存"),
            &m.performance.enableWeaponCache, p);

        ImGui::Dummy(ImVec2(0, 12));
        SectionLabel(TR("QUICK PRESETS", "快速预设"), p);
        if (ImGui::Button(TR("Performance", "性能模式"), ImVec2(130, 0))) {
            m.performance.updateInterval = 5;
            m.performance.maxDrawDistance = 5000.0f;
            m.performance.c4MaxDistance = 3000.0f;
            m.performance.enableLOD = true;
            m.performance.lodDistance1 = 2000.0f;
            m.performance.lodDistance2 = 4000.0f;
            m.performance.enableDistanceFilter = true;
            m.performance.enableDynamicUpdate = true;
            m.performance.enableWeaponCache = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(TR("Balanced", "均衡模式"), ImVec2(130, 0))) {
            m.performance.updateInterval = 3;
            m.performance.maxDrawDistance = 8000.0f;
            m.performance.c4MaxDistance = 5000.0f;
            m.performance.enableLOD = true;
            m.performance.lodDistance1 = 3000.0f;
            m.performance.lodDistance2 = 6000.0f;
            m.performance.enableDistanceFilter = true;
            m.performance.enableDynamicUpdate = true;
            m.performance.enableWeaponCache = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(TR("Quality", "质量模式"), ImVec2(130, 0))) {
            m.performance.updateInterval = 1;
            m.performance.maxDrawDistance = 12000.0f;
            m.performance.c4MaxDistance = 8000.0f;
            m.performance.enableLOD = false;
            m.performance.enableDistanceFilter = false;
            m.performance.enableDynamicUpdate = true;
            m.performance.enableWeaponCache = true;
        }

        ImGui::Dummy(ImVec2(0, 12));
        SectionLabel(TR("PERFORMANCE INFO", "性能信息"), p);
        float fps = ImGui::GetIO().Framerate;
        ImGui::Text("%s: %.1f", TR("Current FPS", "当前FPS"), fps);
        ImGui::Text("%s: %d", TR("Update Interval", "更新间隔"),
            m.performance.updateInterval);
        ImGui::Text("%s: %.0f", TR("Draw Distance", "绘制距离"),
            m.performance.maxDrawDistance);
        ImGui::Text("%s: %s", TR("LOD", "LOD状态"),
            m.performance.enableLOD ? TR("Enabled", "启用") : TR("Disabled", "禁用"));
    }

    // ================= Page: Settings =================
    static void PageSettings(AppState& st, const Theme::Palette& p) {
        MenuData& m = *st.menu;

        SectionLabel(TR("LANGUAGE", "语言"), p);
        static const char* langItems[] = { "English", "中文" };
        int prevLang = m.language;
        RowDropdown(TR("Language", "语言"), &m.language, langItems, IM_ARRAYSIZE(langItems), p);
        if (m.language < 0 || m.language > 1) m.language = 0;
        if (m.language != prevLang) {
            Lang::SetLanguage(static_cast<Language>(m.language));
        }

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("AIM LOCK", "锁定线"), p);
        ImGui::ColorEdit4(TR("Aim Lock Color", "锁定线颜色"),
            &m.AimLockColor.x, ImGuiColorEditFlags_NoInputs);
        ImGui::Text("%s", TR("Ray Thickness", "射线粗细"));
        FancySlider("rayTh", &m.RayThickness, 0.5f, 5.0f, "%.1f", p);

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("VISIBLE / INVISIBLE COLORS", "可见 / 不可见颜色"), p);
        ImGui::ColorEdit4(TR("Box Visible", "方框-可见"),
            &m.BoxColorVisible.x, ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit4(TR("Box Invisible", "方框-不可见"),
            &m.BoxColorInvisible.x, ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit4(TR("Ray Visible", "射线-可见"),
            &m.RayColorVisible.x, ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit4(TR("Ray Invisible", "射线-不可见"),
            &m.RayColorInvisible.x, ImGuiColorEditFlags_NoInputs);

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("WINDOW APPEARANCE", "窗口外观"), p);
        ImGui::Text("%s", TR("Window Transparency", "窗口透明度"));
        FancySlider("wAlpha", &m.WindowAlpha, 0.4f, 1.0f, "%.2f", p);
        ImGui::Text("%s", TR("UI Corner Radius", "界面圆角"));
        FancySlider("wRound", &m.WindowRound, 0.0f, 14.0f, "%.0f", p);
        ImGui::Text("%s", TR("Global Font Scale", "全局字体缩放"));
        FancySlider("fontScale", &m.FontScale, 0.7f, 1.4f, "%.2f", p);

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("LAYOUT SPACING", "布局间距"), p);
        ImGui::Text("%s", TR("Item Spacing", "元素间距"));
        FancySlider("itemSp", &m.ItemSpacing, 4.0f, 12.0f, "%.0f", p);
        ImGui::Text("%s", TR("Frame Padding X", "框架水平内边距"));
        FancySlider("fpX", &m.FramePaddingX, 3.0f, 10.0f, "%.0f", p);
        ImGui::Text("%s", TR("Frame Padding Y", "框架垂直内边距"));
        FancySlider("fpY", &m.FramePaddingY, 3.0f, 10.0f, "%.0f", p);

        ImGui::Dummy(ImVec2(0, 8));
        SectionLabel(TR("DEBUG", "调试选项"), p);
        RowToggle(TR("Grenade Debug Info", "投掷物调试信息"),
            &m.bGrenadeDebug, p);
    }

    // ================= Page: About =================
    static void PageAbout(AppState& st, const Theme::Palette& p) {
        (void)st;
        SectionLabel(TR("CREDITS", "制作者名单"), p);
        ImGui::Text("%s", TR("Copyright @Qingning", "版权所有 @Qingning"));
        ImGui::Text("%s", TR("qwit (Dawn)", "qwit（黎明）"));
        ImGui::Text("%s", TR("Qingning (Pudding Bear)", "Qingning（布丁小熊）"));
        ImGui::Text("__invoker (Bilibili UID@174466535)");

        ImGui::Dummy(ImVec2(0, 12));
        SectionLabel(TR("ACKNOWLEDGMENTS", "致谢名单"), p);
        ImGui::Text("%s", TR("List of Acknowledgments", "致谢名单"));

        ImGui::Dummy(ImVec2(0, 12));
        SectionLabel(TR("COMMUNITY", "官方交流"), p);
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
            "%s", TR("Official QQ Group", "官方交流QQ群"));
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
            "%s", TR("One: 1041141006", "一群：1041141006"));

        (void)p;
    }

    // ---- 导航图标 ----
    static void NavIcon(ImDrawList* dl, int kind, ImVec2 c, float s, ImU32 col, float th) {
        switch (kind) {
        case 0:
            dl->AddLine(ImVec2(c.x - s * 0.5f, c.y + s * 0.1f), ImVec2(c.x, c.y - s * 0.45f), col, th);
            dl->AddLine(ImVec2(c.x, c.y - s * 0.45f), ImVec2(c.x + s * 0.5f, c.y + s * 0.1f), col, th);
            dl->AddRect(ImVec2(c.x - s * 0.32f, c.y + s * 0.1f), ImVec2(c.x + s * 0.32f, c.y + s * 0.45f), col, 1.0f, 0, th);
            break;
        case 1:
            dl->AddRect(ImVec2(c.x - s * 0.42f, c.y - s * 0.42f), ImVec2(c.x + s * 0.42f, c.y + s * 0.42f), col, 2.0f, 0, th);
            dl->AddCircleFilled(c, s * 0.12f, col);
            break;
        case 2:
            dl->AddCircle(c, s * 0.42f, col, 16, th);
            dl->AddLine(ImVec2(c.x - s * 0.55f, c.y), ImVec2(c.x + s * 0.55f, c.y), col, th);
            dl->AddLine(ImVec2(c.x, c.y - s * 0.55f), ImVec2(c.x, c.y + s * 0.55f), col, th);
            break;
        case 3: {
            float r = s * 0.5f, rin = s * 0.18f;
            ImVec2 pts[8];
            for (int i = 0; i < 8; i++) {
                float ang = -1.5708f + i * 0.7853982f;
                float rad = (i % 2 == 0) ? r : rin;
                pts[i] = ImVec2(c.x + cosf(ang) * rad, c.y + sinf(ang) * rad);
            }
            dl->AddConvexPolyFilled(pts, 8, col);
            break;
        }
        case 4:
            dl->AddLine(ImVec2(c.x - s * 0.4f, c.y + s * 0.3f), ImVec2(c.x + s * 0.4f, c.y - s * 0.3f), col, th);
            dl->AddLine(ImVec2(c.x + s * 0.4f, c.y - s * 0.3f), ImVec2(c.x + s * 0.05f, c.y - s * 0.3f), col, th);
            dl->AddLine(ImVec2(c.x + s * 0.4f, c.y - s * 0.3f), ImVec2(c.x + s * 0.4f, c.y + s * 0.05f), col, th);
            break;
        case 5:
            dl->AddLine(ImVec2(c.x - s * 0.5f, c.y - s * 0.28f), ImVec2(c.x + s * 0.5f, c.y - s * 0.28f), col, th);
            dl->AddLine(ImVec2(c.x - s * 0.5f, c.y + s * 0.28f), ImVec2(c.x + s * 0.5f, c.y + s * 0.28f), col, th);
            dl->AddCircleFilled(ImVec2(c.x + s * 0.16f, c.y - s * 0.28f), th * 1.3f, col);
            dl->AddCircleFilled(ImVec2(c.x - s * 0.16f, c.y + s * 0.28f), th * 1.3f, col);
            break;
        case 6:
            dl->AddCircle(c, s * 0.46f, col, 20, th);
            dl->AddCircleFilled(ImVec2(c.x, c.y - s * 0.22f), th * 0.8f, col);
            dl->AddLine(ImVec2(c.x, c.y - s * 0.05f), ImVec2(c.x, c.y + s * 0.25f), col, th);
            break;
        }
    }

    static void BottomNav(AppState& st, const Theme::Palette& p, ImVec2 areaMin, ImVec2 areaMax) {
        const char* tabs[] = { "Home", "ESP", "Aim", "Other", "Perf", "Settings", "About" };
        const int N = 7;
        float navW = 720.0f, navH = 58.0f;
        float cx = (areaMin.x + areaMax.x) * 0.5f;
        ImVec2 min(cx - navW * 0.5f, areaMax.y - navH - 16.0f);
        ImVec2 max(min.x + navW, min.y + navH);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float rounding = navH * 0.5f;

        GlassPill(dl, min, max, p, rounding);

        float cellW = navW / N;
        float target = (float)st.page;
        st.navIndic = Spring(st.navIndic, target, st.navVel, ImGui::GetIO().DeltaTime);
        float ix = min.x + (st.navIndic + 0.5f) * cellW;

        ImVec2 selMin(ix - cellW * 0.46f, min.y + 7), selMax(ix + cellW * 0.46f, max.y - 7);
        float selR = (selMax.y - selMin.y) * 0.5f;
        ImVec4 selCol = ImGui::ColorConvertU32ToFloat4(p.accent); selCol.w = 0.12f;
        dl->AddRectFilled(selMin, selMax, ImGui::ColorConvertFloat4ToU32(selCol), selR);

        float lampW = 24.0f, lampH = 5.0f;
        float lampCY = min.y - 2.0f;
        for (int b = 10; b >= 1; b--) {
            float rx = 8.0f + b * 4.5f;
            float ry = 5.0f + b * 3.4f;
            ImVec4 bc = ImGui::ColorConvertU32ToFloat4(p.accent);
            bc.w = 0.05f * (11 - b) / 10.0f;
            dl->AddEllipseFilled(ImVec2(ix, lampCY), ImVec2(rx, ry),
                ImGui::ColorConvertFloat4ToU32(bc), 0.0f, 24);
        }
        const int strips = 60;
        float glowHalf = lampW * 0.8f;
        float gTopY = min.y + 1.0f, gBotY = min.y + 26.0f;
        ImVec4 gBot = ImGui::ColorConvertU32ToFloat4(p.accent); gBot.w = 0.0f;
        ImU32 gBotU = ImGui::ColorConvertFloat4ToU32(gBot);
        float spacing = (2.0f * glowHalf) / strips;
        for (int s = 0; s < strips; s++) {
            float fx = (s / (strips - 1.0f)) * 2.0f - 1.0f;
            float bell = 1.0f - fx * fx;
            if (bell <= 0.0f) continue;
            float sx = ix + fx * glowHalf;
            float sw = spacing * 5.0f;
            ImVec4 ct = ImGui::ColorConvertU32ToFloat4(p.accent); ct.w = 0.055f * bell;
            ImU32 ctU = ImGui::ColorConvertFloat4ToU32(ct);
            dl->AddRectFilledMultiColor(ImVec2(sx - sw * 0.5f, gTopY), ImVec2(sx + sw * 0.5f, gBotY),
                ctU, ctU, gBotU, gBotU);
        }
        dl->AddRectFilled(ImVec2(ix - lampW * 0.5f, lampCY - lampH * 0.5f),
            ImVec2(ix + lampW * 0.5f, lampCY + lampH * 0.5f),
            p.accent, lampH * 0.5f);

        for (int i = 0; i < N; i++) {
            float x0 = min.x + i * cellW;
            bool sel = (i == st.page);
            ImU32 tc = sel ? p.accent : p.textDim;
            ImVec2 ts = ImGui::CalcTextSize(tabs[i]);
            float iconSz = 14.0f, gap = 6.0f;
            float groupW = iconSz + gap + ts.x;
            float gx = x0 + (cellW - groupW) * 0.5f;
            float cy = (min.y + max.y) * 0.5f;
            NavIcon(dl, i, ImVec2(gx + iconSz * 0.5f, cy), iconSz, tc, sel ? 1.8f : 1.5f);
            dl->AddText(ImVec2(gx + iconSz + gap, cy - ts.y * 0.5f), tc, tabs[i]);
            ImGui::SetCursorScreenPos(ImVec2(x0, min.y));
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("nav", ImVec2(cellW, navH))) st.page = i;
            ImGui::PopID();
        }
    }

    static void TitleBar(AppState& st, const Theme::Palette& p, ImVec2 min, ImVec2 max, ImFont* bigFont) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        (void)bigFont;

        ImVec2 lc(min.x + 28, (min.y + max.y) * 0.5f);
        float discR = 18.0f;
        ImU32 discCol = ImGui::ColorConvertFloat4ToU32(ImLerp(
            ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
            ImVec4(0.13f, 0.13f, 0.17f, 1.0f),
            st.themeT));
        dl->AddCircleFilled(lc, discR, discCol, 32);
        dl->AddCircleFilled(lc, discR, p.accentSoft, 32);
        dl->AddCircle(lc, discR, p.cardBorder, 32, 1.0f);

        float cy = (min.y + max.y) * 0.5f;
        ImFont* font = ImGui::GetFont();
        float fs = ImGui::GetFontSize();
        ImFont* bf = bigFont ? bigFont : font;

        float s1 = 20.0f, s2 = 11.0f, gap = 2.0f;
        float blockH = s1 + gap + s2;
        float by = cy - blockH * 0.5f;
        const char* kFull = "SeroDraw";
        int kLen = 8;
        st.caesarTimer += ImGui::GetIO().DeltaTime;
        if (st.caesarTimer >= 10.0f) { st.caesarTimer = 0.0f; st.caesarScramble = 0.0f; }
        if (st.caesarScramble < 1.0f)
            st.caesarScramble += ImGui::GetIO().DeltaTime / 1.1f;
        float prog = st.caesarScramble > 1.0f ? 1.0f : st.caesarScramble;
        char shown[64];
        static const char glyph[] = "!@#$%*+=?0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        int resolved = (int)(prog * kLen + 0.0001f);
        for (int i = 0; i < kLen; i++) {
            if (i < resolved) shown[i] = kFull[i];
            else {
                int r = (int)(ImGui::GetTime() * 30.0f + i * 7) % (int)(sizeof(glyph) - 1);
                shown[i] = glyph[r];
            }
        }
        shown[kLen] = '\0';
        float txtX = min.x + 64;
        dl->AddText(bf, s1, ImVec2(txtX, by), p.text, shown);
        dl->AddText(font, s2, ImVec2(txtX, by + s1 + gap), p.textDim, TR("Right Shift to hide menu", "右Shift隐藏菜单"));

        static const char* kTitles[7] = {
            "Home", "ESP", "Aim Assist", "Other", "Performance", "Settings", "About"
        };
        static const char* kSubs[7] = {
            "Version & announcements", "Visual overlays", "Aim & silent",
            "Misc features", "CPU & GPU tuning", "Appearance & layout", "Credits"
        };
        if (st.titlePage != st.page) {
            st.titleFade -= ImGui::GetIO().DeltaTime / 0.12f;
            if (st.titleFade <= 0.0f) { st.titleFade = 0.0f; st.titlePage = st.page; }
        }
        else if (st.titleFade < 1.0f) {
            st.titleFade += ImGui::GetIO().DeltaTime / 0.16f;
            if (st.titleFade > 1.0f) st.titleFade = 1.0f;
        }
        int tp = st.titlePage;
        float scSize = 24.0f, scX = min.x + 168;
        ImVec4 tcol = Theme::U32ToVec4(p.text);   tcol.w *= st.titleFade;
        ImVec4 scol = Theme::U32ToVec4(p.textDim); scol.w *= st.titleFade;
        float yoff = (1.0f - st.titleFade) * -4.0f;
        dl->AddText(bf, scSize, ImVec2(scX, cy - scSize * 0.5f + yoff),
            ImGui::ColorConvertFloat4ToU32(tcol), kTitles[tp]);
        float scW = bf->CalcTextSizeA(scSize, FLT_MAX, 0.0f, kTitles[tp]).x;
        dl->AddText(font, fs, ImVec2(scX + scW + 16, cy - fs * 0.5f + yoff),
            ImGui::ColorConvertFloat4ToU32(scol), kSubs[tp]);

        float sw = 56.0f, sh = 28.0f;
        ImVec2 tmin(max.x - 224, (min.y + max.y) * 0.5f - sh * 0.5f);
        ImVec2 tmax(tmin.x + sw, tmin.y + sh);
        ImGui::SetCursorScreenPos(tmin);
        if (ImGui::InvisibleButton("themeSw", ImVec2(sw, sh))) st.dark = !st.dark;
        float k = st.themeT;
        float scy = tmin.y + sh * 0.5f;
        float knobx = tmin.x + sh * 0.5f + (sw - sh) * k;
        ImVec2 knobC(knobx, scy);
        float knobR = sh * 0.5f - 3.0f;

        ImVec4 trkFill = ImGui::ColorConvertU32ToFloat4(p.card); trkFill.w = 0.0f;
        dl->AddRectFilled(tmin, tmax, ImGui::ColorConvertFloat4ToU32(trkFill), sh * 0.5f);
        ImVec4 rim = ImGui::ColorConvertU32ToFloat4(p.text); rim.w = 0.28f;
        dl->AddRect(tmin, tmax, ImGui::ColorConvertFloat4ToU32(rim), sh * 0.5f, 0, 1.3f);

        auto drawSun = [&](ImVec2 c, ImU32 col, float rr) {
            dl->AddCircleFilled(c, rr, col, 16);
            for (int i = 0; i < 8; i++) {
                float a = i * 0.7853982f;
                dl->AddLine(ImVec2(c.x + cosf(a) * (rr + 2), c.y + sinf(a) * (rr + 2)),
                    ImVec2(c.x + cosf(a) * (rr + 3.6f), c.y + sinf(a) * (rr + 3.6f)), col, 1.3f);
            }
            };
        auto drawMoon = [&](ImVec2 c, ImU32 col, ImU32 maskCol, float mr) {
            dl->AddCircleFilled(c, mr, col, 20);
            dl->AddCircleFilled(ImVec2(c.x + 2.2f, c.y - 1.4f), mr, maskCol, 20);
            };
        ImU32 dimCol = ImGui::ColorConvertFloat4ToU32(ImLerp(
            ImVec4(0, 0, 0, 0), ImGui::ColorConvertU32ToFloat4(p.textDim), 1.0f));
        ImVec2 leftC(tmin.x + sh * 0.5f, scy), rightC(tmax.x - sh * 0.5f, scy);
        if (k > 0.5f) drawSun(leftC, dimCol, 3.2f);
        else          drawMoon(rightC, dimCol, p.card, 5.0f);

        ImVec4 halo(1, 1, 1, 0.25f);
        dl->AddCircleFilled(knobC, knobR + 3.0f, ImGui::ColorConvertFloat4ToU32(halo), 28);
        dl->AddCircleFilled(ImVec2(knobC.x, knobC.y + 1.0f), knobR, IM_COL32(0, 0, 0, 35), 28);
        dl->AddCircleFilled(knobC, knobR, IM_COL32_WHITE, 28);
        if (k > 0.5f) drawMoon(knobC, p.accent, IM_COL32_WHITE, 5.0f);
        else          drawSun(knobC, p.accent, 3.2f);

        ImVec2 rs = ImGui::CalcTextSize("Ready");
        float dotR = 4.0f, padL = 16.0f, dotGap = 8.0f, padR = 16.0f;
        float badgeW = padL + dotR * 2 + dotGap + rs.x + padR;
        float badgeH = 30.0f;
        ImVec2 bmax(max.x - 24, (min.y + max.y) * 0.5f + badgeH * 0.5f);
        ImVec2 bmin(bmax.x - badgeW, bmax.y - badgeH);
        float bcy = (bmin.y + bmax.y) * 0.5f;
        dl->AddRectFilled(bmin, bmax, p.accentSoft, badgeH * 0.5f);
        dl->AddCircleFilled(ImVec2(bmin.x + padL + dotR, bcy), dotR, p.success);
        dl->AddText(ImVec2(bmin.x + padL + dotR * 2 + dotGap, bcy - rs.y * 0.5f), p.text, "Ready");
    }

    void Draw(AppState& st, const Theme::Palette& p, float dt, ImFont* bigFont) {
        st.themeT = Approach(st.themeT, st.dark ? 1.0f : 0.0f, dt, 8.0f);
        st.beams.Update(dt);
        st.burst.Update(dt);

        if (!st.menu) return;

        // ★ 完全缩到中心（隐藏）→ 直接不画
        if (st.menuScale < 0.005f) return;

        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::Begin("root", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = vp->WorkPos, ws = vp->WorkSize;

        float cardW = ImMin(ws.x - 80.0f, 960.0f);
        float cardH = ImMin(ws.y - 80.0f, 660.0f);
        ImVec2 cmin(wp.x + (ws.x - cardW) * 0.5f, wp.y + (ws.y - cardH) * 0.5f);
        ImVec2 cmax(cmin.x + cardW, cmin.y + cardH);

        // ★★★ 根据 menuScale 计算裁切区域（从中心向外扩张 / 向中心收缩）
        float s = st.menuScale;
        if (s < 0.0f) s = 0.0f;
        if (s > 1.0f) s = 1.0f;

        ImVec2 center((cmin.x + cmax.x) * 0.5f, (cmin.y + cmax.y) * 0.5f);
        float halfW = (cmax.x - cmin.x) * 0.5f * s;
        float halfH = (cmax.y - cmin.y) * 0.5f * s;

        ImVec2 clipMin(center.x - halfW, center.y - halfH);
        ImVec2 clipMax(center.x + halfW, center.y + halfH);

        // 用 ImGui::PushClipRect 影响本窗口后续**所有**绘制（含子控件）
        ImGui::PushClipRect(clipMin, clipMax, true);

        // ===== 以下所有绘制完全保持不变 =====
        PanelWithShadow(dl, cmin, cmax, p.card, p.cardBorder, p.shadow, 22.0f);
        if (st.showDots) Fx::DrawDotGrid(dl, cmin, cmax, p.dotGrid, 16.0f);
        if (st.showBeams) st.beams.Draw(dl, cmin, cmax, p.accent);

        ImVec2 tbMin(cmin.x, cmin.y), tbMax(cmax.x, cmin.y + 64);
        TitleBar(st, p, tbMin, tbMax, bigFont);
        dl->AddLine(ImVec2(cmin.x + 20, tbMax.y), ImVec2(cmax.x - 20, tbMax.y), p.cardBorder, 1.0f);

        ImGui::SetCursorScreenPos(ImVec2(cmin.x + 28, tbMax.y + 16));
        ImGui::BeginChild("content", ImVec2(cardW - 56, cardH - 64 - 16 - 84), false,
            ImGuiWindowFlags_NoBackground);
        switch (st.page) {
        case 0: PageHome(st, p);        break;
        case 1: PageESP(st, p);         break;
        case 2: PageAim(st, p);         break;
        case 3: PageOther(st, p);       break;
        case 4: PagePerformance(st, p); break;
        case 5: PageSettings(st, p);    break;
        case 6: PageAbout(st, p);       break;
        }
        ImGui::EndChild();

        BottomNav(st, p, cmin, cmax);
        st.burst.Draw(dl);
        // ===== 绘制结束 =====

        ImGui::PopClipRect();   // ★ 取消裁切

        ImGui::End();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }
} // namespace UI