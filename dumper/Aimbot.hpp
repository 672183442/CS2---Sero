#pragma once
#include "PlayerHelper.hpp"
#include "qangle.h"

class AimBot {

public:
    static  QAngle_t GetTargetAngle(uint64_t playerPtr , uint64_t localPlayer ) {
        auto targetEye = CS2Helper::getOldOrigin<Vector_t>(playerPtr) + CS2Helper::getViewOffset<Vector_t>(playerPtr);
        auto localEye = CS2Helper::getOldOrigin<Vector_t>(localPlayer) + CS2Helper::getViewOffset<Vector_t>(localPlayer);

        auto angle = Vector_t::CalculateViewAngle(localEye , targetEye);
        angle.Normalize();
        auto punch = CS2Helper::getAimPunchAngle<QAngle_t>(localPlayer);
        return angle - (punch * 2);
    }
};