#include "ImGui_Menu.h"
#include "features/cheat.hpp"
#include <cstring>
#include <algorithm>
#include <unordered_map>
#include <string>
#include <vector>
#include <map>
#include <time.h>
#include <random>

MenuData g_MenuData;
static bool g_styleInitialized = false;

struct HUDNotification {
    std::string text;
    std::string subtext;
    float progress;
    float lifetime;
    float max_lifetime;
    bool is_closing;
    ImU32 color;
    float exit_offset_y;
    HUDNotification()
        : text(""), subtext(""), progress(0.0f), lifetime(0.0f), max_lifetime(3.0f),
        is_closing(false), color(IM_COL32(100, 200, 100, 255)), exit_offset_y(0.0f) {
    }
};

static std::vector<HUDNotification> hud_notifications;

static void AddHUDNotification(const std::string& text, const std::string& subtext = "",
    ImU32 color = IM_COL32(100, 200, 100, 255)) {
    const size_t MAX_NOTIFICATIONS = 5;
    if (hud_notifications.size() >= MAX_NOTIFICATIONS) {
        HUDNotification& oldest = hud_notifications.front();
        if (!oldest.is_closing) {
            oldest.is_closing = true;
            oldest.progress = 1.0f;
            oldest.exit_offset_y = 0.0f;
        }
    }
    HUDNotification notif;
    notif.text = text;
    notif.subtext = subtext;
    notif.color = color;
    hud_notifications.push_back(notif);
}

static float EaseOut(float t) {
    return 1.0f - (1.0f - t) * (1.0f - t);
}

struct ToggleAnimation {
    float progress;
    float target;
    float speed;
    ToggleAnimation() : progress(0.0f), target(0.0f), speed(0.15f) {}
    void update(bool enabled) {
        target = enabled ? 1.0f : 0.0f;
        if (progress < target) { progress += speed; if (progress > target) progress = target; }
        else if (progress > target) { progress -= speed; if (progress < target) progress = target; }
    }
    float getPosition(float minPos, float maxPos) {
        float t = progress;
        float eased = 1.0f - (1.0f - t) * (1.0f - t);
        return minPos + (maxPos - minPos) * eased;
    }
};
static std::map<const char*, ToggleAnimation> toggle_animations;

static void ToggleSwitch(const char* id, bool* value, const char* feature_name = nullptr) {
    float frame_height = ImGui::GetFrameHeight();
    const float switch_width = 40.0f;
    const float switch_height = 20.0f;

    ImVec2 cursor = ImGui::GetCursorScreenPos();
    float offset_y = (frame_height - switch_height) / 2.0f;
    ImVec2 toggle_pos(cursor.x, cursor.y + offset_y);
    ImVec2 toggle_size(switch_width, switch_height);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    toggle_animations[id].update(*value);
    float anim_progress = toggle_animations[id].progress;
    int r = 80 + (int)(120 * EaseOut(anim_progress));
    int g = 80 + (int)(120 * EaseOut(anim_progress));
    int b = 80 + (int)(0 * EaseOut(anim_progress));
    ImU32 bg_color = IM_COL32(r, g, b, 255);

    draw_list->AddRectFilled(toggle_pos, ImVec2(toggle_pos.x + toggle_size.x, toggle_pos.y + toggle_size.y),
        bg_color, toggle_size.y / 2);

    float knob_radius = toggle_size.y / 2 - 2;
    float min_x = toggle_pos.x + knob_radius + 2;
    float max_x = toggle_pos.x + toggle_size.x - knob_radius - 2;
    float knob_x = toggle_animations[id].getPosition(min_x, max_x);

    draw_list->AddCircleFilled(ImVec2(knob_x, toggle_pos.y + toggle_size.y / 2),
        knob_radius, IM_COL32(255, 255, 255, 255));

    ImGui::InvisibleButton(id, ImVec2(switch_width, frame_height));
    if (ImGui::IsItemClicked()) {
        bool new_value = !*value;
        *value = new_value;
        if (feature_name) {
            std::string status = new_value ? "Enabled" : "Disabled";
            ImU32 color = new_value ? IM_COL32(100, 200, 100, 255) : IM_COL32(200, 100, 100, 255);
            AddHUDNotification(feature_name, status, color);
        }
    }

    if (feature_name && feature_name[0]) {
        ImGui::SameLine(0, 12.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s", feature_name);
    }
}

void RenderHUD() {
    float delta_time = ImGui::GetIO().DeltaTime;
    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    float start_x = display_size.x - 20.0f;
    float start_y = display_size.y - 20.0f;
    float notif_width = 200.0f;
    float notif_height = 55.0f;
    float spacing = 10.0f;

    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(display_size, ImGuiCond_Always);
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoInputs;
    ImGui::Begin("HUDLayer", nullptr, window_flags);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    std::vector<HUDNotification*> active_notifs;
    std::vector<HUDNotification*> closing_notifs;
    for (auto& n : hud_notifications) {
        if (n.is_closing) closing_notifs.push_back(&n);
        else active_notifs.push_back(&n);
    }

    int active_count = (int)active_notifs.size();
    float total_height = active_count * (notif_height + spacing) - spacing;
    if (total_height < 0) total_height = 0;

    for (int i = 0; i < active_count; ++i) {
        HUDNotification* notif = active_notifs[i];
        float y = start_y - total_height + i * (notif_height + spacing);
        float x = start_x - notif_width;
        if (notif->progress < 1.0f) {
            notif->progress += delta_time * 5.0f;
            if (notif->progress > 1.0f) notif->progress = 1.0f;
        }
        else {
            notif->lifetime += delta_time;
            if (notif->lifetime >= notif->max_lifetime) {
                notif->is_closing = true;
                notif->exit_offset_y = 0.0f;
            }
        }
        float eased_progress = EaseOut(notif->progress);
        float x_offset = (1.0f - eased_progress) * (notif_width + 50.0f);
        ImVec2 pos(x - x_offset, y);
        ImVec2 size(notif_width, notif_height);
        ImU32 bg_color = IM_COL32(30, 30, 35, (int)(230 * eased_progress));
        draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg_color, 8.0f);
        draw_list->AddRectFilled(ImVec2(pos.x, pos.y + 8.0f), ImVec2(pos.x + 4.0f, pos.y + notif_height - 8.0f), notif->color, 2.0f);
        ImU32 text_color = IM_COL32(255, 255, 255, (int)(255 * eased_progress));
        draw_list->AddText(ImVec2(pos.x + 15.0f, pos.y + 6.0f), text_color, notif->text.c_str());
        if (!notif->subtext.empty()) {
            ImU32 sub_color = IM_COL32(180, 180, 180, (int)(200 * eased_progress));
            draw_list->AddText(ImVec2(pos.x + 15.0f, pos.y + 24.0f), sub_color, notif->subtext.c_str());
        }
        float fill_ratio = notif->lifetime / notif->max_lifetime;
        if (fill_ratio > 1.0f) fill_ratio = 1.0f;
        float bar_width = notif_width - 10.0f;
        float bar_height = 3.0f;
        float bar_x = pos.x + 5.0f;
        float bar_y = pos.y + notif_height - 6.0f;
        draw_list->AddRectFilled(ImVec2(bar_x, bar_y), ImVec2(bar_x + bar_width, bar_y + bar_height), IM_COL32(60, 60, 60, 180), 1.5f);
        if (fill_ratio > 0.0f) {
            float fill_width = bar_width * fill_ratio;
            ImU32 bar_color = IM_COL32((int)(100 - 60 * fill_ratio), 200, (int)(100 + 100 * fill_ratio), 255);
            draw_list->AddRectFilled(ImVec2(bar_x, bar_y), ImVec2(bar_x + fill_width, bar_y + bar_height), bar_color, 1.5f);
        }
    }

    std::vector<HUDNotification*> closing_to_remove;
    for (auto* notif : closing_notifs) {
        const float exit_speed = 3.0f;
        const float offset_speed = notif_height * 1.5f * exit_speed;

        notif->progress -= delta_time * exit_speed;
        notif->exit_offset_y -= delta_time * offset_speed;

        if (notif->progress <= 0.0f) {
            closing_to_remove.push_back(notif);
        }
        else {
            float y = start_y - total_height + notif->exit_offset_y;
            float x = start_x - notif_width;
            float eased_progress = EaseOut(notif->progress);
            float x_offset = (1.0f - eased_progress) * (notif_width + 50.0f);
            ImVec2 pos(x - x_offset, y);
            ImVec2 size(notif_width, notif_height);
            ImU32 bg_color = IM_COL32(30, 30, 35, (int)(230 * eased_progress));
            draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bg_color, 8.0f);
            draw_list->AddRectFilled(ImVec2(pos.x, pos.y + 8.0f), ImVec2(pos.x + 4.0f, pos.y + notif_height - 8.0f), notif->color, 2.0f);
            ImU32 text_color = IM_COL32(255, 255, 255, (int)(255 * eased_progress));
            draw_list->AddText(ImVec2(pos.x + 15.0f, pos.y + 6.0f), text_color, notif->text.c_str());
            if (!notif->subtext.empty()) {
                ImU32 sub_color = IM_COL32(180, 180, 180, (int)(200 * eased_progress));
                draw_list->AddText(ImVec2(pos.x + 15.0f, pos.y + 24.0f), sub_color, notif->subtext.c_str());
            }
            float fill_ratio = notif->lifetime / notif->max_lifetime;
            if (fill_ratio > 1.0f) fill_ratio = 1.0f;
            float bar_width = notif_width - 10.0f;
            float bar_height = 3.0f;
            float bar_x = pos.x + 5.0f;
            float bar_y = pos.y + notif_height - 6.0f;
            draw_list->AddRectFilled(ImVec2(bar_x, bar_y), ImVec2(bar_x + bar_width, bar_y + bar_height), IM_COL32(60, 60, 60, 180), 1.5f);
            if (fill_ratio > 0.0f) {
                float fill_width = bar_width * fill_ratio;
                ImU32 bar_color = IM_COL32((int)(100 - 60 * fill_ratio), 200, (int)(100 + 100 * fill_ratio), 255);
                draw_list->AddRectFilled(ImVec2(bar_x, bar_y), ImVec2(bar_x + fill_width, bar_y + bar_height), bar_color, 1.5f);
            }
        }
    }

    for (auto* notif : closing_to_remove) {
        auto it = std::find_if(hud_notifications.begin(), hud_notifications.end(),
            [notif](const HUDNotification& n) { return &n == notif; });
        if (it != hud_notifications.end()) hud_notifications.erase(it);
    }

    ImGui::End();
}

static void SyncLanguage() {
    Lang::SetLanguage(static_cast<Language>(g_MenuData.language));
}

static void HelpMarker(const char* desc) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

void SetupModernImGuiStyle() {
    static MenuData lastSettings;
    static bool firstRun = true;

    bool settingsChanged = false;
    if (firstRun) {
        settingsChanged = true;
        firstRun = false;
        lastSettings = g_MenuData;
    }
    else {
        if (lastSettings.WindowRound != g_MenuData.WindowRound ||
            lastSettings.WindowAlpha != g_MenuData.WindowAlpha ||
            lastSettings.FontScale != g_MenuData.FontScale ||
            lastSettings.ItemSpacing != g_MenuData.ItemSpacing ||
            lastSettings.FramePaddingX != g_MenuData.FramePaddingX ||
            lastSettings.FramePaddingY != g_MenuData.FramePaddingY ||
            lastSettings.AccentColor.x != g_MenuData.AccentColor.x ||
            lastSettings.AccentColor.y != g_MenuData.AccentColor.y ||
            lastSettings.AccentColor.z != g_MenuData.AccentColor.z) {
            settingsChanged = true;
            lastSettings = g_MenuData;
        }
    }

    if (!settingsChanged && g_styleInitialized) return;

    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO();
    ImVec4* colors = style.Colors;

    io.FontGlobalScale = g_MenuData.FontScale;

    style.WindowRounding = 14.0f;
    style.ChildRounding = 10.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 14.0f;
    style.WindowPadding = ImVec2(12, 12);
    style.ItemSpacing = ImVec2(8, 8);
    style.ItemInnerSpacing = ImVec2(4, 4);
    style.WindowBorderSize = 0.f;
    style.ChildBorderSize = 0.f;
    style.FrameBorderSize = 0.f;

    colors[ImGuiCol_Text] = ImVec4(0.60f, 0.20f, 0.40f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.70f, 0.40f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.85f, 0.90f, g_MenuData.WindowAlpha);
    colors[ImGuiCol_ChildBg] = ImVec4(0.98f, 0.90f, 0.95f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.98f, 0.90f, 0.95f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 0.95f, 0.98f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(1.00f, 0.90f, 0.95f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(1.00f, 0.85f, 0.90f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.90f, 0.50f, 0.70f, 0.75f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.95f, 0.60f, 0.80f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.40f, 0.60f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.90f, 0.50f, 0.70f, 0.70f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.85f, 0.45f, 0.65f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.90f, 0.50f, 0.70f, 0.50f);
    colors[ImGuiCol_Header] = ImVec4(0.90f, 0.50f, 0.70f, 0.30f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.95f, 0.60f, 0.80f, 0.40f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.80f, 0.40f, 0.60f, 0.50f);
    colors[ImGuiCol_Separator] = ImVec4(0.80f, 0.60f, 0.70f, 0.50f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.98f, 0.90f, 0.95f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.60f, 0.70f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.90f, 0.50f, 0.70f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.80f, 0.40f, 0.60f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.50f, 0.70f, 1.00f);
    style.FramePadding.x = g_MenuData.FramePaddingX;
    style.FramePadding.y = g_MenuData.FramePaddingY;
    style.ItemSpacing.x = g_MenuData.ItemSpacing;
    style.ItemSpacing.y = g_MenuData.ItemSpacing;

    g_styleInitialized = true;
}

static void DisplayMultilineText(const std::string& text, const std::string& emptyMsg, float maxHeight = 200.0f) {
    if (text.empty()) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", emptyMsg.c_str());
        return;
    }
    static std::unordered_map<size_t, float> heightCache;
    size_t hash = std::hash<std::string>{}(text);
    float height;
    auto it = heightCache.find(hash);
    if (it != heightCache.end()) {
        height = it->second;
    }
    else {
        int lineCount = 1;
        for (char c : text) if (c == '\n') lineCount++;
        height = (std::min)((std::max)(60.0f, static_cast<float>(lineCount * 18 + 10)), maxHeight);
        heightCache[hash] = height;
        if (heightCache.size() > 100) heightCache.clear();
    }
    ImGui::InputTextMultiline("##Display",
        const_cast<char*>(text.c_str()),
        text.size() + 1,
        ImVec2(-1, height),
        ImGuiInputTextFlags_ReadOnly);
}

static void SliderWithButtons(const char* label, float* value, float min, float max, float step, const char* format = "%.1f") {
    ImGui::SliderFloat(label, value, min, max, format);
    ImGui::SameLine();
    ImGui::PushID(label);
    if (ImGui::SmallButton("+")) {
        *value = (*value + step > max) ? max : *value + step;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("-")) {
        *value = (*value - step < min) ? min : *value - step;
    }
    ImGui::PopID();
}

static void SliderWithButtonsInt(const char* label, int* value, int min, int max, int step) {
    ImGui::SliderInt(label, value, min, max);
    ImGui::SameLine();
    ImGui::PushID(label);
    if (ImGui::SmallButton("+")) {
        *value = (*value + step > max) ? max : *value + step;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("-")) {
        *value = (*value - step < min) ? min : *value - step;
    }
    ImGui::PopID();
}

extern void RefreshRemoteData();

void RenderMainMenu() {
    SyncLanguage();
    SetupModernImGuiStyle();

    static int selected_tab = 0;

    ImGui::SetNextWindowSize(ImVec2(800, 500), ImGuiCond_Always);

    ImGui::Begin(TR("Sero Menu - Press F4 to show/hide cheat menu ", "Sero 菜单 - 按 F4 显示/隐藏"), nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize);

    ImGui::BeginChild("##tabs", ImVec2(130, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        if (ImGui::Button(TR("Home", "首页"), ImVec2(-1, 36))) selected_tab = 0;
        if (ImGui::Button(TR("ESP", "透视"), ImVec2(-1, 36))) selected_tab = 1;
        if (ImGui::Button(TR("Aim", "自瞄"), ImVec2(-1, 36))) selected_tab = 2;
        if (ImGui::Button(TR("Other", "其他"), ImVec2(-1, 36))) selected_tab = 3;
        if (ImGui::Button(TR("Performance", "性能"), ImVec2(-1, 36))) selected_tab = 4;
        if (ImGui::Button(TR("Settings", "设置"), ImVec2(-1, 36))) selected_tab = 5;
        if (ImGui::Button(TR("About", "关于"), ImVec2(-1, 36))) selected_tab = 6;
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##panel", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    {
        ImGui::Indent(20.0f);
        ImGui::Dummy(ImVec2(0, 10.0f));

        switch (selected_tab) {
        case 0: { // Home
            if (ImGui::Button(TR("Refresh", "刷新"))) {
                RefreshRemoteData();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                TR("(Click to fetch latest info)", "(点击获取最新信息)"));

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.60f, 0.20f, 0.40f, 1.00f), TR("Version Info", "版本信息"));
            ImGui::Separator();
            DisplayMultilineText(
                g_MenuData.versionInfo,
                TR("Click 'Refresh' to load version info", "点击「刷新」加载版本信息")
            );

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.60f, 0.20f, 0.40f, 1.00f), TR("Announcement", "公告"));
            ImGui::Separator();
            DisplayMultilineText(
                g_MenuData.announcement,
                TR("Click 'Refresh' to load announcement", "点击「刷新」加载公告")
            );
            break;
        }
        case 1: { // ESP
            ToggleSwitch("##MainESP", &g_MenuData.bMainESP, TR("ESP Master Switch", "透视总开关"));
            ImGui::Spacing();

            ImGui::BeginDisabled(!g_MenuData.bMainESP);

            ToggleSwitch("##BoxESP", &g_MenuData.bBoxESP, TR("Box ESP", "方框透视"));
            ToggleSwitch("##HealthESP", &g_MenuData.bHealthESP, TR("Health ESP", "血量透视"));
            ToggleSwitch("##RayESP", &g_MenuData.bRayESP, TR("Ray ESP", "射线透视"));
            ToggleSwitch("##WeaponESP", &g_MenuData.bWeaponESP, TR("Weapon ESP", "武器透视"));
            ToggleSwitch("##ArmorESP", &g_MenuData.bArmorESP, TR("Armor ESP", "护甲透视"));
            ToggleSwitch("##DistanceESP", &g_MenuData.bDistanceESP, TR("Distance ESP", "距离透视"));

            ImGui::Spacing();
            ToggleSwitch("##C4ESP", &g_MenuData.bC4ESP, TR("C4 ESP", "C4 透视"));

            // ★ 投掷物透视（带子选项）
            ToggleSwitch("##GrenadeESP", &g_MenuData.bGrenadeESP, TR("Grenade ESP", "投掷物透视"));
            ImGui::BeginDisabled(!g_MenuData.bGrenadeESP);
            ImGui::Indent(20.0f);
            ToggleSwitch("##GrenadeCarried", &g_MenuData.bGrenadeCarried, TR("Show Carried", "绘制携带"));
            ToggleSwitch("##GrenadeThrown", &g_MenuData.bGrenadeThrown, TR("Show Thrown", "绘制丢出"));
            ImGui::Unindent(20.0f);
            ImGui::EndDisabled();

            ToggleSwitch("##RemoveFriends", &g_MenuData.bRemoveFriends, TR("Remove friends", "排除队友"));

            ImGui::EndDisabled();
            break;
        }
        case 2: { // Aim
            ToggleSwitch("##AutoAim", &g_MenuData.bAutoAim, TR("Auto Aim", "自动瞄准"));
            ImGui::Spacing();

            ImGui::BeginDisabled(!g_MenuData.bAutoAim);
            ImGui::PushItemWidth(200.0f);
            SliderWithButtons(TR("Aim Smoothness", "瞄准平滑度"), &g_MenuData.fAimSmooth, 0.1f, 10.0f, 0.1f, "%.1f");
            SliderWithButtons(TR("Aim Speed", "瞄准速度"), &g_MenuData.fAimSpeed, 0.1f, 5.0f, 0.1f, "%.1f");
            ImGui::PopItemWidth();
            ImGui::EndDisabled();

            ImGui::Spacing();
            ToggleSwitch("##SilentAim", &g_MenuData.bSilentAim, TR("Silence Aim", "静默瞄准"));

            static const char* aimKeys[] = {
                "Ctrl", "Shift", "W", "Q", "E", "R",
                TR("Left Click", "鼠标左键"), TR("Right Click", "鼠标右键"),
                TR("Side Button 1", "侧键1"), TR("Side Button 2", "侧键2"),
                TR("Middle Click", "鼠标中键"),
                "Z", "X", "C", "CapsLock"
            };
            ImGui::Combo(TR("Aim Key", "自瞄按键"), &g_MenuData.aimKey, aimKeys, IM_ARRAYSIZE(aimKeys));

            ToggleSwitch("##VisibleAim", &g_MenuData.bVisibleAim, TR("Visible aiming", "仅瞄准可见目标"));
            ToggleSwitch("##AimFriends", &g_MenuData.bAimFriends, TR("Aim friends", "瞄准队友"));

            ImGui::Separator();
            ImGui::Spacing();

            ToggleSwitch("##AimScope", &g_MenuData.bAimScope, TR("Aim Scope", "自瞄范围"));
            ImGui::PushItemWidth(200.0f);
            SliderWithButtons(TR("Aim FOV", "自瞄视野"), &g_MenuData.fAimFOV, 1.0f, 60.0f, 0.1f, "%.1f deg");

            ImGui::BeginDisabled(!g_MenuData.bAutoAim);
            ToggleSwitch("##ShowAimLock", &g_MenuData.bShowAimLock, TR("Currently locked", "显示自瞄锁定线"));
            ImGui::EndDisabled();
            break;
        }
        case 3: { // Other
            ToggleSwitch("##AntiFlash", &g_MenuData.bAntiFlash, TR("Anti-Flash", "防闪光"));
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ToggleSwitch("##FOVChanger", &g_MenuData.bFOVChanger, TR("Custom FOV", "自定义视野"));
            ImGui::BeginDisabled(!g_MenuData.bFOVChanger);
            ImGui::PushItemWidth(200.0f);
            SliderWithButtons(TR("FOV Value", "视野值"), &g_MenuData.fFOVValue, 60.0f, 120.0f, 1.0f, "%.0f");
            ImGui::PopItemWidth();
            ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ToggleSwitch("##ThirdPerson", &g_MenuData.bThirdPerson, TR("Third Person", "第三人称"));
            ImGui::BeginDisabled(!g_MenuData.bThirdPerson);
            ImGui::PushItemWidth(200.0f);
            SliderWithButtons(TR("Third Person Distance", "第三人称距离"),
                &g_MenuData.fThirdPersonDist, 50.0f, 500.0f, 10.0f, "%.0f");
            const char* thirdPersonStyles[] = {
                TR("Toggle", "切换"),
                TR("Hold", "按住"),
                TR("Always", "始终")
            };
            ImGui::Combo(TR("Third Person Mode", "第三人称模式"),
                &g_MenuData.thirdPersonStyle, thirdPersonStyles, IM_ARRAYSIZE(thirdPersonStyles));
            static const char* thirdPersonKeys[] = {
                "F5", "F6", "F7", "F8", "F9",
                "Ctrl", "Shift", "V", "B", "N", "CapsLock"
            };
            ImGui::BeginDisabled(g_MenuData.thirdPersonStyle == 2);
            ImGui::Combo(TR("Third Person Key", "第三人称按键"),
                &g_MenuData.thirdPersonKey, thirdPersonKeys, IM_ARRAYSIZE(thirdPersonKeys));
            ImGui::EndDisabled();
            ToggleSwitch("##ThirdPersonNoInterp", &g_MenuData.bThirdPersonNoInterp,
                TR("Disable Transition", "禁用过渡动画"));
            ImGui::PopItemWidth();
            ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ToggleSwitch("##GlowESP", &g_MenuData.bGlowESP, TR("Glow ESP", "玩家热能"));
            ImGui::BeginDisabled(!g_MenuData.bGlowESP);
            ImGui::Indent(20.0f);

            ToggleSwitch("##GlowSeparate", &g_MenuData.bGlowSeparateColors, TR("Separate Visible/Invisible", "区分可见"));

            if (g_MenuData.bGlowSeparateColors) {
                ImGui::ColorEdit4(TR("Visible Color", "可视颜色"), &g_MenuData.GlowColorVisible.x, ImGuiColorEditFlags_NoInputs);
                ImGui::ColorEdit4(TR("Invisible Color", "不可视颜色"), &g_MenuData.GlowColorInvisible.x, ImGuiColorEditFlags_NoInputs);
            }
            else {
                ImGui::ColorEdit4(TR("Glow Color", "发光颜色"), &g_MenuData.GlowColor.x, ImGuiColorEditFlags_NoInputs);
            }

            ImGui::Unindent(20.0f);
            ImGui::EndDisabled();
            break;
        }
        case 4: { // Performance
            ImGui::TextColored(g_MenuData.AccentColor, TR("Update Settings", "更新设置"));
            ImGui::Separator();
            ImGui::PushItemWidth(200.0f);
            SliderWithButtonsInt(TR("Update Interval (frames)", "更新间隔（帧数）"),
                &g_MenuData.performance.updateInterval, 1, 10, 1);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                TR("(Higher = less CPU)", "(越高CPU占用越低)"));

            ToggleSwitch("##DynamicUpdate", &g_MenuData.performance.enableDynamicUpdate, TR("Dynamic Update", "动态更新"));
            HelpMarker(TR(
                "When enabled, updates happen more frequently when interacting with the menu.\n\n"
                "This ensures smoother UI response while saving CPU when menu is idle.",
                "启用后，在菜单交互时会更频繁地更新数据。\n\n"
                "确保UI响应流畅，同时在菜单闲置时节省CPU资源。"
            ));

            ImGui::Spacing();
            ImGui::TextColored(g_MenuData.AccentColor, TR("Distance Filter", "距离过滤"));
            ImGui::Separator();
            ImGui::PushItemWidth(200.0f);
            SliderWithButtons(TR("Max Draw Distance", "最大绘制距离"),
                &g_MenuData.performance.maxDrawDistance, 1000.0f, 15000.0f, 100.0f, "%.0f");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                TR("(Lower = faster)", "(越低越快)"));
            ImGui::PushItemWidth(200.0f);
            SliderWithButtons(TR("C4 Max Distance", "C4最大距离"),
                &g_MenuData.performance.c4MaxDistance, 1000.0f, 10000.0f, 100.0f, "%.0f");

            ToggleSwitch("##EnableDistanceFilter", &g_MenuData.performance.enableDistanceFilter, TR("Enable Distance Filter", "启用距离过滤"));
            HelpMarker(TR(
                "Skips processing entities beyond the set distance limit.\n\n"
                "This significantly reduces CPU usage, especially in crowded areas.",
                "跳过处理超过设定距离限制的实体。\n\n"
                "这能显著降低CPU使用率，尤其是在人群密集区域。"
            ));

            ImGui::Spacing();
            ImGui::TextColored(g_MenuData.AccentColor, TR("Level of Detail (LOD)", "细节层次"));
            ImGui::Separator();

            ToggleSwitch("##EnableLOD", &g_MenuData.performance.enableLOD, TR("Enable LOD", "启用LOD"));
            HelpMarker(TR(
                "Reduces drawing details for far-away enemies.\n\n"
                "LOD1: Simplified drawing (fewer details)\n"
                "LOD2: Only shows the box ESP\n\n"
                "Saves GPU resources without losing crucial info.",
                "对远处的敌人减少绘制细节。\n\n"
                "LOD1: 简化绘制（减少细节）\n"
                "LOD2: 仅显示方框透视\n\n"
                "在保留关键信息的同时节省GPU资源。"
            ));
            ImGui::PushItemWidth(200.0f);
            SliderWithButtons(TR("LOD1 Distance", "LOD1距离"),
                &g_MenuData.performance.lodDistance1, 1000.0f, 8000.0f, 100.0f, "%.0f");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                TR("(Simplified draw)", "(简化绘制)"));
            ImGui::PushItemWidth(200.0f);
            SliderWithButtons(TR("LOD2 Distance", "LOD2距离"),
                &g_MenuData.performance.lodDistance2, 2000.0f, 12000.0f, 100.0f, "%.0f");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                TR("(Box only)", "(仅显示方框)"));
            ImGui::PushItemWidth(200.0f);
            ImGui::Spacing();
            ImGui::TextColored(g_MenuData.AccentColor, TR("Cache Settings", "缓存设置"));
            ImGui::Separator();

            ToggleSwitch("##EnableWeaponCache", &g_MenuData.performance.enableWeaponCache, TR("Enable Weapon Cache", "启用武器缓存"));
            HelpMarker(TR(
                "Caches weapon names so they don't need to be looked up every frame.\n\n"
                "This reduces string operations and memory reads,\n"
                "especially useful when many players are visible.",
                "缓存武器名称，避免每帧都去查找。\n\n"
                "减少字符串操作和内存读取，\n"
                "在可见玩家较多时特别有用。"
            ));

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::PushItemWidth(200.0f);
            ImGui::TextColored(g_MenuData.AccentColor, TR("Quick Presets", "快速预设"));
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                TR("Click to apply preset settings:", "点击应用预设配置："));
            ImGui::Spacing();
            ImGui::PushItemWidth(200.0f);
            if (ImGui::Button(TR("Performance Mode", "性能模式"), ImVec2(130, 0))) {
                g_MenuData.performance.updateInterval = 5;
                g_MenuData.performance.maxDrawDistance = 5000.0f;
                g_MenuData.performance.c4MaxDistance = 3000.0f;
                g_MenuData.performance.enableLOD = true;
                g_MenuData.performance.lodDistance1 = 2000.0f;
                g_MenuData.performance.lodDistance2 = 4000.0f;
                g_MenuData.performance.enableDistanceFilter = true;
                g_MenuData.performance.enableDynamicUpdate = true;
                g_MenuData.performance.enableWeaponCache = true;
            }
            HelpMarker(TR(
                "Maximum performance for low-end PCs.\n\n"
                "Update: 5 frames\n"
                "Draw Distance: 5000\n"
                "C4 Distance: 3000\n"
                "LOD: Enabled\n\n"
                "Saves the most CPU/GPU resources.",
                "为低配电脑提供最高性能。\n\n"
                "更新间隔: 5帧\n"
                "绘制距离: 5000\n"
                "C4距离: 3000\n"
                "LOD: 启用\n\n"
                "最大化节省CPU/GPU资源。"
            ));

            ImGui::SameLine();
            ImGui::PushItemWidth(200.0f);
            if (ImGui::Button(TR("Balanced Mode", "均衡模式"), ImVec2(130, 0))) {
                g_MenuData.performance.updateInterval = 3;
                g_MenuData.performance.maxDrawDistance = 8000.0f;
                g_MenuData.performance.c4MaxDistance = 5000.0f;
                g_MenuData.performance.enableLOD = true;
                g_MenuData.performance.lodDistance1 = 3000.0f;
                g_MenuData.performance.lodDistance2 = 6000.0f;
                g_MenuData.performance.enableDistanceFilter = true;
                g_MenuData.performance.enableDynamicUpdate = true;
                g_MenuData.performance.enableWeaponCache = true;
            }
            HelpMarker(TR(
                "Best balance between performance and quality.\n\n"
                "Update: 3 frames\n"
                "Draw Distance: 8000\n"
                "C4 Distance: 5000\n"
                "LOD: Enabled\n\n"
                "Recommended for most users.",
                "性能和画质的最佳平衡。\n\n"
                "更新间隔: 3帧\n"
                "绘制距离: 8000\n"
                "C4距离: 5000\n"
                "LOD: 启用\n\n"
                "推荐大多数用户使用。"
            ));

            ImGui::SameLine();
            ImGui::PushItemWidth(200.0f);
            if (ImGui::Button(TR("Quality Mode", "质量模式"), ImVec2(130, 0))) {
                g_MenuData.performance.updateInterval = 1;
                g_MenuData.performance.maxDrawDistance = 12000.0f;
                g_MenuData.performance.c4MaxDistance = 8000.0f;
                g_MenuData.performance.enableLOD = false;
                g_MenuData.performance.enableDistanceFilter = false;
                g_MenuData.performance.enableDynamicUpdate = true;
                g_MenuData.performance.enableWeaponCache = true;
            }
            HelpMarker(TR(
                "Maximum visual quality for high-end PCs.\n\n"
                "Update: Every frame\n"
                "Draw Distance: 12000\n"
                "C4 Distance: 8000\n"
                "LOD: Disabled\n\n"
                "Requires powerful hardware.",
                "为高配电脑提供最高画质。\n\n"
                "更新间隔: 每帧\n"
                "绘制距离: 12000\n"
                "C4距离: 8000\n"
                "LOD: 禁用\n\n"
                "需要强劲的硬件支持。"
            ));

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::TextColored(g_MenuData.AccentColor, TR("Performance Info", "性能信息"));
            ImGui::Separator();
            ImGui::PushItemWidth(200.0f);
            float currentFPS = ImGui::GetIO().Framerate;
            ImGui::Text(TR("Current FPS: %.1f", "当前FPS: %.1f"), currentFPS);

            if (currentFPS > 120) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Excellent");
            }
            else if (currentFPS > 60) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Good");
            }
            else if (currentFPS > 30) {
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: Acceptable");
            }
            else {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Status: Low - Consider Performance Mode");
            }

            ImGui::Spacing();
            ImGui::Text(TR("Current Update Interval: %d frames", "当前更新间隔: %d帧"),
                g_MenuData.performance.updateInterval);
            ImGui::Text(TR("Current Draw Distance: %.0f", "当前绘制距离: %.0f"),
                g_MenuData.performance.maxDrawDistance);
            ImGui::Text(TR("LOD Status: %s", "LOD状态: %s"),
                g_MenuData.performance.enableLOD ? TR("Enabled", "启用") : TR("Disabled", "禁用"));
            break;
        }
        case 5: { // Settings
            static const char* langItems[] = { "English", "中文" };
            if (ImGui::Combo(TR("Language", "语言"), &g_MenuData.language, langItems, IM_ARRAYSIZE(langItems))) {
                SyncLanguage();
            }
            ImGui::PushItemWidth(200.0f);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::ColorEdit4(TR("Aim Lock Color", "锁定线颜色"), &g_MenuData.AimLockColor.x, ImGuiColorEditFlags_NoInputs);

            SliderWithButtons(TR("Ray Thickness", "射线粗细"), &g_MenuData.RayThickness, 0.5f, 5.0f, 0.1f, "%.1f");

            ImGui::Spacing();
            ImGui::TextColored(g_MenuData.AccentColor, TR("Visible / Invisible Colors", "可见 / 不可见颜色"));
            ImGui::Separator();
            ImGui::ColorEdit4(TR("Box Visible", "方框-可见"), &g_MenuData.BoxColorVisible.x, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4(TR("Box Invisible", "方框-不可见"), &g_MenuData.BoxColorInvisible.x, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4(TR("Ray Visible", "射线-可见"), &g_MenuData.RayColorVisible.x, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4(TR("Ray Invisible", "射线-不可见"), &g_MenuData.RayColorInvisible.x, ImGuiColorEditFlags_NoInputs);
            ImGui::PushItemWidth(200.0f);
            ImGui::Spacing();
            ImGui::TextColored(g_MenuData.AccentColor, TR("Window Appearance", "窗口外观"));
            ImGui::Separator();
            ImGui::PushItemWidth(200.0f);
            SliderWithButtons(TR("Window Transparency", "窗口透明度"), &g_MenuData.WindowAlpha, 0.4f, 1.0f, 0.1f, "%.2f");
            SliderWithButtons(TR("UI Corner Radius", "界面圆角"), &g_MenuData.WindowRound, 0.0f, 14.0f, 0.1f, "%.0f");
            SliderWithButtons(TR("Global Font Scale", "全局字体缩放"), &g_MenuData.FontScale, 0.7f, 1.4f, 0.1f, "%.2f");
            ImGui::PushItemWidth(200.0f);
            ImGui::Spacing();
            ImGui::TextColored(g_MenuData.AccentColor, TR("Layout Spacing", "布局间距"));
            ImGui::Separator();
            ImGui::PushItemWidth(200.0f);
            SliderWithButtons(TR("Item Spacing", "元素间距"), &g_MenuData.ItemSpacing, 4.0f, 12.0f, 0.1f, "%.0f");
            SliderWithButtons(TR("Frame Padding X", "框架水平内边距"), &g_MenuData.FramePaddingX, 3.0f, 10.0f, 0.1f, "%.0f");
            SliderWithButtons(TR("Frame Padding Y", "框架垂直内边距"), &g_MenuData.FramePaddingY, 3.0f, 10.0f, 0.1f, "%.0f");

            // ★ 调试选项
            ImGui::Spacing();
            ImGui::TextColored(g_MenuData.AccentColor, TR("Debug Options", "调试选项"));
            ImGui::Separator();
            ToggleSwitch("##GrenadeDebug", &g_MenuData.bGrenadeDebug,
                TR("Debug Info", "调试信息"));
            break;
        }
        case 6: { // About
            ImGui::Text(TR("Copyright @Qingning ", "版权所有 @Qingning "));
            ImGui::Text(TR("List of Acknowledgments", "致谢名单"));
            ImGui::Text(TR("Production Credits", "制作者名单"));
            ImGui::Text(TR("qwit", "黎明"));
            ImGui::Text(TR("Qingning", "布丁小熊"));
            ImGui::Text("__invoker (Bilibili UID@174466535)");
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), TR("Our official communication QQ group", "官方交流QQ群"));
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), TR("One:1041141006", "一群:1041141006"));
            break;
        }
        }

        ImGui::Unindent(20.0f);
    }
    ImGui::EndChild();

    ImGui::End();
}