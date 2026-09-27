#include "runtime_offsets.hpp"

#include "memory/memory.hpp"
#include "../../dumper/pattern_scan.hpp"

#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>

namespace {
    using namespace cs2_runtime;

    struct ModuleView {
        uintptr_t base = 0;
        std::size_t size = 0;

        bool contains(uintptr_t address, std::size_t length = 1) const {
            return base && address >= base && length <= size
                && address - base <= size - length;
        }

        std::ptrdiff_t rva(uintptr_t address) const {
            return contains(address) ? static_cast<std::ptrdiff_t>(address - base) : 0;
        }
    };

    struct GlobalSnapshot {
        std::ptrdiff_t dwCSGOInput = 0;
        std::ptrdiff_t dwEntityList = 0;
        std::ptrdiff_t dwGameEntitySystem = 0;
        std::ptrdiff_t dwGameEntitySystem_highestEntityIndex = 0;
        std::ptrdiff_t dwGameRules = 0;
        std::ptrdiff_t dwGlobalVars = 0;
        std::ptrdiff_t dwGlowManager = 0;
        std::ptrdiff_t dwLocalPlayerController = 0;
        std::ptrdiff_t dwLocalPlayerPawn = 0;
        std::ptrdiff_t dwPlantedC4 = 0;
        std::ptrdiff_t dwPrediction = 0;
        std::ptrdiff_t dwSensitivity = 0;
        std::ptrdiff_t dwSensitivity_sensitivity = 0;
        std::ptrdiff_t dwViewAngles = 0;
        std::ptrdiff_t dwViewMatrix = 0;
        std::ptrdiff_t dwViewRender = 0;
        std::ptrdiff_t dwWeaponC4 = 0;
        std::ptrdiff_t dwBuildNumber = 0;
        std::ptrdiff_t inputViewAngles = 0;
        std::uint32_t buildNumber = 0;
    };

    struct SchemaFieldRequest {
        const char* className;
        const char* fieldName;
        std::ptrdiff_t* destination;
        std::ptrdiff_t value = 0;
        bool found = false;
    };

    std::atomic_bool g_initialized = false;
    std::mutex g_initializeMutex;
    GlobalSnapshot g_pendingGlobals{};
    bool g_havePendingGlobals = false;
    std::uint32_t g_buildNumber = 0;

    ModuleView GetModuleView(const char* name) {
        ModuleView result{};
        const auto module = reinterpret_cast<uintptr_t>(GetModuleHandleA(name));
        if (!module || !memory::IsReadable(module, sizeof(IMAGE_DOS_HEADER))) return result;

        const auto dos = memory::Read<IMAGE_DOS_HEADER>(module);
        if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew <= 0) return result;

        const uintptr_t ntAddress = module + static_cast<uintptr_t>(dos.e_lfanew);
        if (!memory::IsReadable(ntAddress, sizeof(IMAGE_NT_HEADERS64))) return result;

        const auto nt = memory::Read<IMAGE_NT_HEADERS64>(ntAddress);
        if (nt.Signature != IMAGE_NT_SIGNATURE || nt.OptionalHeader.SizeOfImage == 0) return result;

        result.base = module;
        result.size = nt.OptionalHeader.SizeOfImage;
        return result;
    }

    uintptr_t ResolveRip(uintptr_t match, std::size_t displacementOffset,
        std::size_t instructionSize) {
        if (!match || !memory::IsReadable(match + displacementOffset, sizeof(std::int32_t))) {
            return 0;
        }
        const auto displacement = memory::Read<std::int32_t>(match + displacementOffset);
        return match + instructionSize + displacement;
    }

    uintptr_t Find(const char* module, const char* pattern) {
        return reinterpret_cast<uintptr_t>(Scanner::PatternScan(module, pattern));
    }

    uintptr_t FindFirst(const char* module, std::initializer_list<const char*> patterns) {
        for (const char* pattern : patterns) {
            if (const uintptr_t match = Find(module, pattern)) return match;
        }
        return 0;
    }

    uintptr_t FindRipFirst(const char* module, std::initializer_list<const char*> patterns,
        std::size_t displacementOffset = 3, std::size_t instructionSize = 7) {
        return ResolveRip(FindFirst(module, patterns), displacementOffset, instructionSize);
    }

    std::string ReadString(uintptr_t address) {
        if (!address) return {};

        std::array<char, 256> buffer{};
        std::size_t length = 0;
        while (length + 1 < buffer.size()) {
            char value = 0;
            if (!memory::ReadRaw(address + length, &value, sizeof(value))) return {};
            if (!value) break;
            buffer[length++] = value;
        }
        return std::string(buffer.data(), length);
    }

    std::string ReadInlineString(uintptr_t address, std::size_t maxLength) {
        if (!address || maxLength == 0 || maxLength > 4096) return {};

        std::string result;
        result.reserve(maxLength);
        for (std::size_t index = 0; index < maxLength; ++index) {
            char value = 0;
            if (!memory::ReadRaw(address + index, &value, sizeof(value))) return {};
            if (!value) break;
            result.push_back(value);
        }
        return result;
    }

    bool ReadGlobalOffsets(GlobalSnapshot& out) {
        const ModuleView client = GetModuleView("client.dll");
        const ModuleView engine = GetModuleView("engine2.dll");
        if (!client.base || !engine.base) return false;

        out.dwCSGOInput = client.rva(FindRipFirst("client.dll", {
            "48 89 05 ? ? ? ? 0F 57 C0 0F 11 05"
        }));
        out.dwEntityList = client.rva(FindRipFirst("client.dll", {
            "48 89 0D ? ? ? ? E9 ? ? ? ? CC"
        }));
        out.dwGameEntitySystem = client.rva(FindRipFirst("client.dll", {
            "48 8B 1D ? ? ? ? 48 89 1D ? ? ? ? 4C 63 B3",
            "48 8B 1D ? ? ? ? 48 89 1D ? ? ? ?"
        }));

        if (const uintptr_t match = Find("client.dll", "FF 81 ? ? ? ? 48 85 D2")) {
            out.dwGameEntitySystem_highestEntityIndex =
                memory::Read<std::uint32_t>(match + 2);
        }

        out.dwGameRules = client.rva(FindRipFirst("client.dll", {
            "48 8D 05 ? ? ? ? 48 89 06 48 8D 4E 44",
            "48 8B 1D ? ? ? ? 48 8D 54 24 ? 0F 28 D0 48 8D 4C 24 ?"
        }));
        if (!out.dwGameRules) {
            const uintptr_t match = Find("client.dll",
                "F6 C1 01 0F 85 ? ? ? ? 4C 8B 05 ? ? ? ? 4D 85 C0");
            out.dwGameRules = client.rva(ResolveRip(match, 12, 16));
        }

        out.dwGlobalVars = client.rva(FindRipFirst("client.dll", {
            "48 89 15 ? ? ? ? 48 89 42"
        }));
        out.dwGlowManager = client.rva(FindRipFirst("client.dll", {
            "48 8B 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 8B 41"
        }));
        out.dwLocalPlayerController = client.rva(FindRipFirst("client.dll", {
            "48 8B 05 ? ? ? ? 41 89 BE"
        }));
        out.dwPlantedC4 = client.rva(FindRipFirst("client.dll", {
            "48 8B 1D ? ? ? ? 45 32 F6",
            "48 8B 15 ? ? ? ? 41 FF C0 48 8D 4C 24 ? 44 89 05 ? ? ? ?"
        }));
        if (!out.dwPlantedC4) {
            const uintptr_t match = Find("client.dll",
                "0F 85 ? ? ? ? 39 3D ? ? ? ? 7E ? 49 8B 0E 48 8B 1D ? ? ? ? 48 8B D3");
            out.dwPlantedC4 = client.rva(ResolveRip(match + 17, 3, 7));
        }
        out.dwPrediction = client.rva(FindRipFirst("client.dll", {
            "48 8D 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 40 53 56 41 54"
        }));
        out.dwSensitivity = client.rva(FindRipFirst("client.dll", {
            "48 8D 0D ? ? ? ? 66 0F 6E CD"
        }));
        out.dwViewMatrix = client.rva(FindRipFirst("client.dll", {
            "48 8D 0D ? ? ? ? 48 C1 E0 06"
        }));
        out.dwViewRender = client.rva(FindRipFirst("client.dll", {
            "48 89 05 ? ? ? ? 48 8B C8 48 85 C0"
        }));
        out.dwWeaponC4 = client.rva(FindRipFirst("client.dll", {
            "48 8B 15 ? ? ? ? 48 8B 5C 24 ? FF C0 89 05 ? ? ? ? 48 8B C6 48 89 34 EA 80 BE",
            "48 8B 15 ? ? ? ? 48 8B 5C 24 ? FF C0 89 05 ? ? ? ? 48 8B C6"
        }));

        if (const uintptr_t match = Find("client.dll", "4C 39 B6 ? ? ? ? 74 ? 44 88 BE")) {
            const auto relative = memory::Read<std::uint32_t>(match + 3);
            const uintptr_t address = client.base + out.dwPrediction + relative;
            out.dwLocalPlayerPawn = client.rva(address);
        }

        if (const uintptr_t match = Find("client.dll", "F2 42 0F 10 84 28 ? ? ? ?")) {
            out.inputViewAngles = memory::Read<std::uint32_t>(match + 6);
            const uintptr_t address = client.base + out.dwCSGOInput + out.inputViewAngles;
            out.dwViewAngles = client.rva(address);
        }

        if (const uintptr_t match = Find("client.dll",
            "48 8D 7E ? 48 0F BA E0 ? 72 ? 85 D2 49 0F 4F FF")) {
            out.dwSensitivity_sensitivity = memory::Read<std::uint8_t>(match + 3);
        }
        if (!out.dwSensitivity_sensitivity) out.dwSensitivity_sensitivity = 0x58;

        out.dwBuildNumber = engine.rva(FindRipFirst("engine2.dll", {
            "89 05 ? ? ? ? 48 8D 0D ? ? ? ? FF 15 ? ? ? ? 48 8B 0D",
            "89 05 ? ? ? ? 48 8D 0D ? ? ? ? FF 15"
        }, 2, 6));
        if (out.dwBuildNumber) {
            out.buildNumber = memory::Read<std::uint32_t>(engine.base + out.dwBuildNumber);
        }

        const bool offsetsInRange = out.dwCSGOInput > 0 && out.dwEntityList > 0
            && out.dwLocalPlayerPawn > 0 && out.dwPlantedC4 > 0
            && out.dwViewAngles > 0 && out.dwViewMatrix > 0
            && out.dwWeaponC4 > 0 && out.dwBuildNumber > 0;
        const bool memberOffsetsSane = out.dwGameEntitySystem_highestEntityIndex > 0
            && out.dwGameEntitySystem_highestEntityIndex < 0x10000
            && out.inputViewAngles > 0 && out.inputViewAngles < 0x10000
            && out.dwSensitivity_sensitivity < 0x1000;
        const bool buildSane = out.buildNumber >= 1000 && out.buildNumber <= 1000000;
        return offsetsInRange && memberOffsetsSane && buildSane;
    }

    template <std::size_t N>
    bool ResolveClass(uintptr_t classInfo, std::array<SchemaFieldRequest, N>& requests) {
        if (!classInfo) return false;
        const std::string className = ReadString(memory::Read<uintptr_t>(classInfo + 0x8));
        if (className.empty()) return false;

        bool requestedClass = false;
        for (const auto& request : requests) {
            if (!request.found && className == request.className) {
                requestedClass = true;
                break;
            }
        }
        if (!requestedClass) return false;

        const auto fieldCount = memory::Read<std::int16_t>(classInfo + 0x24);
        const uintptr_t fields = memory::Read<uintptr_t>(classInfo + 0x30);
        if (fieldCount <= 0 || fieldCount > 4096 || !fields) return false;

        for (auto& request : requests) {
            if (request.found || className != request.className) continue;
            for (std::int16_t fieldIndex = 0; fieldIndex < fieldCount; ++fieldIndex) {
                const uintptr_t field = fields + static_cast<uintptr_t>(fieldIndex) * 0x20;
                const std::string fieldName = ReadString(memory::Read<uintptr_t>(field));
                if (fieldName != request.fieldName) continue;

                const auto offset = memory::Read<std::int32_t>(field + 0x10);
                if (offset < 0 || offset > 0x100000) return false;
                request.value = offset;
                request.found = true;
                break;
            }
        }
        return true;
    }

    bool ReadSchemaFields() {
        const ModuleView schemaModule = GetModuleView("schemasystem.dll");
        if (!schemaModule.base) return false;

        const uintptr_t schemaSystem = FindRipFirst("schemasystem.dll", {
            "4C 8D 35 ? ? ? ? 0F 28 45"
        });
        if (!schemaModule.contains(schemaSystem)
            || memory::Read<std::int32_t>(schemaSystem + 0x280) <= 0) {
            return false;
        }

        const auto scopeCount = memory::Read<std::int32_t>(schemaSystem + 0x190);
        const uintptr_t scopeData = memory::Read<uintptr_t>(schemaSystem + 0x198);
        if (scopeCount <= 0 || scopeCount > 64 || !scopeData) return false;

        std::array requests = {
            SchemaFieldRequest{ "C_AttributeContainer", "m_Item", &schemas::client_dll::C_AttributeContainer::m_Item },
            SchemaFieldRequest{ "C_BaseEntity", "m_hOwnerEntity", &schemas::client_dll::C_BaseEntity::m_hOwnerEntity },
            SchemaFieldRequest{ "C_BaseEntity", "m_iHealth", &schemas::client_dll::C_BaseEntity::m_iHealth },
            SchemaFieldRequest{ "C_BaseEntity", "m_iTeamNum", &schemas::client_dll::C_BaseEntity::m_iTeamNum },
            SchemaFieldRequest{ "C_BaseEntity", "m_lifeState", &schemas::client_dll::C_BaseEntity::m_lifeState },
            SchemaFieldRequest{ "C_BaseEntity", "m_pGameSceneNode", &schemas::client_dll::C_BaseEntity::m_pGameSceneNode },
            SchemaFieldRequest{ "C_BaseModelEntity", "m_Glow", &schemas::client_dll::C_BaseModelEntity::m_Glow },
            SchemaFieldRequest{ "C_BaseModelEntity", "m_vecViewOffset", &schemas::client_dll::C_BaseModelEntity::m_vecViewOffset },
            SchemaFieldRequest{ "C_BasePlayerPawn", "m_hController", &schemas::client_dll::C_BasePlayerPawn::m_hController },
            SchemaFieldRequest{ "C_BasePlayerPawn", "m_pCameraServices", &schemas::client_dll::C_BasePlayerPawn::m_pCameraServices },
            SchemaFieldRequest{ "C_BasePlayerPawn", "m_pMovementServices", &schemas::client_dll::C_BasePlayerPawn::m_pMovementServices },
            SchemaFieldRequest{ "C_BasePlayerPawn", "m_pWeaponServices", &schemas::client_dll::C_BasePlayerPawn::m_pWeaponServices },
            SchemaFieldRequest{ "C_BasePlayerPawn", "m_vOldOrigin", &schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin },
            SchemaFieldRequest{ "C_C4", "m_bBombPlanted", &schemas::client_dll::C_C4::m_bBombPlanted },
            SchemaFieldRequest{ "C_CSPlayerPawn", "m_ArmorValue", &schemas::client_dll::C_CSPlayerPawn::m_ArmorValue },
            SchemaFieldRequest{ "C_CSPlayerPawn", "m_angEyeAngles", &schemas::client_dll::C_CSPlayerPawn::m_angEyeAngles },
            SchemaFieldRequest{ "C_CSPlayerPawn", "m_entitySpottedState", &schemas::client_dll::C_CSPlayerPawn::m_entitySpottedState },
            SchemaFieldRequest{ "C_CSPlayerPawn", "m_pAimPunchServices", &schemas::client_dll::C_CSPlayerPawn::m_pAimPunchServices },
            SchemaFieldRequest{ "C_CSPlayerPawnBase", "m_flFlashDuration", &schemas::client_dll::C_CSPlayerPawnBase::m_flFlashDuration },
            SchemaFieldRequest{ "C_EconEntity", "m_AttributeManager", &schemas::client_dll::C_EconEntity::m_AttributeManager },
            SchemaFieldRequest{ "C_EconItemView", "m_iItemDefinitionIndex", &schemas::client_dll::C_EconItemView::m_iItemDefinitionIndex },
            SchemaFieldRequest{ "C_PlantedC4", "m_bBeingDefused", &schemas::client_dll::C_PlantedC4::m_bBeingDefused },
            SchemaFieldRequest{ "C_PlantedC4", "m_bBombDefused", &schemas::client_dll::C_PlantedC4::m_bBombDefused },
            SchemaFieldRequest{ "C_PlantedC4", "m_bBombTicking", &schemas::client_dll::C_PlantedC4::m_bBombTicking },
            SchemaFieldRequest{ "C_PlantedC4", "m_bHasExploded", &schemas::client_dll::C_PlantedC4::m_bHasExploded },
            SchemaFieldRequest{ "C_PlantedC4", "m_nBombSite", &schemas::client_dll::C_PlantedC4::m_nBombSite },
            SchemaFieldRequest{ "CBasePlayerController", "m_hPawn", &schemas::client_dll::CBasePlayerController::m_hPawn },
            SchemaFieldRequest{ "CCSPlayer_AimPunchServices", "m_predictableBaseAngle", &schemas::client_dll::CCSPlayer_AimPunchServices::m_predictableBaseAngle },
            SchemaFieldRequest{ "CCSPlayer_MovementServices", "m_flDuckAmount", &schemas::client_dll::CCSPlayer_MovementServices::m_flDuckAmount },
            SchemaFieldRequest{ "CCSPlayerBase_CameraServices", "m_iFOV", &schemas::client_dll::CCSPlayerBase_CameraServices::m_iFOV },
            SchemaFieldRequest{ "CCSPlayerBase_CameraServices", "m_iFOVStart", &schemas::client_dll::CCSPlayerBase_CameraServices::m_iFOVStart },
            SchemaFieldRequest{ "CGameSceneNode", "m_vecAbsOrigin", &schemas::client_dll::CGameSceneNode::m_vecAbsOrigin },
            SchemaFieldRequest{ "CGlowProperty", "m_bEligibleForScreenHighlight", &schemas::client_dll::CGlowProperty::m_bEligibleForScreenHighlight },
            SchemaFieldRequest{ "CGlowProperty", "m_bFlashing", &schemas::client_dll::CGlowProperty::m_bFlashing },
            SchemaFieldRequest{ "CGlowProperty", "m_bGlowing", &schemas::client_dll::CGlowProperty::m_bGlowing },
            SchemaFieldRequest{ "CGlowProperty", "m_fGlowColor", &schemas::client_dll::CGlowProperty::m_fGlowColor },
            SchemaFieldRequest{ "CGlowProperty", "m_glowColorOverride", &schemas::client_dll::CGlowProperty::m_glowColorOverride },
            SchemaFieldRequest{ "CGlowProperty", "m_iGlowTeam", &schemas::client_dll::CGlowProperty::m_iGlowTeam },
            SchemaFieldRequest{ "CGlowProperty", "m_iGlowType", &schemas::client_dll::CGlowProperty::m_iGlowType },
            SchemaFieldRequest{ "CGlowProperty", "m_nGlowRange", &schemas::client_dll::CGlowProperty::m_nGlowRange },
            SchemaFieldRequest{ "CGlowProperty", "m_nGlowRangeMin", &schemas::client_dll::CGlowProperty::m_nGlowRangeMin },
            SchemaFieldRequest{ "CPlayer_WeaponServices", "m_hActiveWeapon", &schemas::client_dll::CPlayer_WeaponServices::m_hActiveWeapon },
            SchemaFieldRequest{ "EntitySpottedState_t", "m_bSpotted", &schemas::client_dll::EntitySpottedState_t::m_bSpotted },
            SchemaFieldRequest{ "EntitySpottedState_t", "m_bSpottedByMask", &schemas::client_dll::EntitySpottedState_t::m_bSpottedByMask }
        };

        uintptr_t clientScope = 0;
        for (std::int32_t index = 0; index < scopeCount; ++index) {
            const uintptr_t scope = memory::Read<uintptr_t>(
                scopeData + static_cast<uintptr_t>(index) * sizeof(uintptr_t));
            const std::string scopeName = scope ? ReadInlineString(scope + 0x8, 256) : std::string{};
            if (scope && (_stricmp(scopeName.c_str(), "client.dll") == 0
                || _stricmp(scopeName.c_str(), "client") == 0)) {
                clientScope = scope;
                break;
            }
        }
        if (!clientScope) return false;

        const uintptr_t bindings = clientScope + 0x560;
        const auto blocksAllocated = memory::Read<std::int32_t>(bindings + 0xC);
        const auto peakAllocated = memory::Read<std::int32_t>(bindings + 0x10);
        if (blocksAllocated < 0 || blocksAllocated > 100000
            || peakAllocated < 0 || peakAllocated > 100000) {
            return false;
        }

        std::unordered_set<uintptr_t> seenNodes;
        std::unordered_set<uintptr_t> seenClasses;
        const auto visitList = [&](uintptr_t first, std::size_t limit, std::size_t nextOffset) {
            uintptr_t node = first;
            for (std::size_t count = 0; node && count < limit; ++count) {
                if (!seenNodes.insert(node).second) break;
                const uintptr_t classInfo = memory::Read<uintptr_t>(node + 0x10);
                if (classInfo && seenClasses.insert(classInfo).second) {
                    ResolveClass(classInfo, requests);
                }
                node = memory::Read<uintptr_t>(node + nextOffset);
            }
        };

        const std::size_t bucketLimit = static_cast<std::size_t>(blocksAllocated) + 1;
        for (std::size_t bucketIndex = 0; bucketIndex < 256; ++bucketIndex) {
            const uintptr_t bucket = bindings + 0x60 + bucketIndex * 0x18;
            visitList(memory::Read<uintptr_t>(bucket + 0x10), bucketLimit, 0x8);
            visitList(memory::Read<uintptr_t>(bucket + 0x8), bucketLimit, 0x8);
        }

        const std::size_t freeLimit = static_cast<std::size_t>(peakAllocated) + 1;
        visitList(memory::Read<uintptr_t>(bindings + 0x20), freeLimit, 0x0);

        for (const auto& request : requests) {
            if (!request.found) return false;
        }
        for (const auto& request : requests) {
            *request.destination = request.value;
        }
        return true;
    }

    void PublishGlobals(const GlobalSnapshot& value) {
        offsets::client_dll::dwCSGOInput = value.dwCSGOInput;
        offsets::client_dll::dwEntityList = value.dwEntityList;
        offsets::client_dll::dwGameEntitySystem = value.dwGameEntitySystem;
        offsets::client_dll::dwGameEntitySystem_highestEntityIndex = value.dwGameEntitySystem_highestEntityIndex;
        offsets::client_dll::dwGameRules = value.dwGameRules;
        offsets::client_dll::dwGlobalVars = value.dwGlobalVars;
        offsets::client_dll::dwGlowManager = value.dwGlowManager;
        offsets::client_dll::dwLocalPlayerController = value.dwLocalPlayerController;
        offsets::client_dll::dwLocalPlayerPawn = value.dwLocalPlayerPawn;
        offsets::client_dll::dwPlantedC4 = value.dwPlantedC4;
        offsets::client_dll::dwPrediction = value.dwPrediction;
        offsets::client_dll::dwSensitivity = value.dwSensitivity;
        offsets::client_dll::dwSensitivity_sensitivity = value.dwSensitivity_sensitivity;
        offsets::client_dll::dwViewAngles = value.dwViewAngles;
        offsets::client_dll::dwViewMatrix = value.dwViewMatrix;
        offsets::client_dll::dwViewRender = value.dwViewRender;
        offsets::client_dll::dwWeaponC4 = value.dwWeaponC4;
        offsets::engine2_dll::dwBuildNumber = value.dwBuildNumber;
        layouts::CCSGOInput_m_angViewAngles = value.inputViewAngles;
        g_buildNumber = value.buildNumber;
    }
}

namespace cs2_runtime {
    bool Initialize() {
        if (g_initialized.load(std::memory_order_acquire)) return true;

        std::lock_guard lock(g_initializeMutex);
        if (g_initialized.load(std::memory_order_relaxed)) return true;

        if (!g_havePendingGlobals) {
            GlobalSnapshot globals{};
            if (!ReadGlobalOffsets(globals)) return false;
            g_pendingGlobals = globals;
            g_havePendingGlobals = true;
        }
        if (!ReadSchemaFields()) return false;

        PublishGlobals(g_pendingGlobals);
        g_initialized.store(true, std::memory_order_release);
        return true;
    }

    bool IsInitialized() {
        return g_initialized.load(std::memory_order_acquire);
    }

    std::uint32_t BuildNumber() {
        return IsInitialized() ? g_buildNumber : 0;
    }
}
