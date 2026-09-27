#pragma once
#include "..\imgui_d11\imgui.h"
#include "theme.h"

namespace Fx {

    void DrawDotGrid(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 dotColor, float spacing = 14.0f);

    class BeamRain {
    public:
        void Update(float dt);
        void Draw(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color);
    private:
        struct Beam { float x, y, len, speed, life, maxLife; bool hit; };
        struct Splash {
            float x, y, life, maxLife;
            ImVec2 pv[10];
            ImVec2 pp[10];
            float  ps[10];
            float  pk[10];
            int    pn;
            bool   alive;
        };
        static const int kMax = 9;
        static const int kSplash = 16;
        Beam beams[kMax] = {};
        Splash splashes[kSplash] = {};
        void Respawn(Beam& b, ImVec2 min, ImVec2 max);
        void SpawnSplash(float x, float y);
        bool inited = false;
    };

    class ParticleBurst {
    public:
        void Emit(ImVec2 origin, ImU32 color, int count = 40);
        void Update(float dt);
        void Draw(ImDrawList* dl);
        bool Active() const { return aliveCount > 0; }
    private:
        struct P { ImVec2 pos, vel; float life, maxLife, size; ImU32 col; bool alive; };
        static const int kMax = 200;
        P parts[kMax] = {};
        int aliveCount = 0;
    };

} // namespace Fx