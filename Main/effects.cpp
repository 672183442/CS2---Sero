#include "effects.h"
#include <cstdlib>
#include <cmath>

namespace Fx {

    static float Rnd01() { return (float)rand() / (float)RAND_MAX; }
    static float RndRange(float a, float b) { return a + (b - a) * Rnd01(); }

    void DrawDotGrid(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 dotColor, float spacing) {
        dl->PushClipRect(min, max, true);
        for (float y = min.y + spacing * 0.5f; y < max.y; y += spacing)
            for (float x = min.x + spacing * 0.5f; x < max.x; x += spacing)
                dl->AddRectFilled(ImVec2(x - 0.6f, y - 0.6f), ImVec2(x + 0.6f, y + 0.6f), dotColor);
        dl->PopClipRect();
    }

    void BeamRain::Respawn(Beam& b, ImVec2 min, ImVec2 max) {
        b.x = RndRange(min.x + 12, max.x - 12);
        b.len = RndRange(50, 140);
        b.y = min.y - b.len - RndRange(0, 400);
        b.speed = RndRange(110, 210);
        b.maxLife = 1.0f;
        b.life = 1.0f;
        b.hit = false;
    }

    void BeamRain::SpawnSplash(float x, float y) {
        int idx = -1;
        for (int i = 0; i < kSplash; i++) if (!splashes[i].alive) { idx = i; break; }
        if (idx < 0) return;
        Splash& s = splashes[idx];
        s.x = x; s.y = y;
        s.maxLife = 0.8f; s.life = s.maxLife;
        s.alive = true;
        s.pn = 7 + (rand() % 3);
        for (int i = 0; i < s.pn; i++) {
            float ang = -1.5708f + RndRange(-1.0f, 1.0f);
            float spd = RndRange(40, 130);
            s.pv[i] = ImVec2(cosf(ang) * spd, sinf(ang) * spd);
            s.pp[i] = ImVec2(x + RndRange(-2, 2), y - RndRange(0, 4));
            s.ps[i] = RndRange(1.3f, 3.4f);
            s.pk[i] = RndRange(0.6f, 1.0f);
        }
    }

    void BeamRain::Update(float dt) {
        for (int i = 0; i < kMax; i++) beams[i].y += beams[i].speed * dt;
        for (int i = 0; i < kSplash; i++) {
            if (!splashes[i].alive) continue;
            Splash& s = splashes[i];
            s.life -= dt;
            for (int k = 0; k < s.pn; k++) {
                s.pv[k].y += 55.0f * dt;
                s.pv[k].x *= (1.0f - 1.4f * dt);
                s.pv[k].y *= (1.0f - 1.4f * dt);
                s.pp[k].x += s.pv[k].x * dt;
                s.pp[k].y += s.pv[k].y * dt;
            }
            if (s.life <= 0.0f) s.alive = false;
        }
    }

    void BeamRain::Draw(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color) {
        if (!inited) { for (int i = 0; i < kMax; i++) Respawn(beams[i], min, max); inited = true; }
        float floorY = max.y - 3.0f;
        dl->PushClipRect(min, max, true);
        ImVec4 base = ImGui::ColorConvertU32ToFloat4(color);

        for (int i = 0; i < kMax; i++) {
            Beam& b = beams[i];
            if (!b.hit && b.y >= floorY) { b.hit = true; SpawnSplash(b.x, floorY); }
            if (b.y - b.len > floorY) { Respawn(b, min, max); continue; }
            float fade = 1.0f;
            if (b.y < min.y + 40.0f) fade = (b.y - (min.y - b.len)) / (b.len + 40.0f);
            if (fade < 0) fade = 0; if (fade > 1) fade = 1;
            float y0 = (b.y < floorY) ? b.y : floorY;
            float y1 = b.y - b.len;
            const int seg = 10;
            for (int s = 0; s < seg; s++) {
                float t0 = (float)s / seg, t1 = (float)(s + 1) / seg;
                ImVec2 p0(b.x, y1 + (y0 - y1) * t0);
                ImVec2 p1(b.x, y1 + (y0 - y1) * t1);
                ImVec4 cs = base; cs.w = (0.08f + 0.6f * t0) * fade;
                dl->AddLine(p0, p1, ImGui::ColorConvertFloat4ToU32(cs), 1.5f);
            }
        }

        for (int i = 0; i < kSplash; i++) {
            if (!splashes[i].alive) continue;
            Splash& s = splashes[i];
            float k = s.life / s.maxLife;
            float age = 1.0f - k;
            for (int g = 3; g >= 1; g--) {
                float hw = (7.0f + age * 12.0f) * (0.6f + 0.4f * g);
                float hh = 2.2f * g;
                ImVec4 c = base; c.w = 0.20f * k / (float)g;
                dl->AddRectFilled(ImVec2(s.x - hw, s.y - hh), ImVec2(s.x + hw, s.y + hh),
                    ImGui::ColorConvertFloat4ToU32(c), hh);
            }
            for (int p = 0; p < s.pn; p++) {
                float pk = k * s.pk[p];
                if (pk <= 0.0f) continue;
                ImVec4 pc = base; pc.w = pk;
                float sz = s.ps[p] * (0.5f + 0.5f * pk);
                dl->AddRectFilled(ImVec2(s.pp[p].x - sz, s.pp[p].y - sz),
                    ImVec2(s.pp[p].x + sz, s.pp[p].y + sz),
                    ImGui::ColorConvertFloat4ToU32(pc), 0.8f);
            }
        }
        dl->PopClipRect();
    }

    void ParticleBurst::Emit(ImVec2 origin, ImU32 color, int count) {
        for (int n = 0; n < count && aliveCount < kMax; n++) {
            int idx = -1;
            for (int i = 0; i < kMax; i++) if (!parts[i].alive) { idx = i; break; }
            if (idx < 0) break;
            P& p = parts[idx];
            float ang = RndRange(0.0f, 6.2831853f);
            float spd = RndRange(60, 260);
            p.pos = origin;
            p.vel = ImVec2(cosf(ang) * spd, sinf(ang) * spd);
            p.maxLife = RndRange(0.4f, 0.9f);
            p.life = p.maxLife;
            p.size = RndRange(2.0f, 5.0f);
            p.col = color;
            p.alive = true;
            aliveCount++;
        }
    }

    void ParticleBurst::Update(float dt) {
        for (int i = 0; i < kMax; i++) {
            if (!parts[i].alive) continue;
            P& p = parts[i];
            p.vel.x *= (1.0f - 3.0f * dt);
            p.vel.y = p.vel.y * (1.0f - 3.0f * dt) + 120.0f * dt;
            p.pos.x += p.vel.x * dt;
            p.pos.y += p.vel.y * dt;
            p.life -= dt;
            if (p.life <= 0.0f) { p.alive = false; aliveCount--; }
        }
    }

    void ParticleBurst::Draw(ImDrawList* dl) {
        for (int i = 0; i < kMax; i++) {
            if (!parts[i].alive) continue;
            P& p = parts[i];
            float k = p.life / p.maxLife;
            ImVec4 col = ImGui::ColorConvertU32ToFloat4(p.col);
            col.w = k;
            float s = p.size * (0.5f + 0.5f * k);
            dl->AddRectFilled(ImVec2(p.pos.x - s, p.pos.y - s),
                ImVec2(p.pos.x + s, p.pos.y + s),
                ImGui::ColorConvertFloat4ToU32(col), 1.5f);
        }
    }

} // namespace Fx