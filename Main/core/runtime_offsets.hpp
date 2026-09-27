#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstdint>

namespace cs2_runtime {
    namespace offsets {
        namespace client_dll {
            inline std::ptrdiff_t dwCSGOInput = 0;
            inline std::ptrdiff_t dwEntityList = 0;
            inline std::ptrdiff_t dwGameEntitySystem = 0;
            inline std::ptrdiff_t dwGameEntitySystem_highestEntityIndex = 0;
            inline std::ptrdiff_t dwGameRules = 0;
            inline std::ptrdiff_t dwGlobalVars = 0;
            inline std::ptrdiff_t dwGlowManager = 0;
            inline std::ptrdiff_t dwLocalPlayerController = 0;
            inline std::ptrdiff_t dwLocalPlayerPawn = 0;
            inline std::ptrdiff_t dwPlantedC4 = 0;
            inline std::ptrdiff_t dwPrediction = 0;
            inline std::ptrdiff_t dwSensitivity = 0;
            inline std::ptrdiff_t dwSensitivity_sensitivity = 0;
            inline std::ptrdiff_t dwViewAngles = 0;
            inline std::ptrdiff_t dwViewMatrix = 0;
            inline std::ptrdiff_t dwViewRender = 0;
            inline std::ptrdiff_t dwWeaponC4 = 0;
        }
        namespace engine2_dll {
            inline std::ptrdiff_t dwBuildNumber = 0;
        }
    }

    namespace schemas {
        namespace client_dll {
            #define CS2_RUNTIME_CLASS(name) namespace name {
            #define CS2_RUNTIME_FIELD(name) inline std::ptrdiff_t name = 0;
            CS2_RUNTIME_CLASS(C_AttributeContainer) CS2_RUNTIME_FIELD(m_Item) }
            CS2_RUNTIME_CLASS(C_BaseEntity)
                CS2_RUNTIME_FIELD(m_hOwnerEntity) CS2_RUNTIME_FIELD(m_iHealth)
                CS2_RUNTIME_FIELD(m_iTeamNum) CS2_RUNTIME_FIELD(m_lifeState)
                CS2_RUNTIME_FIELD(m_pGameSceneNode) }
            CS2_RUNTIME_CLASS(C_BaseModelEntity)
                CS2_RUNTIME_FIELD(m_Glow) CS2_RUNTIME_FIELD(m_vecViewOffset) }
            CS2_RUNTIME_CLASS(C_BasePlayerPawn)
                CS2_RUNTIME_FIELD(m_hController) CS2_RUNTIME_FIELD(m_pCameraServices)
                CS2_RUNTIME_FIELD(m_pMovementServices) CS2_RUNTIME_FIELD(m_pWeaponServices)
                CS2_RUNTIME_FIELD(m_vOldOrigin) }
            CS2_RUNTIME_CLASS(C_C4) CS2_RUNTIME_FIELD(m_bBombPlanted) }
            CS2_RUNTIME_CLASS(C_CSPlayerPawn)
                CS2_RUNTIME_FIELD(m_ArmorValue) CS2_RUNTIME_FIELD(m_angEyeAngles)
                CS2_RUNTIME_FIELD(m_entitySpottedState) CS2_RUNTIME_FIELD(m_pAimPunchServices) }
            CS2_RUNTIME_CLASS(C_CSPlayerPawnBase) CS2_RUNTIME_FIELD(m_flFlashDuration) }
            CS2_RUNTIME_CLASS(C_EconEntity) CS2_RUNTIME_FIELD(m_AttributeManager) }
            CS2_RUNTIME_CLASS(C_EconItemView) CS2_RUNTIME_FIELD(m_iItemDefinitionIndex) }
            CS2_RUNTIME_CLASS(C_PlantedC4)
                CS2_RUNTIME_FIELD(m_bBeingDefused) CS2_RUNTIME_FIELD(m_bBombDefused)
                CS2_RUNTIME_FIELD(m_bBombTicking) CS2_RUNTIME_FIELD(m_bHasExploded)
                CS2_RUNTIME_FIELD(m_nBombSite) }
            CS2_RUNTIME_CLASS(CBasePlayerController) CS2_RUNTIME_FIELD(m_hPawn) }
            CS2_RUNTIME_CLASS(CCSPlayer_AimPunchServices) CS2_RUNTIME_FIELD(m_predictableBaseAngle) }
            CS2_RUNTIME_CLASS(CCSPlayer_MovementServices) CS2_RUNTIME_FIELD(m_flDuckAmount) }
            CS2_RUNTIME_CLASS(CCSPlayerBase_CameraServices)
                CS2_RUNTIME_FIELD(m_iFOV) CS2_RUNTIME_FIELD(m_iFOVStart) }
            CS2_RUNTIME_CLASS(CGameSceneNode) CS2_RUNTIME_FIELD(m_vecAbsOrigin) }
            CS2_RUNTIME_CLASS(CGlowProperty)
                CS2_RUNTIME_FIELD(m_bEligibleForScreenHighlight) CS2_RUNTIME_FIELD(m_bFlashing)
                CS2_RUNTIME_FIELD(m_bGlowing) CS2_RUNTIME_FIELD(m_fGlowColor)
                CS2_RUNTIME_FIELD(m_glowColorOverride) CS2_RUNTIME_FIELD(m_iGlowTeam)
                CS2_RUNTIME_FIELD(m_iGlowType) CS2_RUNTIME_FIELD(m_nGlowRange)
                CS2_RUNTIME_FIELD(m_nGlowRangeMin) }
            CS2_RUNTIME_CLASS(CPlayer_WeaponServices) CS2_RUNTIME_FIELD(m_hActiveWeapon) }
            CS2_RUNTIME_CLASS(EntitySpottedState_t)
                CS2_RUNTIME_FIELD(m_bSpotted) CS2_RUNTIME_FIELD(m_bSpottedByMask) }
            #undef CS2_RUNTIME_FIELD
            #undef CS2_RUNTIME_CLASS
        }
    }

    namespace layouts {
        inline std::ptrdiff_t CCSGOInput_m_angViewAngles = 0;
        // CViewSetup is a non-schema layout in the supplied universal dumper.
        inline constexpr std::ptrdiff_t CViewSetup_m_vecOrigin = 0x4A0;
        inline constexpr std::ptrdiff_t CViewSetup_m_angViewAngles = 0x4B8;
    }

    bool Initialize();
    bool IsInitialized();
    std::uint32_t BuildNumber();
}

// Keep existing feature code source-compatible while the values come from the
// runtime resolver instead of generated constexpr headers.
namespace cs2_dumper {
    namespace offsets {
        namespace client_dll {
            inline auto& dwCSGOInput = cs2_runtime::offsets::client_dll::dwCSGOInput;
            inline auto& dwEntityList = cs2_runtime::offsets::client_dll::dwEntityList;
            inline auto& dwGameEntitySystem = cs2_runtime::offsets::client_dll::dwGameEntitySystem;
            inline auto& dwGameEntitySystem_highestEntityIndex = cs2_runtime::offsets::client_dll::dwGameEntitySystem_highestEntityIndex;
            inline auto& dwGameRules = cs2_runtime::offsets::client_dll::dwGameRules;
            inline auto& dwGlobalVars = cs2_runtime::offsets::client_dll::dwGlobalVars;
            inline auto& dwGlowManager = cs2_runtime::offsets::client_dll::dwGlowManager;
            inline auto& dwLocalPlayerController = cs2_runtime::offsets::client_dll::dwLocalPlayerController;
            inline auto& dwLocalPlayerPawn = cs2_runtime::offsets::client_dll::dwLocalPlayerPawn;
            inline auto& dwPlantedC4 = cs2_runtime::offsets::client_dll::dwPlantedC4;
            inline auto& dwPrediction = cs2_runtime::offsets::client_dll::dwPrediction;
            inline auto& dwSensitivity = cs2_runtime::offsets::client_dll::dwSensitivity;
            inline auto& dwSensitivity_sensitivity = cs2_runtime::offsets::client_dll::dwSensitivity_sensitivity;
            inline auto& dwViewAngles = cs2_runtime::offsets::client_dll::dwViewAngles;
            inline auto& dwViewMatrix = cs2_runtime::offsets::client_dll::dwViewMatrix;
            inline auto& dwViewRender = cs2_runtime::offsets::client_dll::dwViewRender;
            inline auto& dwWeaponC4 = cs2_runtime::offsets::client_dll::dwWeaponC4;
        }
        namespace engine2_dll {
            inline auto& dwBuildNumber = cs2_runtime::offsets::engine2_dll::dwBuildNumber;
        }
    }

    namespace schemas {
        namespace client_dll {
            #define CS2_RUNTIME_ALIAS(className, fieldName) \
                inline auto& fieldName = cs2_runtime::schemas::client_dll::className::fieldName;
            #define CS2_RUNTIME_ALIAS_CLASS(className, fields) namespace className { fields }
            CS2_RUNTIME_ALIAS_CLASS(C_AttributeContainer, CS2_RUNTIME_ALIAS(C_AttributeContainer, m_Item))
            CS2_RUNTIME_ALIAS_CLASS(C_BaseEntity,
                CS2_RUNTIME_ALIAS(C_BaseEntity, m_hOwnerEntity)
                CS2_RUNTIME_ALIAS(C_BaseEntity, m_iHealth)
                CS2_RUNTIME_ALIAS(C_BaseEntity, m_iTeamNum)
                CS2_RUNTIME_ALIAS(C_BaseEntity, m_lifeState)
                CS2_RUNTIME_ALIAS(C_BaseEntity, m_pGameSceneNode))
            CS2_RUNTIME_ALIAS_CLASS(C_BaseModelEntity,
                CS2_RUNTIME_ALIAS(C_BaseModelEntity, m_Glow)
                CS2_RUNTIME_ALIAS(C_BaseModelEntity, m_vecViewOffset))
            CS2_RUNTIME_ALIAS_CLASS(C_BasePlayerPawn,
                CS2_RUNTIME_ALIAS(C_BasePlayerPawn, m_hController)
                CS2_RUNTIME_ALIAS(C_BasePlayerPawn, m_pCameraServices)
                CS2_RUNTIME_ALIAS(C_BasePlayerPawn, m_pMovementServices)
                CS2_RUNTIME_ALIAS(C_BasePlayerPawn, m_pWeaponServices)
                CS2_RUNTIME_ALIAS(C_BasePlayerPawn, m_vOldOrigin))
            CS2_RUNTIME_ALIAS_CLASS(C_C4, CS2_RUNTIME_ALIAS(C_C4, m_bBombPlanted))
            CS2_RUNTIME_ALIAS_CLASS(C_CSPlayerPawn,
                CS2_RUNTIME_ALIAS(C_CSPlayerPawn, m_ArmorValue)
                CS2_RUNTIME_ALIAS(C_CSPlayerPawn, m_angEyeAngles)
                CS2_RUNTIME_ALIAS(C_CSPlayerPawn, m_entitySpottedState)
                CS2_RUNTIME_ALIAS(C_CSPlayerPawn, m_pAimPunchServices))
            CS2_RUNTIME_ALIAS_CLASS(C_CSPlayerPawnBase, CS2_RUNTIME_ALIAS(C_CSPlayerPawnBase, m_flFlashDuration))
            CS2_RUNTIME_ALIAS_CLASS(C_EconEntity, CS2_RUNTIME_ALIAS(C_EconEntity, m_AttributeManager))
            CS2_RUNTIME_ALIAS_CLASS(C_EconItemView, CS2_RUNTIME_ALIAS(C_EconItemView, m_iItemDefinitionIndex))
            CS2_RUNTIME_ALIAS_CLASS(C_PlantedC4,
                CS2_RUNTIME_ALIAS(C_PlantedC4, m_bBeingDefused)
                CS2_RUNTIME_ALIAS(C_PlantedC4, m_bBombDefused)
                CS2_RUNTIME_ALIAS(C_PlantedC4, m_bBombTicking)
                CS2_RUNTIME_ALIAS(C_PlantedC4, m_bHasExploded)
                CS2_RUNTIME_ALIAS(C_PlantedC4, m_nBombSite))
            CS2_RUNTIME_ALIAS_CLASS(CBasePlayerController, CS2_RUNTIME_ALIAS(CBasePlayerController, m_hPawn))
            CS2_RUNTIME_ALIAS_CLASS(CCSPlayer_AimPunchServices, CS2_RUNTIME_ALIAS(CCSPlayer_AimPunchServices, m_predictableBaseAngle))
            CS2_RUNTIME_ALIAS_CLASS(CCSPlayer_MovementServices, CS2_RUNTIME_ALIAS(CCSPlayer_MovementServices, m_flDuckAmount))
            CS2_RUNTIME_ALIAS_CLASS(CCSPlayerBase_CameraServices,
                CS2_RUNTIME_ALIAS(CCSPlayerBase_CameraServices, m_iFOV)
                CS2_RUNTIME_ALIAS(CCSPlayerBase_CameraServices, m_iFOVStart))
            CS2_RUNTIME_ALIAS_CLASS(CGameSceneNode, CS2_RUNTIME_ALIAS(CGameSceneNode, m_vecAbsOrigin))
            CS2_RUNTIME_ALIAS_CLASS(CGlowProperty,
                CS2_RUNTIME_ALIAS(CGlowProperty, m_bEligibleForScreenHighlight)
                CS2_RUNTIME_ALIAS(CGlowProperty, m_bFlashing)
                CS2_RUNTIME_ALIAS(CGlowProperty, m_bGlowing)
                CS2_RUNTIME_ALIAS(CGlowProperty, m_fGlowColor)
                CS2_RUNTIME_ALIAS(CGlowProperty, m_glowColorOverride)
                CS2_RUNTIME_ALIAS(CGlowProperty, m_iGlowTeam)
                CS2_RUNTIME_ALIAS(CGlowProperty, m_iGlowType)
                CS2_RUNTIME_ALIAS(CGlowProperty, m_nGlowRange)
                CS2_RUNTIME_ALIAS(CGlowProperty, m_nGlowRangeMin))
            CS2_RUNTIME_ALIAS_CLASS(CPlayer_WeaponServices, CS2_RUNTIME_ALIAS(CPlayer_WeaponServices, m_hActiveWeapon))
            CS2_RUNTIME_ALIAS_CLASS(EntitySpottedState_t,
                CS2_RUNTIME_ALIAS(EntitySpottedState_t, m_bSpotted)
                CS2_RUNTIME_ALIAS(EntitySpottedState_t, m_bSpottedByMask))
            #undef CS2_RUNTIME_ALIAS_CLASS
            #undef CS2_RUNTIME_ALIAS
        }
    }
}
