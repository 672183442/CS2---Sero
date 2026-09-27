#pragma once
#include <windows.h>
#include <cstdint>
#include "output/offsets.hpp"
#include "output/client_dll.hpp"

//帮助类
class CS2Helper {
    uint64_t clientBase;
    uint64_t entityListPtr;
    uint64_t entityList;
    uint64_t entry;


public:
    CS2Helper() {
         clientBase = (uint64_t)GetModuleHandle("client.dll");
         entityListPtr = (clientBase + cs2_dumper::offsets::client_dll::dwEntityList);
         entityList = *((uint64_t*)entityListPtr);
         entry = *(uint64_t*)(entityList + 0x10);
    }

    static CS2Helper & Instance() {
        static CS2Helper ins;
        return ins;
    }

    uint64_t getPlayer(int i) const {

        auto controller = *(uint64_t*)(entry + i * 0x78);

        if (!controller) {
            return 0;
        }

        auto playerPawnHandle = *(uint64_t*)(controller + cs2_dumper::schemas::client_dll::CCSPlayerController::m_hPlayerPawn);

        if (!playerPawnHandle) {
            return 0;
        }

        auto entry_2 = *(uint64_t*)(entityList + 0x8 * ((playerPawnHandle & 0x7fff) >> 9) + 0x10);

        auto current = *(uint64_t*)((entry_2 + 0x78 * (playerPawnHandle & 0x1ff)));

        return current;
    }

    uint64_t getLocalPlayerPawn() const {
        return *(uint64_t*)(clientBase + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
    }

    template<typename T>
    void writeViewAngle(const T & newAngle) const {
        static_assert(sizeof(T) >= 8);
        static auto ptr = clientBase + cs2_dumper::offsets::client_dll::dwViewAngles;
        memcpy((void *)ptr , &newAngle , sizeof(newAngle));
    }

    template<typename T>
    T getViewAngle()const {
        static_assert(sizeof(T) >=8);
        static auto ptr = clientBase + cs2_dumper::offsets::client_dll::dwViewAngles;
        return *(T*)ptr;
    }

    static int8_t getTeamNum(uint64_t targetPtr){
        return *(int8_t *)(targetPtr + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
    }

    static int8_t getLifeState(uint64_t targetPtr) {
        return *(int8_t *)(targetPtr + cs2_dumper::schemas::client_dll::C_BaseEntity::m_lifeState);
    }

    template<typename Vec3>
    static Vec3 getOldOrigin(uint64_t targetPtr) {
        static_assert(sizeof(Vec3) == 4 * 3);
        return *(Vec3*) (targetPtr + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
    }

    //C_BaseModelEntity
   template<typename Vec3>
   static Vec3 getViewOffset(uint64_t targetPtr) {
        static_assert(sizeof(Vec3) == 4 * 3);
        return *(Vec3*) (targetPtr + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset);
   }
   template<typename Vec3>
   static Vec3 getAimPunchAngle(uint64_t targetPtr) {
        static_assert(sizeof(Vec3) == 4 * 3);
        return *(Vec3*) (targetPtr + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_aimPunchAngle);
    }

};