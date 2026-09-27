#include "menu.h"
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>

MenuData g_MenuData;

// ============================================================
// HUD 通知
// ============================================================
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

static float EaseOut(float t) {
    return 1.0f - (1.0f - t) * (1.0f - t);
}

void AddHUDNotification(const char* text, const char* subtext, ImU32 color) {
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
    notif.text = text ? text : "";
    notif.subtext = subtext ? subtext : "";
    notif.color = color;
    hud_notifications.push_back(notif);
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
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoFocusOnAppearing |
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