#include "cheat.hpp"
#include "../utils/weapon_names.hpp"
#include "../../dumper/pattern_scan.hpp"
#include "../../dumper/patterns.hpp"
#include "../../minhook_debug_x64/include/MinHook.h"
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <cctype>

uint32_t WIDTH = 1920;
uint32_t HEIGHT = 1080;
uint32_t WINDOW_W = 1920;
uint32_t WINDOW_H = 1080;

namespace Cheat
{
    std::vector<EnemyInfo> enemies;
    viewMatrix vm = {};
    LocalPlayerCache localPlayer;
    std::mutex dataMutex;
    bool hasAimTarget = false;
    vec3 aimTargetPosition = { 0,0,0 };
    C4Info c4Info;
    std::vector<GrenadeInfo> grenades;
    std::vector<DroppedWeaponInfo> droppedWeapons;
    static std::unordered_map<uintptr_t, GrenadeInfo> g_grenadeMap;
    static std::unordered_map<uintptr_t, DroppedWeaponInfo> g_droppedWeaponMap;
    static bool g_SilentAimHookReady = false;
    static volatile LONG g_localSpottedBit = -1;
    static float g_ThirdPersonProgress = 0.0f;
    static bool g_ThirdPersonHookReady = false;
    static ULONGLONG g_lastThirdPersonHookAttempt = 0;
    using OnGlowTypeChangedFn = void(__fastcall*)(uintptr_t);
    static OnGlowTypeChangedFn g_onGlowTypeChanged = nullptr;
    static bool g_glowCallbackResolved = false;
    static std::unordered_set<uintptr_t> g_glowingEntities;

    static int frameCounter = 0;
    static int lastUpdateFrame = 0;

    struct WeaponNameCache {
        std::unordered_map<uint16_t, std::string> cache;
        std::mutex mutex;
    };
    static WeaponNameCache weaponCache;

    static EnemyInfo enemyBuffer[64];

    // ===== 调试变量 =====
    static std::vector<std::string> g_debugEntityNames;
    static size_t g_debugTrackedCount = 0;
    static int g_debugUpdateCalls = 0;
    static int g_debugTotalNames = 0;
    static int g_debugGrenadeNames = 0;
    static int g_debugProjectileNames = 0;
    static int g_debugCarriedCount = 0;
    static int g_debugThrownCount = 0;
    static int g_debugDroppedWeaponCount = 0;
    static uint32_t g_debugHighestIndex = 0;
    static std::unordered_set<uintptr_t> g_prevFrameEntities;
    static std::vector<std::pair<std::string, float>> g_recentNewEntities;

    // ============================================================================
    // ★ 前置声明：供 UpdateGroundItems 使用
    // ============================================================================
    static uintptr_t ResolveEntityHandle(uintptr_t entityList, uint32_t handle);
    static bool TryGetEntityPosition(uintptr_t entity, vec3& position);

    static bool ResolveGlowCallback() {
        if (!g_glowCallbackResolved) {
            g_glowCallbackResolved = true;
            g_onGlowTypeChanged = reinterpret_cast<OnGlowTypeChangedFn>(Scanner::PatternScan(
                "client.dll",
                cs2_patterns::OnGlowTypeChanged
            ));
        }
        return g_onGlowTypeChanged != nullptr;
    }

    static void NotifyGlowChanged(uintptr_t glowProperty) {
        if (!ResolveGlowCallback()) return;
        __try { g_onGlowTypeChanged(glowProperty); }
        __except (EXCEPTION_EXECUTE_HANDLER) { g_onGlowTypeChanged = nullptr; }
    }

    static uint8_t GlowColorChannel(float value) {
        return static_cast<uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
    }

    static uint32_t PackGlowColor(const ImVec4& color) {
        return static_cast<uint32_t>(GlowColorChannel(color.x))
            | (static_cast<uint32_t>(GlowColorChannel(color.y)) << 8)
            | (static_cast<uint32_t>(GlowColorChannel(color.z)) << 16)
            | (static_cast<uint32_t>(GlowColorChannel(color.w)) << 24);
    }

    static bool SetEntityGlow(uintptr_t entity, bool enabled, const ImVec4& color) {
        namespace Glow = cs2_dumper::schemas::client_dll::CGlowProperty;
        constexpr int32_t playerGlowType = 3;
        constexpr size_t glowPropertySize = 0x58;

        uintptr_t glowProperty = entity + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_Glow;
        if (!memory::IsWritable(glowProperty, glowPropertySize)) return false;

        int32_t currentType = memory::Read<int32_t>(glowProperty + Glow::m_iGlowType);
        bool currentGlowing = memory::Read<bool>(glowProperty + Glow::m_bGlowing);

        if (!enabled) {
            memory::Write<int32_t>(glowProperty + Glow::m_iGlowType, 0);
            if (currentType != 0 || currentGlowing) NotifyGlowChanged(glowProperty);
            memory::Write<bool>(glowProperty + Glow::m_bEligibleForScreenHighlight, false);
            memory::Write<bool>(glowProperty + Glow::m_bGlowing, false);
            return true;
        }

        uint32_t packedColor = PackGlowColor(color);
        uint32_t currentColor = memory::Read<uint32_t>(glowProperty + Glow::m_glowColorOverride);
        bool needsRefresh = currentType != playerGlowType || currentColor != packedColor || !currentGlowing;

        memory::Write<float>(glowProperty + Glow::m_fGlowColor, color.x);
        memory::Write<float>(glowProperty + Glow::m_fGlowColor + sizeof(float), color.y);
        memory::Write<float>(glowProperty + Glow::m_fGlowColor + sizeof(float) * 2, color.z);
        memory::Write<int32_t>(glowProperty + Glow::m_iGlowType, playerGlowType);
        memory::Write<int32_t>(glowProperty + Glow::m_iGlowTeam, 0);
        memory::Write<int32_t>(glowProperty + Glow::m_nGlowRange, 0);
        memory::Write<int32_t>(glowProperty + Glow::m_nGlowRangeMin, 0);
        memory::Write<uint32_t>(glowProperty + Glow::m_glowColorOverride, packedColor);
        memory::Write<bool>(glowProperty + Glow::m_bFlashing, false);
        memory::Write<bool>(glowProperty + Glow::m_bEligibleForScreenHighlight, true);
        memory::Write<bool>(glowProperty + Glow::m_bGlowing, true);

        if (needsRefresh) NotifyGlowChanged(glowProperty);
        return true;
    }

    static void ClearRuntimeCache() {
        std::lock_guard<std::mutex> lock(dataMutex);
        enemies.clear();
        enemies.shrink_to_fit();
        localPlayer = {};
        hasAimTarget = false;
        aimTargetPosition = { 0,0,0 };
        c4Info = {};
        g_localSpottedBit = -1;
        g_glowingEntities.clear();
        grenades.clear();
        g_grenadeMap.clear();
        droppedWeapons.clear();
        g_droppedWeaponMap.clear();
    }

    std::string GetWeaponNameCached(uint16_t itemDef) {
        if (!g_MenuData.performance.enableWeaponCache) return weapon_names::getWeaponName(itemDef);
        std::lock_guard<std::mutex> lock(weaponCache.mutex);
        auto it = weaponCache.cache.find(itemDef);
        if (it != weaponCache.cache.end()) return it->second;
        std::string name = weapon_names::getWeaponName(itemDef);
        weaponCache.cache[itemDef] = name;
        if (weaponCache.cache.size() > 100) weaponCache.cache.clear();
        return name;
    }

    static std::string GetEntityDesignName(uintptr_t entity) {
        if (!entity) return "";
        uintptr_t pIdentity = memory::Read<uintptr_t>(entity + 0x10);
        if (!pIdentity || pIdentity < 0x10000) return "";

        const uintptr_t kCandidates[] = { 0x20, 0x18, 0x28, 0x30, 0x10, 0x38, 0x40, 0x08, 0x48, 0x50 };
        for (uintptr_t off : kCandidates) {
            uintptr_t pString = memory::Read<uintptr_t>(pIdentity + off);
            if (!pString || pString < 0x10000) continue;
            if (pString > 0x00007FFFFFFFFFFFULL) continue;

            char buffer[96] = {};
            int validLen = 0;
            bool valid = true;
            for (int i = 0; i < 95; ++i) {
                char c = memory::Read<char>(pString + i);
                if (c == '\0') { validLen = i; break; }
                const bool isLower = (c >= 'a' && c <= 'z');
                const bool isUpper = (c >= 'A' && c <= 'Z');
                const bool isDigit = (c >= '0' && c <= '9');
                const bool isPunct = (c == '_' || c == '-' || c == '.');
                const bool isSlash = (c == '/' || c == '\\');
                if (!(isLower || isUpper || isDigit || isPunct || isSlash)) { valid = false; break; }
                buffer[i] = c;
            }
            if (valid && validLen >= 3) return std::string(buffer, validLen);
        }
        return "";
    }

    static int ClassifyGrenade(const std::string& designName, bool& isThrown) {
        isThrown = false;
        if (designName.empty()) return -1;

        std::string lower = designName;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (lower.find("projectile") != std::string::npos) {
            isThrown = true;
        }
        else if (lower.find("weapon_") != std::string::npos) {
            isThrown = false;
        }
        else {
            return -1;
        }

        if (lower.find("hegrenade") != std::string::npos) return 0;
        if (lower.find("flashbang") != std::string::npos) return 1;
        if (lower.find("smokegrenade") != std::string::npos) return 2;
        if (lower.find("molotov") != std::string::npos) return 3;
        if (lower.find("decoy") != std::string::npos) return 4;
        if (lower.find("incendiary") != std::string::npos || lower.find("incgrenade") != std::string::npos) return 5;
        if (lower.find("smoke") != std::string::npos) return 2;

        return -1;
    }

    static const char* GetGrenadeNameEN(int type) {
        switch (type) {
        case 0: return "HE";
        case 1: return "Flash";
        case 2: return "Smoke";
        case 3: return "Molotov";
        case 4: return "Decoy";
        case 5: return "Incendiary";
        default: return "Grenade";
        }
    }

    static const char* GetGrenadeNameCN(int type) {
        switch (type) {
        case 0: return "手雷";
        case 1: return "闪光弹";
        case 2: return "烟雾弹";
        case 3: return "燃烧瓶";
        case 4: return "诱饵弹";
        case 5: return "燃烧弹";
        default: return "投掷物";
        }
    }

    static const char* GetGrenadeName(int type) {
        return g_MenuData.language == 0 ? GetGrenadeNameEN(type) : GetGrenadeNameCN(type);
    }

    static ImU32 GetGrenadeColor(int type) {
        switch (type) {
        case 0: return IM_COL32(255, 100, 100, 255);
        case 1: return IM_COL32(255, 255, 100, 255);
        case 2: return IM_COL32(150, 200, 255, 255);
        case 3: return IM_COL32(255, 150, 50, 255);
        case 4: return IM_COL32(200, 100, 255, 255);
        case 5: return IM_COL32(255, 80, 30, 255);
        default: return IM_COL32(200, 200, 200, 255);
        }
    }

    void UpdateGrenades() {
        uintptr_t client = memory::clientBase;
        if (!client) return;

        uintptr_t entitySystem = memory::Read<uintptr_t>(
            client + cs2_dumper::offsets::client_dll::dwGameEntitySystem);
        if (!entitySystem) return;

        uint32_t highestIndex = memory::Read<uint32_t>(
            entitySystem + cs2_dumper::offsets::client_dll::dwGameEntitySystem_highestEntityIndex);
        if (highestIndex > 0x10000) highestIndex = 0;

        g_debugHighestIndex = highestIndex;

        uint32_t scanLimit = highestIndex;
        if (scanLimit < 2048) scanLimit = 2048;
        if (scanLimit > 8192) scanLimit = 8192;

        uintptr_t entityList = memory::Read<uintptr_t>(
            client + cs2_dumper::offsets::client_dll::dwEntityList);
        if (!entityList) return;

        float deltaTime = ImGui::GetIO().DeltaTime;
        if (deltaTime <= 0.0f || deltaTime > 0.5f) deltaTime = 0.016f;

        std::unordered_set<uintptr_t> foundEntities;

        bool debugEnabled = g_MenuData.bGrenadeDebug;
        float now = ImGui::GetTime();

        std::vector<std::string> debugNames;
        std::unordered_set<uintptr_t> currentFrameEntities;
        int totalNames = 0, grenadeNames = 0, projectileNames = 0;
        int carriedCount = 0, thrownCount = 0;

        if (debugEnabled) {
            auto it = g_recentNewEntities.begin();
            while (it != g_recentNewEntities.end()) {
                if (now - it->second > 5.0f) it = g_recentNewEntities.erase(it);
                else ++it;
            }
            debugNames.reserve(64);
            g_debugUpdateCalls++;
        }

        for (uint32_t i = 1; i <= scanLimit; ++i) {
            uintptr_t listEntry = memory::Read<uintptr_t>(entityList + 0x10 + 0x8 * (i >> 9));
            if (!listEntry) continue;

            uintptr_t entity = memory::Read<uintptr_t>(listEntry + 0x70 * (i & 0x1FF));
            if (!entity || entity < 0x10000) continue;

            std::string designName = GetEntityDesignName(entity);
            if (designName.empty()) continue;

            bool isThrown = false;
            int type = ClassifyGrenade(designName, isThrown);

            if (debugEnabled) {
                totalNames++;
                currentFrameEntities.insert(entity);

                if (g_prevFrameEntities.find(entity) == g_prevFrameEntities.end()) {
                    char newLine[160];
                    snprintf(newLine, sizeof(newLine), "[%u] %s", i, designName.c_str());
                    g_recentNewEntities.push_back({ std::string(newLine), now });
                }

                std::string lowerDbg = designName;
                std::transform(lowerDbg.begin(), lowerDbg.end(), lowerDbg.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (lowerDbg.find("grenade") != std::string::npos) grenadeNames++;
                if (lowerDbg.find("projectile") != std::string::npos) projectileNames++;

                bool skip =
                    lowerDbg == "cs_player_controller" ||
                    lowerDbg == "cs_player_manager" ||
                    lowerDbg == "cs_team_manager" ||
                    lowerDbg == "cs_gamerules" ||
                    lowerDbg == "vote_controller";

                if (!skip && debugNames.size() < 60) {
                    char line[160];
                    snprintf(line, sizeof(line), "[%u] %s", i, designName.c_str());
                    debugNames.push_back(line);
                }
            }

            if (type < 0) continue;

            if (isThrown && !g_MenuData.bGrenadeThrown) continue;
            if (!isThrown && !g_MenuData.bGrenadeCarried) continue;

            if (isThrown) thrownCount++;
            else carriedCount++;

            uintptr_t sceneNode = memory::Read<uintptr_t>(
                entity + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode);
            if (!sceneNode) continue;

            vec3 pos = memory::Read<vec3>(
                sceneNode + cs2_dumper::schemas::client_dll::CGameSceneNode::m_vecAbsOrigin);
            if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z)) continue;
            if (std::abs(pos.x) > 100000.0f || std::abs(pos.y) > 100000.0f || std::abs(pos.z) > 100000.0f) continue;

            foundEntities.insert(entity);

            auto it = g_grenadeMap.find(entity);
            if (it == g_grenadeMap.end()) {
                GrenadeInfo info;
                info.entity = entity;
                info.position = pos;
                info.lastPosition = pos;
                info.landingPosition = pos;
                info.type = type;
                info.isActive = true;
                info.landingRecorded = false;
                info.disappearTimer = 0.0f;
                info.isThrown = isThrown;
                g_grenadeMap[entity] = info;
            }
            else {
                GrenadeInfo& info = it->second;
                float dx = pos.x - info.lastPosition.x;
                float dy = pos.y - info.lastPosition.y;
                float dz = pos.z - info.lastPosition.z;
                float moved = std::sqrt(dx * dx + dy * dy + dz * dz);

                info.lastPosition = info.position;
                info.position = pos;
                info.type = type;
                info.isActive = true;
                info.disappearTimer = 0.0f;
                info.isThrown = isThrown;

                if (!info.landingRecorded && moved < 0.5f) {
                    info.landingPosition = pos;
                    info.landingRecorded = true;
                }
            }
        }

        constexpr float LANDING_RETAIN_TIME = 1.5f;

        for (auto it = g_grenadeMap.begin(); it != g_grenadeMap.end();) {
            if (foundEntities.find(it->first) != foundEntities.end()) { ++it; continue; }
            GrenadeInfo& info = it->second;
            if (info.isActive) {
                info.isActive = false;
                info.disappearTimer = 0.0f;
                if (!info.landingRecorded) {
                    info.landingPosition = info.position;
                    info.landingRecorded = true;
                }
            }
            else {
                info.disappearTimer += deltaTime;
                if (info.disappearTimer > LANDING_RETAIN_TIME) {
                    it = g_grenadeMap.erase(it);
                    continue;
                }
            }
            ++it;
        }

        {
            std::lock_guard<std::mutex> lock(dataMutex);
            grenades.clear();
            grenades.reserve(g_grenadeMap.size());
            for (const auto& kv : g_grenadeMap) grenades.push_back(kv.second);
        }

        if (debugEnabled) {
            std::lock_guard<std::mutex> lock(dataMutex);
            g_debugEntityNames = std::move(debugNames);
            g_debugTrackedCount = g_grenadeMap.size();
            g_debugTotalNames = totalNames;
            g_debugGrenadeNames = grenadeNames;
            g_debugProjectileNames = projectileNames;
            g_debugCarriedCount = carriedCount;
            g_debugThrownCount = thrownCount;
            g_prevFrameEntities = std::move(currentFrameEntities);
        }
    }

    // ============================================================================
    // 掉落武器 + C4 携带/掉落
    // ============================================================================
    void UpdateGroundItems() {
        bool needWeapons = g_MenuData.bWeaponESP;
        bool needC4 = g_MenuData.bC4ESP;

        if (!needWeapons && !needC4) {
            std::lock_guard<std::mutex> lock(dataMutex);
            droppedWeapons.clear();
            g_droppedWeaponMap.clear();
            g_debugDroppedWeaponCount = 0;
            if (!c4Info.isPlanted) {
                c4Info.exists = false;
                c4Info.isCarried = false;
                c4Info.isDropped = false;
                c4Info.carrierPawn = 0;
                c4Info.position = { 0,0,0 };
            }
            for (auto& e : enemies) e.hasC4 = false;
            return;
        }

        uintptr_t client = memory::clientBase;
        if (!client) return;

        uintptr_t entityList = memory::Read<uintptr_t>(
            client + cs2_dumper::offsets::client_dll::dwEntityList);
        if (!entityList) return;

        uintptr_t entitySystem = memory::Read<uintptr_t>(
            client + cs2_dumper::offsets::client_dll::dwGameEntitySystem);
        uint32_t highest = 0;
        if (entitySystem) {
            highest = memory::Read<uint32_t>(
                entitySystem + cs2_dumper::offsets::client_dll::dwGameEntitySystem_highestEntityIndex);
        }
        if (highest < 4096) highest = 4096;
        if (highest > 8192) highest = 8192;

        std::unordered_set<uintptr_t> foundDropped;

        bool foundCarriedC4 = false;
        uintptr_t carriedC4Owner = 0;
        vec3 carriedC4Pos = { 0,0,0 };
        bool foundDroppedC4 = false;
        vec3 droppedC4Pos = { 0,0,0 };

        for (uint32_t i = 1; i <= highest; ++i) {
            uintptr_t listEntry = memory::Read<uintptr_t>(entityList + 0x10 + 0x8 * (i >> 9));
            if (!listEntry) continue;

            uintptr_t entity = memory::Read<uintptr_t>(listEntry + 0x70 * (i & 0x1FF));
            if (!entity || entity < 0x10000) continue;

            std::string designName = GetEntityDesignName(entity);
            if (designName.empty()) continue;

            std::string lower = designName;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            if (lower.find("weapon_") != 0) continue;

            uint32_t ownerHandle = memory::Read<uint32_t>(
                entity + cs2_dumper::schemas::client_dll::C_BaseEntity::m_hOwnerEntity);
            bool isDropped = (ownerHandle == 0xFFFFFFFF || ownerHandle == 0);

            // C4
            if (lower.find("weapon_c4") != std::string::npos) {
                if (needC4) {
                    if (isDropped) {
                        uintptr_t sceneNode = memory::Read<uintptr_t>(
                            entity + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode);
                        if (sceneNode) {
                            vec3 pos = memory::Read<vec3>(
                                sceneNode + cs2_dumper::schemas::client_dll::CGameSceneNode::m_vecAbsOrigin);
                            if (std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z)) {
                                foundDroppedC4 = true;
                                droppedC4Pos = pos;
                            }
                        }
                    }
                    else {
                        uintptr_t owner = ResolveEntityHandle(entityList, ownerHandle);
                        if (owner) {
                            foundCarriedC4 = true;
                            carriedC4Owner = owner;
                            if (!TryGetEntityPosition(owner, carriedC4Pos)) {
                                TryGetEntityPosition(entity, carriedC4Pos);
                            }
                        }
                    }
                }
                continue;
            }

            // 普通掉落武器
            if (needWeapons && isDropped) {
                uintptr_t sceneNode = memory::Read<uintptr_t>(
                    entity + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode);
                if (!sceneNode) continue;
                vec3 pos = memory::Read<vec3>(
                    sceneNode + cs2_dumper::schemas::client_dll::CGameSceneNode::m_vecAbsOrigin);
                if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z)) continue;
                if (std::abs(pos.x) > 100000.0f || std::abs(pos.y) > 100000.0f || std::abs(pos.z) > 100000.0f) continue;

                uintptr_t attrMgr = entity + cs2_dumper::schemas::client_dll::C_EconEntity::m_AttributeManager;
                uint16_t itemDef = memory::Read<uint16_t>(
                    attrMgr
                    + cs2_dumper::schemas::client_dll::C_AttributeContainer::m_Item
                    + cs2_dumper::schemas::client_dll::C_EconItemView::m_iItemDefinitionIndex);
                std::string weaponName = GetWeaponNameCached(itemDef);

                foundDropped.insert(entity);

                DroppedWeaponInfo info;
                info.entity = entity;
                info.position = pos;
                info.weaponName = weaponName;
                info.itemDef = itemDef;
                info.isC4 = false;
                g_droppedWeaponMap[entity] = info;
            }
        }

        for (auto it = g_droppedWeaponMap.begin(); it != g_droppedWeaponMap.end();) {
            if (foundDropped.find(it->first) == foundDropped.end()) {
                it = g_droppedWeaponMap.erase(it);
            }
            else ++it;
        }

        {
            std::lock_guard<std::mutex> lock(dataMutex);

            droppedWeapons.clear();
            droppedWeapons.reserve(g_droppedWeaponMap.size());
            for (const auto& kv : g_droppedWeaponMap) droppedWeapons.push_back(kv.second);
            g_debugDroppedWeaponCount = (int)droppedWeapons.size();

            if (!c4Info.isPlanted) {
                if (foundCarriedC4) {
                    c4Info.exists = true;
                    c4Info.isCarried = true;
                    c4Info.isDropped = false;
                    c4Info.carrierPawn = carriedC4Owner;
                    c4Info.position = carriedC4Pos;
                }
                else if (foundDroppedC4) {
                    c4Info.exists = true;
                    c4Info.isCarried = false;
                    c4Info.isDropped = true;
                    c4Info.carrierPawn = 0;
                    c4Info.position = droppedC4Pos;
                }
                else {
                    c4Info.exists = false;
                    c4Info.isCarried = false;
                    c4Info.isDropped = false;
                    c4Info.carrierPawn = 0;
                    c4Info.position = { 0,0,0 };
                }
            }

            for (auto& e : enemies) {
                e.hasC4 = c4Info.isCarried && c4Info.carrierPawn != 0 && e.pawn == c4Info.carrierPawn;
            }
        }
    }

    static const int aimKeyMap[] = {
        VK_CONTROL, VK_SHIFT, 'W', 'Q', 'E', 'R',
        VK_LBUTTON, VK_RBUTTON, VK_XBUTTON1, VK_XBUTTON2, VK_MBUTTON,
        'Z', 'X', 'C', VK_CAPITAL
    };

    static const int thirdPersonKeyMap[] = {
        VK_F5, VK_F6, VK_F7, VK_F8, VK_F9,
        VK_CONTROL, VK_SHIFT, 'V', 'B', 'N', VK_CAPITAL
    };

    bool Init(uintptr_t clientBase) {
        if (!clientBase || !memory::engineBase) return false;
        return cs2_runtime::Initialize();
    }

    float NormalizeAngle(float angle) {
        while (angle > 180.0f) angle -= 360.0f;
        while (angle < -180.0f) angle += 360.0f;
        return angle;
    }

    vec2 CalcAngle(const vec3& src, const vec3& dst) {
        vec3 delta = { dst.x - src.x, dst.y - src.y, dst.z - src.z };
        float hyp = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        vec2 ang;
        ang.y = std::atan2(delta.y, delta.x) * (180.0f / 3.14159265f);
        ang.x = -std::atan2(delta.z, hyp) * (180.0f / 3.14159265f);
        return ang;
    }

    float GetFOV(const vec2& viewAngle, const vec2& aimAngle) {
        float pitchDiff = NormalizeAngle(aimAngle.x - viewAngle.x);
        float yawDiff = NormalizeAngle(aimAngle.y - viewAngle.y);
        return std::sqrt(pitchDiff * pitchDiff + yawDiff * yawDiff);
    }

    inline bool WorldToScreen(const vec3& world, vec2& screen, const float m[16]) {
        vec4 clip;
        clip.x = world.x * m[0] + world.y * m[1] + world.z * m[2] + m[3];
        clip.y = world.x * m[4] + world.y * m[5] + world.z * m[6] + m[7];
        clip.z = world.x * m[8] + world.y * m[9] + world.z * m[10] + m[11];
        clip.w = world.x * m[12] + world.y * m[13] + world.z * m[14] + m[15];
        if (clip.w < 0.1f) return false;
        vec3 ndc = { clip.x / clip.w, clip.y / clip.w, clip.z / clip.w };
        screen.x = (WINDOW_W / 2.0f * ndc.x) + WINDOW_W / 2.0f;
        screen.y = -(WINDOW_H / 2.0f * ndc.y) + WINDOW_H / 2.0f;
        return true;
    }

    inline double PlayerDistance(const vec3& a, const vec3& b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    static int ResolveLocalSpottedBit(uintptr_t clientBase, uintptr_t localPawn) {
        if (!clientBase || !localPawn) return -1;
        uint32_t controllerHandle = memory::Read<uint32_t>(localPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_hController);
        if (!controllerHandle || controllerHandle == 0xFFFFFFFF) return -1;
        uint32_t controllerIndex = controllerHandle & 0x7FFF;
        if (controllerIndex == 0 || controllerIndex > 64) return -1;
        return static_cast<int>(controllerIndex - 1);
    }

    static bool IsSpottedByLocal(uintptr_t entity) {
        int bit = static_cast<int>(g_localSpottedBit);
        if (bit < 0 || bit >= 64) {
            return memory::Read<bool>(entity + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_entitySpottedState
                + cs2_dumper::schemas::client_dll::EntitySpottedState_t::m_bSpotted);
        }
        uint64_t mask = memory::Read<uint64_t>(entity + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_entitySpottedState
            + cs2_dumper::schemas::client_dll::EntitySpottedState_t::m_bSpottedByMask);
        return (mask & (1ULL << bit)) != 0;
    }

    // ★ 定义（前置声明在上面）
    static uintptr_t ResolveEntityHandle(uintptr_t entityList, uint32_t handle) {
        if (!entityList || !handle || handle == 0xFFFFFFFF) return 0;
        uint32_t entityIndex = handle & 0x7FFF;
        uintptr_t listEntry = memory::Read<uintptr_t>(entityList + 0x10 + 0x8 * (entityIndex >> 9));
        if (!listEntry) return 0;
        return memory::Read<uintptr_t>(listEntry + 0x70 * (entityIndex & 0x1FF));
    }

    // ★ 定义（前置声明在上面）
    static bool TryGetEntityPosition(uintptr_t entity, vec3& position) {
        if (!entity) return false;
        uintptr_t sceneNode = memory::Read<uintptr_t>(entity + cs2_dumper::schemas::client_dll::C_BaseEntity::m_pGameSceneNode);
        if (!sceneNode) return false;
        vec3 value = memory::Read<vec3>(sceneNode + cs2_dumper::schemas::client_dll::CGameSceneNode::m_vecAbsOrigin);
        constexpr float coordinateLimit = 1000000.0f;
        if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)
            || std::abs(value.x) > coordinateLimit || std::abs(value.y) > coordinateLimit || std::abs(value.z) > coordinateLimit) return false;
        position = value;
        return true;
    }

    static vec3 Vec3Cross(const vec3& a, const vec3& b) {
        return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
    }

    static void AngleVectors(const vec2& angles, vec3& forward, vec3& right, vec3& up) {
        float pitch = angles.x * (3.14159265f / 180.0f);
        float yaw = angles.y * (3.14159265f / 180.0f);
        float sp = std::sin(pitch), cp = std::cos(pitch);
        float sy = std::sin(yaw), cy = std::cos(yaw);
        forward = { cp * cy, cp * sy, -sp };
        right = { -sy, cy, 0.0f };
        up = Vec3Cross(right, forward);
    }

    static float BezierSmooth(float value) { return value * value * (3.0f - 2.0f * value); }

    static bool IsThirdPersonActive() {
        int style = g_MenuData.thirdPersonStyle;
        if (style == 2) return true;
        int keyIndex = g_MenuData.thirdPersonKey;
        if (keyIndex < 0 || keyIndex >= static_cast<int>(sizeof(thirdPersonKeyMap) / sizeof(thirdPersonKeyMap[0]))) return false;
        bool isDown = (GetAsyncKeyState(thirdPersonKeyMap[keyIndex]) & 0x8000) != 0;
        if (style == 1) return isDown;
        static bool toggleState = false;
        static bool wasDown = false;
        if (isDown && !wasDown) toggleState = !toggleState;
        wasDown = isDown;
        return toggleState;
    }

    static bool GetThirdPersonCamera(const vec3& viewAngles, float progress, vec3& cameraPos, vec3& target, vec2& cameraAngles) {
        uintptr_t client = memory::clientBase;
        if (!client) return false;
        uintptr_t localPawn = memory::Read<uintptr_t>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
        if (!localPawn) return false;
        int32_t health = memory::Read<int32_t>(localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
        uint8_t lifeState = memory::Read<uint8_t>(localPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_lifeState);
        if (health <= 0 || lifeState != 0) return false;
        vec3 feet = memory::Read<vec3>(localPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
        vec3 viewOffset = memory::Read<vec3>(localPawn + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset);
        target = feet + viewOffset;
        if (!std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z)) return false;
        vec3 forward, right, up;
        AngleVectors({ -viewAngles.x, viewAngles.y }, forward, right, up);
        float distance = std::clamp(g_MenuData.fThirdPersonDist, 50.0f, 500.0f);
        float distanceFactor = g_MenuData.bThirdPersonNoInterp ? 1.0f : BezierSmooth(progress);
        cameraPos = target - forward * (distance * distanceFactor);
        cameraAngles = CalcAngle(cameraPos, target);
        cameraAngles.x = std::clamp(NormalizeAngle(cameraAngles.x), -89.0f, 89.0f);
        cameraAngles.y = NormalizeAngle(cameraAngles.y);
        return true;
    }

    void RunThirdPerson() {
        if (!g_MenuData.bThirdPerson || g_ThirdPersonHookReady) return;
        ULONGLONG now = GetTickCount64();
        if (now - g_lastThirdPersonHookAttempt < 1000) return;
        g_lastThirdPersonHookAttempt = now;
        InitThirdPersonHook();
    }

    void UpdateEntities() {
        frameCounter++;
        int updateInterval = g_MenuData.performance.updateInterval;
        if (frameCounter - lastUpdateFrame < updateInterval) return;
        lastUpdateFrame = frameCounter;

        localPlayer.isValid = false;
        uintptr_t client = memory::clientBase;
        if (!client) { ClearRuntimeCache(); return; }

        uintptr_t localPlayerPawn = memory::Read<uintptr_t>(client + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
        if (!localPlayerPawn || !memory::IsReadable(localPlayerPawn, sizeof(uintptr_t))) { ClearRuntimeCache(); return; }

        if (g_MenuData.bFOVChanger) {
            uintptr_t cameraServices = memory::Read<uintptr_t>(localPlayerPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pCameraServices);
            if (cameraServices && memory::IsReadable(cameraServices, 0x300)) {
                memory::Write<int32_t>(cameraServices + cs2_dumper::schemas::client_dll::CCSPlayerBase_CameraServices::m_iFOV, (int32_t)g_MenuData.fFOVValue);
                memory::Write<int32_t>(cameraServices + cs2_dumper::schemas::client_dll::CCSPlayerBase_CameraServices::m_iFOVStart, (int32_t)g_MenuData.fFOVValue);
            }
        }

        uintptr_t entitySystem = memory::Read<uintptr_t>(client + cs2_dumper::offsets::client_dll::dwGameEntitySystem);
        if (!entitySystem || !memory::IsReadable(entitySystem, sizeof(uintptr_t))) { ClearRuntimeCache(); return; }
        uint32_t highestEntityIndex = memory::Read<uint32_t>(entitySystem + cs2_dumper::offsets::client_dll::dwGameEntitySystem_highestEntityIndex);
        if (highestEntityIndex == 0 || highestEntityIndex > 0x10000) { ClearRuntimeCache(); return; }

        viewMatrix localVm = memory::Read<viewMatrix>(client + cs2_dumper::offsets::client_dll::dwViewMatrix);
        if (localVm.m[0] == 0 && localVm.m[5] == 0) { ClearRuntimeCache(); return; }

        vec3 localPos = memory::Read<vec3>(localPlayerPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
        vec3 viewOffset = memory::Read<vec3>(localPlayerPawn + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset);
        vec3 localEyeAngles = memory::Read<vec3>(localPlayerPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_angEyeAngles);
        vec3 eyePos = { localPos.x + viewOffset.x, localPos.y + viewOffset.y, localPos.z + viewOffset.z };

        g_localSpottedBit = ResolveLocalSpottedBit(client, localPlayerPawn);
        UpdateSilentAimCache();

        {
            std::lock_guard<std::mutex> lock(dataMutex);
            vm = localVm;
            localPlayer.pawn = localPlayerPawn;
            localPlayer.position = localPos;
            localPlayer.eyePosition = eyePos;
            localPlayer.viewAngle = { localEyeAngles.x, localEyeAngles.y };
            localPlayer.spottedBit = static_cast<int32_t>(g_localSpottedBit);
            localPlayer.isValid = true;
        }

        uintptr_t entity_list = memory::Read<uintptr_t>(client + cs2_dumper::offsets::client_dll::dwEntityList);
        if (!entity_list || !memory::IsReadable(entity_list, sizeof(uintptr_t))) { ClearRuntimeCache(); return; }

        uint8_t myTeam = memory::Read<uint8_t>(localPlayerPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);

        if (g_MenuData.bAntiFlash && myTeam >= 2 && myTeam <= 3) {
            memory::Write<float>(localPlayerPawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawnBase::m_flFlashDuration, 0.0f);
        }

        float maxDist = g_MenuData.performance.enableDistanceFilter ?
            g_MenuData.performance.maxDrawDistance : 15000.0f;

        bool c4IsPlanted = false;
        bool c4IsDefusing = false;
        vec3 c4Pos = { 0,0,0 };
        int c4BombSite = 0;

        if (g_MenuData.bC4ESP) {
            constexpr uintptr_t plantedC4Next = 0x1198;
            uintptr_t plantedC4 = memory::Read<uintptr_t>(client + cs2_dumper::offsets::client_dll::dwPlantedC4);
            for (int i = 0; plantedC4 && i < 8; ++i) {
                bool ticking = memory::Read<bool>(plantedC4 + cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBombTicking);
                bool defused = memory::Read<bool>(plantedC4 + cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBombDefused);
                bool exploded = memory::Read<bool>(plantedC4 + cs2_dumper::schemas::client_dll::C_PlantedC4::m_bHasExploded);
                if (ticking && !defused && !exploded && TryGetEntityPosition(plantedC4, c4Pos)) {
                    c4IsPlanted = true;
                    c4IsDefusing = memory::Read<bool>(plantedC4 + cs2_dumper::schemas::client_dll::C_PlantedC4::m_bBeingDefused);
                    c4BombSite = memory::Read<int32_t>(plantedC4 + cs2_dumper::schemas::client_dll::C_PlantedC4::m_nBombSite);
                    break;
                }
                plantedC4 = memory::Read<uintptr_t>(plantedC4 + plantedC4Next);
            }
        }

        int enemyCount = 0;
        std::unordered_set<uintptr_t> seenPlayerEntities;

        for (uint32_t i = 1; i < 64 && enemyCount < 64; i++) {
            uintptr_t listEntry = memory::Read<uintptr_t>(entity_list + 0x10 + 0x8 * (i >> 9));
            if (!listEntry) continue;
            uintptr_t entityController = memory::Read<uintptr_t>(listEntry + 0x70 * (i & 0x1FF));
            if (!entityController) continue;
            uint32_t pawnHandle = memory::Read<uint32_t>(entityController + cs2_dumper::schemas::client_dll::CBasePlayerController::m_hPawn);
            if (!pawnHandle || pawnHandle == 0xFFFFFFFF) continue;
            uint32_t pawnIndex = pawnHandle & 0x7FFF;
            uintptr_t pawnListEntry = memory::Read<uintptr_t>(entity_list + 0x10 + 0x8 * (pawnIndex >> 9));
            if (!pawnListEntry) continue;
            uintptr_t entity = memory::Read<uintptr_t>(pawnListEntry + 0x70 * (pawnIndex & 0x1FF));
            if (!entity || entity == localPlayerPawn) continue;

            uint8_t team = memory::Read<uint8_t>(entity + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
            int32_t health = memory::Read<int32_t>(entity + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
            bool isFriend = (team == myTeam);
            bool isSpotted = IsSpottedByLocal(entity);
            seenPlayerEntities.insert(entity);

            bool shouldGlow = g_MenuData.bGlowESP && !isFriend && health > 0;
            if (shouldGlow) {
                ImVec4 glowColor = g_MenuData.bGlowSeparateColors
                    ? (isSpotted ? g_MenuData.GlowColorVisible : g_MenuData.GlowColorInvisible)
                    : g_MenuData.GlowColor;
                if (SetEntityGlow(entity, true, glowColor)) g_glowingEntities.insert(entity);
            }
            else if (g_glowingEntities.erase(entity) > 0) SetEntityGlow(entity, false, {});

            if (health <= 0) continue;

            vec3 feetPos = memory::Read<vec3>(entity + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
            float distance = static_cast<float>(PlayerDistance(eyePos, feetPos));
            if (distance > maxDist) continue;
            if (isFriend && g_MenuData.bRemoveFriends) continue;

            bool hasC4 = false;
            int32_t armor = memory::Read<int32_t>(entity + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_ArmorValue);
            vec3 vOffset = memory::Read<vec3>(entity + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset);

            float duckAmount = 0.0f;
            uintptr_t movementServices = memory::Read<uintptr_t>(entity + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pMovementServices);
            if (movementServices) {
                duckAmount = memory::Read<float>(movementServices + cs2_dumper::schemas::client_dll::CCSPlayer_MovementServices::m_flDuckAmount);
            }

            vec3 headPos = { feetPos.x + vOffset.x, feetPos.y + vOffset.y, feetPos.z + vOffset.z };
            headPos.z -= duckAmount * vOffset.z * 0.6f;

            std::string weaponName = "Unknown";
            if (g_MenuData.bWeaponESP) {
                uintptr_t weaponServices = memory::Read<uintptr_t>(entity + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pWeaponServices);
                if (weaponServices) {
                    uint32_t activeWeaponHandle = memory::Read<uint32_t>(weaponServices + cs2_dumper::schemas::client_dll::CPlayer_WeaponServices::m_hActiveWeapon);
                    if (activeWeaponHandle && activeWeaponHandle != 0xFFFFFFFF) {
                        uint32_t weaponIndex = activeWeaponHandle & 0x7FFF;
                        uintptr_t weaponListEntry = memory::Read<uintptr_t>(entity_list + 0x10 + 0x8 * (weaponIndex >> 9));
                        if (weaponListEntry) {
                            uintptr_t weaponEntity = memory::Read<uintptr_t>(weaponListEntry + 0x70 * (weaponIndex & 0x1FF));
                            if (weaponEntity) {
                                uintptr_t attrMgr = weaponEntity + cs2_dumper::schemas::client_dll::C_EconEntity::m_AttributeManager;
                                uint16_t itemDef = memory::Read<uint16_t>(attrMgr + cs2_dumper::schemas::client_dll::C_AttributeContainer::m_Item + cs2_dumper::schemas::client_dll::C_EconItemView::m_iItemDefinitionIndex);
                                weaponName = GetWeaponNameCached(itemDef);
                            }
                        }
                    }
                }
            }

            EnemyInfo& enemy = enemyBuffer[enemyCount];
            enemy.pawn = entity;
            enemy.position = feetPos;
            enemy.headPosition = headPos;
            enemy.health = health;
            enemy.armor = armor;
            enemy.distance = distance;
            enemy.weaponName = weaponName;
            enemy.isSpotted = isSpotted;
            enemy.isFriend = isFriend;
            enemy.hasC4 = hasC4;
            enemyCount++;
        }

        for (auto it = g_glowingEntities.begin(); it != g_glowingEntities.end();) {
            if (seenPlayerEntities.find(*it) == seenPlayerEntities.end()) {
                SetEntityGlow(*it, false, {});
                it = g_glowingEntities.erase(it);
            }
            else ++it;
        }

        {
            std::lock_guard<std::mutex> lock(dataMutex);
            enemies.clear();
            enemies.reserve(enemyCount);
            for (int i = 0; i < enemyCount; i++) enemies.push_back(enemyBuffer[i]);

            if (c4IsPlanted) {
                c4Info.exists = true;
                c4Info.isPlanted = true;
                c4Info.position = c4Pos;
                c4Info.isDefusing = c4IsDefusing;
                c4Info.bombSite = c4BombSite;
                c4Info.isCarried = false;
                c4Info.isDropped = false;
                c4Info.carrierPawn = 0;
            }
            else {
                c4Info.isPlanted = false;
                c4Info.isDefusing = false;
                c4Info.bombSite = 0;
            }
        }

        UpdateGrenades();
        UpdateGroundItems();
    }

    void RunAimbot() {
        hasAimTarget = false;
        if (!g_MenuData.bAutoAim) return;
        int selectedKey = aimKeyMap[g_MenuData.aimKey];
        if (!(GetAsyncKeyState(selectedKey) & 0x8000)) return;
        if (!localPlayer.isValid) return;
        const vec3& eyePos = localPlayer.eyePosition;
        float currentPitch = memory::Read<float>(memory::clientBase + cs2_dumper::offsets::client_dll::dwViewAngles);
        float currentYaw = memory::Read<float>(memory::clientBase + cs2_dumper::offsets::client_dll::dwViewAngles + 0x4);
        vec2 viewAngle = { currentPitch, currentYaw };
        float bestFOV = g_MenuData.fAimFOV;
        vec2 bestAngle = { 0.0f, 0.0f };
        bool found = false;
        vec3 targetPos = { 0,0,0 };
        uintptr_t entityList = memory::Read<uintptr_t>(memory::clientBase + cs2_dumper::offsets::client_dll::dwEntityList);
        if (!entityList) return;
        uint8_t myTeam = memory::Read<uint8_t>(localPlayer.pawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);

        for (int i = 1; i < 64; i++) {
            uintptr_t listEntry = memory::Read<uintptr_t>(entityList + 0x10 + 0x8 * (i >> 9));
            if (!listEntry) continue;
            uintptr_t entityController = memory::Read<uintptr_t>(listEntry + 0x70 * (i & 0x1FF));
            if (!entityController) continue;
            uint32_t pawnHandle = memory::Read<uint32_t>(entityController + cs2_dumper::schemas::client_dll::CBasePlayerController::m_hPawn);
            if (!pawnHandle || pawnHandle == 0xFFFFFFFF) continue;
            uint32_t pawnIndex = pawnHandle & 0x7FFF;
            uintptr_t pawnListEntry = memory::Read<uintptr_t>(entityList + 0x10 + 0x8 * (pawnIndex >> 9));
            if (!pawnListEntry) continue;
            uintptr_t enemyPawn = memory::Read<uintptr_t>(pawnListEntry + 0x70 * (pawnIndex & 0x1FF));
            if (!enemyPawn || enemyPawn == localPlayer.pawn) continue;
            int32_t health = memory::Read<int32_t>(enemyPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iHealth);
            if (health <= 0) continue;
            uint8_t enemyTeam = memory::Read<uint8_t>(enemyPawn + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
            bool isFriend = (enemyTeam == myTeam);
            if (!g_MenuData.bAimFriends && isFriend) continue;
            if (g_MenuData.bVisibleAim) {
                bool isSpotted = IsSpottedByLocal(enemyPawn);
                if (!isSpotted) continue;
            }
            vec3 feetPos = memory::Read<vec3>(enemyPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin);
            vec3 viewOffset = memory::Read<vec3>(enemyPawn + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset);
            float duckAmount = 0.0f;
            uintptr_t movementServices = memory::Read<uintptr_t>(enemyPawn + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pMovementServices);
            if (movementServices) duckAmount = memory::Read<float>(movementServices + cs2_dumper::schemas::client_dll::CCSPlayer_MovementServices::m_flDuckAmount);
            vec3 headPos = feetPos + viewOffset;
            headPos.z -= duckAmount * viewOffset.z * 0.6f;
            vec2 aim = CalcAngle(eyePos, headPos);
            float fov = GetFOV(viewAngle, aim);
            if (fov < bestFOV) { bestFOV = fov; bestAngle = aim; targetPos = headPos; found = true; }
        }

        if (found) { hasAimTarget = true; aimTargetPosition = targetPos; }
        if (!found) return;

        if (g_MenuData.bSilentAim) {
            if (!g_SilentAimHookReady) {
                vec3 compensatedAngle = { bestAngle.x, bestAngle.y, 0.0f };
                uintptr_t aimPunchServices = memory::Read<uintptr_t>(localPlayer.pawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_pAimPunchServices);
                if (aimPunchServices && memory::IsReadable(aimPunchServices, sizeof(vec3))) {
                    vec3 aimPunch = memory::Read<vec3>(aimPunchServices + cs2_dumper::schemas::client_dll::CCSPlayer_AimPunchServices::m_predictableBaseAngle);
                    compensatedAngle.x -= aimPunch.x * 2.0f;
                    compensatedAngle.y -= aimPunch.y * 2.0f;
                }
                uintptr_t csgoInput = memory::clientBase + cs2_dumper::offsets::client_dll::dwCSGOInput;
                const uintptr_t inputAnglesOffset = cs2_runtime::layouts::CCSGOInput_m_angViewAngles;
                if (memory::IsReadable(csgoInput + inputAnglesOffset, sizeof(vec3))) {
                    if (compensatedAngle.x > 89.0f) compensatedAngle.x = 89.0f;
                    if (compensatedAngle.x < -89.0f) compensatedAngle.x = -89.0f;
                    compensatedAngle.y = NormalizeAngle(compensatedAngle.y);
                    memory::Write<float>(csgoInput + inputAnglesOffset, compensatedAngle.x);
                    memory::Write<float>(csgoInput + inputAnglesOffset + sizeof(float), compensatedAngle.y);
                }
            }
        }
        else {
            vec2 compensatedTarget = bestAngle;
            uintptr_t aimPunchServices = memory::Read<uintptr_t>(localPlayer.pawn + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_pAimPunchServices);
            if (aimPunchServices) {
                vec3 aimPunch = memory::Read<vec3>(aimPunchServices + cs2_dumper::schemas::client_dll::CCSPlayer_AimPunchServices::m_predictableBaseAngle);
                compensatedTarget.x -= aimPunch.x * 2.0f;
                compensatedTarget.y -= aimPunch.y * 2.0f;
            }
            if (compensatedTarget.x > 89.0f) compensatedTarget.x = 89.0f;
            if (compensatedTarget.x < -89.0f) compensatedTarget.x = -89.0f;
            compensatedTarget.y = NormalizeAngle(compensatedTarget.y);
            float smooth = g_MenuData.fAimSmooth;
            if (smooth < 1.0f) smooth = 1.0f;
            float remainingPitch = NormalizeAngle(compensatedTarget.x - viewAngle.x);
            float remainingYaw = NormalizeAngle(compensatedTarget.y - viewAngle.y);
            const float arriveThreshold = 0.1f;
            float deltaPitch, deltaYaw;
            if (std::abs(remainingPitch) < arriveThreshold && std::abs(remainingYaw) < arriveThreshold) {
                deltaPitch = remainingPitch; deltaYaw = remainingYaw;
            }
            else {
                deltaPitch = remainingPitch / smooth;
                deltaYaw = remainingYaw / smooth;
            }
            float sensFactor = 0.022f;
            float moveX = -deltaYaw / sensFactor;
            float moveY = deltaPitch / sensFactor;
            float speed = g_MenuData.fAimSpeed;
            if (speed < 0.1f) speed = 0.1f;
            moveX *= speed; moveY *= speed;
            const float maxMove = 100.0f;
            if (moveX > maxMove) moveX = maxMove;
            if (moveX < -maxMove) moveX = -maxMove;
            if (moveY > maxMove) moveY = maxMove;
            if (moveY < -maxMove) moveY = -maxMove;
            INPUT input = {};
            input.type = INPUT_MOUSE;
            input.mi.dwFlags = MOUSEEVENTF_MOVE;
            input.mi.dx = static_cast<LONG>(moveX);
            input.mi.dy = static_cast<LONG>(moveY);
            SendInput(1, &input, sizeof(INPUT));
        }
    }

    void DrawESP(ImDrawList* drawList) {
        if (!drawList) return;

        if (g_MenuData.bGrenadeDebug) {
            char dbg[256];
            snprintf(dbg, sizeof(dbg),
                "DIAG: Main=%d Grn=%d UpdCalls=%d Tracked=%zu Hi=%u Drop=%d",
                (int)g_MenuData.bMainESP,
                (int)g_MenuData.bGrenadeESP,
                g_debugUpdateCalls,
                g_debugTrackedCount,
                g_debugHighestIndex,
                g_debugDroppedWeaponCount);
            drawList->AddText(ImVec2(10, 10), IM_COL32(255, 0, 0, 255), dbg);
        }

        if (!g_MenuData.bMainESP) return;

        static float halfW = WINDOW_W / 2.0f;
        static float halfH = WINDOW_H / 2.0f;
        static uint32_t lastW = 0, lastH = 0;
        if (lastW != WINDOW_W || lastH != WINDOW_H) {
            halfW = WINDOW_W / 2.0f;
            halfH = WINDOW_H / 2.0f;
            lastW = WINDOW_W;
            lastH = WINDOW_H;
        }

        if (g_MenuData.bShowAimLock && hasAimTarget) {
            vec2 screenPos;
            if (WorldToScreen(aimTargetPosition, screenPos, vm.m)) {
                ImU32 lockColor = IM_COL32(
                    (int)(g_MenuData.AimLockColor.x * 255),
                    (int)(g_MenuData.AimLockColor.y * 255),
                    (int)(g_MenuData.AimLockColor.z * 255),
                    (int)(g_MenuData.AimLockColor.w * 255)
                );
                drawList->AddLine(ImVec2(halfW, halfH), ImVec2(screenPos.x, screenPos.y), lockColor, 2.0f);
                drawList->AddCircle(ImVec2(screenPos.x, screenPos.y), 10.0f, lockColor, 12, 2.0f);
            }
        }

        if (g_MenuData.bAimScope) {
            float cx = WINDOW_W / 2.0f;
            float cy = WINDOW_H / 2.0f;
            float fovRad = g_MenuData.fAimFOV * (3.14159265f / 180.0f);
            float radius = tanf(fovRad) * (WINDOW_H / 2.0f);
            ImU32 scopeColor = IM_COL32(255, 255, 255, 120);
            drawList->AddCircle(ImVec2(cx, cy), radius, scopeColor, 32, 1.5f);
            drawList->AddLine(ImVec2(cx - 10, cy), ImVec2(cx + 10, cy), scopeColor, 1.0f);
            drawList->AddLine(ImVec2(cx, cy - 10), ImVec2(cx, cy + 10), scopeColor, 1.0f);
        }

        std::vector<EnemyInfo> snapEnemies;
        viewMatrix snapVm;
        C4Info snapC4;
        vec3 snapLocalEye;
        std::vector<GrenadeInfo> snapGrenades;
        std::vector<DroppedWeaponInfo> snapDroppedWeapons;
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            snapEnemies = enemies;
            snapVm = vm;
            snapC4 = c4Info;
            snapLocalEye = localPlayer.eyePosition;
            snapGrenades = grenades;
            snapDroppedWeapons = droppedWeapons;
        }

        ImU32 boxColorVisible = IM_COL32((int)(g_MenuData.BoxColorVisible.x * 255), (int)(g_MenuData.BoxColorVisible.y * 255), (int)(g_MenuData.BoxColorVisible.z * 255), (int)(g_MenuData.BoxColorVisible.w * 255));
        ImU32 boxColorInvisible = IM_COL32((int)(g_MenuData.BoxColorInvisible.x * 255), (int)(g_MenuData.BoxColorInvisible.y * 255), (int)(g_MenuData.BoxColorInvisible.z * 255), (int)(g_MenuData.BoxColorInvisible.w * 255));
        ImU32 rayColorVisible = IM_COL32((int)(g_MenuData.RayColorVisible.x * 255), (int)(g_MenuData.RayColorVisible.y * 255), (int)(g_MenuData.RayColorVisible.z * 255), (int)(g_MenuData.RayColorVisible.w * 255));
        ImU32 rayColorInvisible = IM_COL32((int)(g_MenuData.RayColorInvisible.x * 255), (int)(g_MenuData.RayColorInvisible.y * 255), (int)(g_MenuData.RayColorInvisible.z * 255), (int)(g_MenuData.RayColorInvisible.w * 255));
        ImU32 textColor = IM_COL32(255, 255, 255, 255);
        ImU32 c4Color = IM_COL32(255, 165, 0, 255);

        bool enableLOD = g_MenuData.performance.enableLOD;
        float lod1 = g_MenuData.performance.lodDistance1;
        float lod2 = g_MenuData.performance.lodDistance2;

        for (const auto& enemy : snapEnemies) {
            vec2 screenFeet, screenHead;
            if (!WorldToScreen(enemy.position, screenFeet, snapVm.m)) continue;
            if (!WorldToScreen(enemy.headPosition, screenHead, snapVm.m)) continue;
            float height = screenFeet.y - screenHead.y;
            float width = height / 2.5f;
            float x = screenHead.x - width / 2;
            float y = screenHead.y;
            ImU32 boxColor, rayColor;
            if (enemy.isFriend) {
                boxColor = IM_COL32(50, 100, 255, 255);
                rayColor = IM_COL32(50, 100, 255, 200);
            }
            else {
                boxColor = enemy.isSpotted ? boxColorVisible : boxColorInvisible;
                rayColor = enemy.isSpotted ? rayColorVisible : rayColorInvisible;
            }
            bool isFar = enemy.distance > lod2;
            bool isMid = enemy.distance > lod1 && enemy.distance <= lod2;

            if (isFar && enableLOD) {
                if (g_MenuData.bBoxESP) drawList->AddRect(ImVec2(x, y), ImVec2(x + width, y + height), boxColor, 0.0f, 0, 1.5f);
                continue;
            }
            if (g_MenuData.bBoxESP) drawList->AddRect(ImVec2(x, y), ImVec2(x + width, y + height), boxColor, 0.0f, 0, 2.0f);

            if (isMid && enableLOD) {
                if (g_MenuData.bHealthESP) {
                    int barWidth = 4, barHeight = (int)height;
                    int barX = (int)x - 6, barY = (int)y;
                    drawList->AddRectFilled(ImVec2((float)barX, (float)barY), ImVec2((float)(barX + barWidth), (float)(barY + barHeight)), IM_COL32(50, 50, 50, 200));
                    int healthHeight = (int)((enemy.health / 100.0f) * barHeight);
                    int healthY = barY + (barHeight - healthHeight);
                    float healthRatio = enemy.health / 100.0f;
                    uint8_t healthR = (uint8_t)(255 * (1.0f - healthRatio));
                    uint8_t healthG = (uint8_t)(255 * healthRatio);
                    drawList->AddRectFilled(ImVec2((float)barX, (float)healthY), ImVec2((float)(barX + barWidth), (float)(barY + barHeight)), IM_COL32(healthR, healthG, 0, 255));
                }
                continue;
            }

            if (g_MenuData.bHealthESP) {
                int barWidth = 4, barHeight = (int)height;
                int barX = (int)x - 6, barY = (int)y;
                drawList->AddRectFilled(ImVec2((float)barX, (float)barY), ImVec2((float)(barX + barWidth), (float)(barY + barHeight)), IM_COL32(50, 50, 50, 200));
                int healthHeight = (int)((enemy.health / 100.0f) * barHeight);
                int healthY = barY + (barHeight - healthHeight);
                float healthRatio = enemy.health / 100.0f;
                uint8_t healthR = (uint8_t)(255 * (1.0f - healthRatio));
                uint8_t healthG = (uint8_t)(255 * healthRatio);
                drawList->AddRectFilled(ImVec2((float)barX, (float)healthY), ImVec2((float)(barX + barWidth), (float)(barY + barHeight)), IM_COL32(healthR, healthG, 0, 255));
            }

            if (g_MenuData.bArmorESP) {
                int barWidth = 4, barHeight = (int)height;
                int barX = (int)(x + width) + 2, barY = (int)y;
                drawList->AddRectFilled(ImVec2((float)barX, (float)barY), ImVec2((float)(barX + barWidth), (float)(barY + barHeight)), IM_COL32(50, 50, 50, 200));
                int armorHeight = (int)((static_cast<float>(enemy.armor) / 100.0f) * barHeight);
                int armorY = barY + (barHeight - armorHeight);
                drawList->AddRectFilled(ImVec2((float)barX, (float)armorY), ImVec2((float)(barX + barWidth), (float)(barY + barHeight)), IM_COL32(0, 150, 255, 255));
            }

            if (g_MenuData.bRayESP) drawList->AddLine(ImVec2(static_cast<float>(WINDOW_W) / 2.0f, static_cast<float>(WINDOW_H)), ImVec2(screenFeet.x, screenFeet.y), rayColor, g_MenuData.RayThickness);
            if (g_MenuData.bWeaponESP) drawList->AddText(ImVec2(x + width / 2 - 20, y - 18), textColor, enemy.weaponName.c_str());
            if (g_MenuData.bDistanceESP) {
                char dist[32];
                snprintf(dist, sizeof(dist), "%.0fm", enemy.distance / 100.0f);
                drawList->AddText(ImVec2(x + width / 2 - 10, y + height + 2), textColor, dist);
            }
            if (g_MenuData.bC4ESP && enemy.hasC4) {
                drawList->AddText(ImVec2(x + width / 2 - 8, y - 32), c4Color, "C4");
                drawList->AddCircleFilled(ImVec2(x + width / 2, y + height / 2), 5.0f, c4Color);
            }
        }

        // ===== 掉落武器 ESP =====
        if (g_MenuData.bWeaponESP) {
            for (const auto& w : snapDroppedWeapons) {
                vec2 screenPos;
                if (!WorldToScreen(w.position, screenPos, snapVm.m)) continue;

                float dist = static_cast<float>(PlayerDistance(snapLocalEye, w.position));

                ImU32 color = IM_COL32(200, 200, 200, 255);
                const char* prefix = g_MenuData.language == 0 ? "[G] " : "[地] ";

                char label[80];
                snprintf(label, sizeof(label), "%s%s %.0fm", prefix, w.weaponName.c_str(), dist / 100.0f);

                ImVec2 textSize = ImGui::CalcTextSize(label);
                ImVec2 textPos(screenPos.x - textSize.x / 2, screenPos.y - 22);

                drawList->AddRectFilled(
                    ImVec2(textPos.x - 4, textPos.y - 2),
                    ImVec2(textPos.x + textSize.x + 4, textPos.y + textSize.y + 2),
                    IM_COL32(0, 0, 0, 150), 3.0f
                );
                drawList->AddText(textPos, color, label);

                drawList->AddRect(
                    ImVec2(screenPos.x - 5.0f, screenPos.y - 5.0f),
                    ImVec2(screenPos.x + 5.0f, screenPos.y + 5.0f),
                    color, 0.0f, 0, 1.5f
                );
            }
        }

        // ===== C4 ESP =====
        if (g_MenuData.bC4ESP) {
            if (snapC4.isPlanted) {
                vec2 screenPos;
                if (WorldToScreen(snapC4.position, screenPos, snapVm.m)) {
                    drawList->AddText(ImVec2(screenPos.x - 20, screenPos.y - 20), c4Color, "BOMB");
                    drawList->AddCircle(ImVec2(screenPos.x, screenPos.y), 15.0f, c4Color, 16, 2.0f);
                    if (snapC4.isDefusing) drawList->AddText(ImVec2(screenPos.x - 30, screenPos.y + 20), IM_COL32(0, 255, 0, 255), "DEFUSING");
                }
            }
            else if (snapC4.isDropped && snapC4.exists) {
                vec2 screenPos;
                if (WorldToScreen(snapC4.position, screenPos, snapVm.m)) {
                    float dist = static_cast<float>(PlayerDistance(snapLocalEye, snapC4.position));
                    char c4Text[64];
                    snprintf(c4Text, sizeof(c4Text), "C4 %.0fm", dist / 100.0f);
                    drawList->AddText(ImVec2(screenPos.x - 20, screenPos.y - 10), c4Color, c4Text);
                    float radius = 8.0f;
                    drawList->AddQuad(ImVec2(screenPos.x, screenPos.y - radius), ImVec2(screenPos.x + radius, screenPos.y), ImVec2(screenPos.x, screenPos.y + radius), ImVec2(screenPos.x - radius, screenPos.y), c4Color, 2.0f);
                }
            }
        }

        // ===== 投掷物 ESP =====
        if (g_MenuData.bGrenadeESP) {
            if (g_MenuData.bGrenadeDebug) {
                char dbg[160];
                size_t tracked = 0;
                int totalNames = 0, grenadeNames = 0, projectileNames = 0;
                int carriedCount = 0, thrownCount = 0;
                std::vector<std::string> namesCopy;
                std::vector<std::pair<std::string, float>> recentCopy;
                {
                    std::lock_guard<std::mutex> lock(dataMutex);
                    tracked = g_debugTrackedCount;
                    totalNames = g_debugTotalNames;
                    grenadeNames = g_debugGrenadeNames;
                    projectileNames = g_debugProjectileNames;
                    carriedCount = g_debugCarriedCount;
                    thrownCount = g_debugThrownCount;
                    namesCopy = g_debugEntityNames;
                    recentCopy = g_recentNewEntities;
                }

                snprintf(dbg, sizeof(dbg),
                    "GrenadeESP ON | tracked=%zu | total=%d gren=%d proj=%d | carried=%d thrown=%d",
                    tracked, totalNames, grenadeNames, projectileNames, carriedCount, thrownCount);
                drawList->AddText(ImVec2(10, 40), IM_COL32(0, 255, 0, 255), dbg);

                float yNew = 60.0f;
                for (const auto& pair : recentCopy) {
                    std::string line = "NEW: " + pair.first;
                    drawList->AddText(ImVec2(10, yNew), IM_COL32(0, 255, 255, 255), line.c_str());
                    yNew += 18.0f;
                    if (yNew > 300.0f) break;
                }

                float yOff = yNew + 5.0f;
                for (const auto& line : namesCopy) {
                    drawList->AddText(ImVec2(10, yOff), IM_COL32(255, 255, 0, 255), line.c_str());
                    yOff += 18.0f;
                    if (yOff > 900.0f) break;
                }
            }

            for (const auto& g : snapGrenades) {
                if (g.isThrown && !g_MenuData.bGrenadeThrown) continue;
                if (!g.isThrown && !g_MenuData.bGrenadeCarried) continue;

                vec3 displayPos = (g.isActive || !g.landingRecorded) ? g.position : g.landingPosition;
                vec2 screenPos;
                if (!WorldToScreen(displayPos, screenPos, snapVm.m)) continue;

                ImU32 color = GetGrenadeColor(g.type);
                const char* name = GetGrenadeName(g.type);
                const char* prefix = g.isThrown ? "" : (g_MenuData.language == 0 ? "[C] " : "[携] ");

                float alpha = 1.0f;
                if (!g.isActive) {
                    alpha = 1.0f - (g.disappearTimer / 1.5f);
                    if (alpha < 0.0f) alpha = 0.0f;
                    if (alpha > 1.0f) alpha = 1.0f;
                }
                int a = (color >> 24) & 0xFF;
                int r = (color >> 0) & 0xFF;
                int gg = (color >> 8) & 0xFF;
                int b = (color >> 16) & 0xFF;
                ImU32 fadedColor = IM_COL32(r, gg, b, static_cast<int>(a * alpha));

                float dist = static_cast<float>(PlayerDistance(snapLocalEye, displayPos));
                char label[64];
                snprintf(label, sizeof(label), "%s%s %.0fm", prefix, name, dist / 100.0f);

                ImVec2 textSize = ImGui::CalcTextSize(label);
                ImVec2 textPos(screenPos.x - textSize.x / 2, screenPos.y - 32);

                drawList->AddRectFilled(
                    ImVec2(textPos.x - 4, textPos.y - 2),
                    ImVec2(textPos.x + textSize.x + 4, textPos.y + textSize.y + 2),
                    IM_COL32(0, 0, 0, static_cast<int>(150 * alpha)), 3.0f
                );
                drawList->AddText(textPos, fadedColor, label);

                if (g.isActive) {
                    drawList->AddCircleFilled(ImVec2(screenPos.x, screenPos.y), 4.0f, fadedColor);
                    drawList->AddCircle(ImVec2(screenPos.x, screenPos.y), 8.0f, fadedColor, 16, 2.0f);
                }
                else {
                    drawList->AddCircle(ImVec2(screenPos.x, screenPos.y), 10.0f, fadedColor, 20, 2.0f);
                    drawList->AddCircle(ImVec2(screenPos.x, screenPos.y), 3.0f, fadedColor, 12, 1.5f);
                    drawList->AddLine(ImVec2(screenPos.x - 14.0f, screenPos.y), ImVec2(screenPos.x + 14.0f, screenPos.y), fadedColor, 1.5f);
                    drawList->AddLine(ImVec2(screenPos.x, screenPos.y - 14.0f), ImVec2(screenPos.x, screenPos.y + 14.0f), fadedColor, 1.5f);
                }
            }
        }
    }

    void* g_SilentAimOrigin = nullptr;
    void* g_SilentAimHookTarget = nullptr;
    using SilentAimFunc = int64_t(*)(int* a1, int64_t a2, char a3, int64_t arg1, int64_t arg2, int64_t arg3);

    static uintptr_t g_cachedClientBase = 0;
    static uintptr_t g_cachedEntityList = 0;
    static volatile LONG g_cacheValid = 0;

    inline uint64_t SafeReadU64(uintptr_t address) { if (address < 0x10000) return 0; __try { return *(uint64_t*)address; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }
    inline uint8_t SafeReadU8(uintptr_t address) { if (address < 0x10000) return 0; __try { return *(uint8_t*)address; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }
    inline uint32_t SafeReadU32(uintptr_t address) { if (address < 0x10000) return 0; __try { return *(uint32_t*)address; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; } }
    inline float SafeReadF32(uintptr_t address) { if (address < 0x10000) return 0.0f; __try { return *(float*)address; } __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; } }

    void UpdateSilentAimCache() {
        uintptr_t clientBase = memory::clientBase;
        if (!clientBase) { InterlockedExchange(&g_cacheValid, 0); return; }
        uint64_t entityList = SafeReadU64(clientBase + cs2_dumper::offsets::client_dll::dwEntityList);
        if (!entityList) { InterlockedExchange(&g_cacheValid, 0); return; }
        g_cachedClientBase = clientBase;
        g_cachedEntityList = entityList;
        InterlockedExchange(&g_cacheValid, 1);
    }

    uint64_t GetSilentAimPlayer(int i) {
        if (!g_cacheValid) return 0;
        if (i < 0 || i > 63) return 0;
        uint64_t listEntry = SafeReadU64(g_cachedEntityList + 0x10 + 0x8 * (i >> 9));
        if (!listEntry) return 0;
        uint64_t controller = SafeReadU64(listEntry + 0x70 * (i & 0x1FF));
        if (!controller) return 0;
        uint32_t playerPawnHandle = SafeReadU32(controller + cs2_dumper::schemas::client_dll::CBasePlayerController::m_hPawn);
        if (!playerPawnHandle) return 0;
        uint32_t pawnIndex = playerPawnHandle & 0x7FFF;
        uint64_t pawnListEntry = SafeReadU64(g_cachedEntityList + 0x10 + 0x8 * (pawnIndex >> 9));
        if (!pawnListEntry) return 0;
        return SafeReadU64(pawnListEntry + 0x70 * (pawnIndex & 0x1FF));
    }

    vec2 GetSilentAimTargetAngle(uint64_t playerPtr, uint64_t localPlayer) {
        if (!playerPtr || !localPlayer) return { 0.0f, 0.0f };
        vec3 targetFeet = { SafeReadF32(playerPtr + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin), SafeReadF32(playerPtr + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin + 4), SafeReadF32(playerPtr + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin + 8) };
        vec3 targetOffset = { SafeReadF32(playerPtr + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset), SafeReadF32(playerPtr + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset + 4), SafeReadF32(playerPtr + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset + 8) };
        vec3 localFeet = { SafeReadF32(localPlayer + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin), SafeReadF32(localPlayer + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin + 4), SafeReadF32(localPlayer + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_vOldOrigin + 8) };
        vec3 localOffset = { SafeReadF32(localPlayer + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset), SafeReadF32(localPlayer + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset + 4), SafeReadF32(localPlayer + cs2_dumper::schemas::client_dll::C_BaseModelEntity::m_vecViewOffset + 8) };
        float duckAmount = 0.0f;
        uint64_t movementServices = SafeReadU64(playerPtr + cs2_dumper::schemas::client_dll::C_BasePlayerPawn::m_pMovementServices);
        if (movementServices) duckAmount = SafeReadF32(movementServices + cs2_dumper::schemas::client_dll::CCSPlayer_MovementServices::m_flDuckAmount);
        vec3 targetHead = { targetFeet.x + targetOffset.x, targetFeet.y + targetOffset.y, targetFeet.z + targetOffset.z };
        targetHead.z -= duckAmount * targetOffset.z * 0.6f;
        vec3 localEye = { localFeet.x + localOffset.x, localFeet.y + localOffset.y, localFeet.z + localOffset.z };
        vec2 angle = CalcAngle(localEye, targetHead);
        angle.x = NormalizeAngle(angle.x);
        angle.y = NormalizeAngle(angle.y);
        uint64_t aimPunchServices = SafeReadU64(localPlayer + cs2_dumper::schemas::client_dll::C_CSPlayerPawn::m_pAimPunchServices);
        if (aimPunchServices) {
            float punchX = SafeReadF32(aimPunchServices + cs2_dumper::schemas::client_dll::CCSPlayer_AimPunchServices::m_predictableBaseAngle);
            float punchY = SafeReadF32(aimPunchServices + cs2_dumper::schemas::client_dll::CCSPlayer_AimPunchServices::m_predictableBaseAngle + 4);
            angle.x -= punchX * 2.0f;
            angle.y -= punchY * 2.0f;
        }
        return angle;
    }

    int64_t SilentAimHook(int* a1, int64_t a2, char a3, int64_t arg1, int64_t arg2, int64_t arg3) {
        __try {
            if (!a1) return ((SilentAimFunc)g_SilentAimOrigin)(a1, a2, a3, arg1, arg2, arg3);
            if (!a3) return ((SilentAimFunc)g_SilentAimOrigin)(a1, a2, a3, arg1, arg2, arg3);
            bool silentAimEnabled = g_MenuData.bSilentAim;
            bool visibleAim = g_MenuData.bVisibleAim;
            bool aimFriends = g_MenuData.bAimFriends;
            float aimFOV = g_MenuData.fAimFOV;
            int aimKey = g_MenuData.aimKey;
            if (!silentAimEnabled) return ((SilentAimFunc)g_SilentAimOrigin)(a1, a2, a3, arg1, arg2, arg3);
            if (aimKey < 0 || aimKey >= (int)(sizeof(aimKeyMap) / sizeof(aimKeyMap[0]))) return ((SilentAimFunc)g_SilentAimOrigin)(a1, a2, a3, arg1, arg2, arg3);
            int selectedKey = aimKeyMap[aimKey];
            if (!(GetAsyncKeyState(selectedKey) & 0x8000)) return ((SilentAimFunc)g_SilentAimOrigin)(a1, a2, a3, arg1, arg2, arg3);
            if (!g_cacheValid) return ((SilentAimFunc)g_SilentAimOrigin)(a1, a2, a3, arg1, arg2, arg3);
            uint64_t localPtr = SafeReadU64(g_cachedClientBase + cs2_dumper::offsets::client_dll::dwLocalPlayerPawn);
            if (!localPtr) return ((SilentAimFunc)g_SilentAimOrigin)(a1, a2, a3, arg1, arg2, arg3);
            uint8_t myTid = SafeReadU8(localPtr + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
            float viewAngleX = SafeReadF32(g_cachedClientBase + cs2_dumper::offsets::client_dll::dwViewAngles);
            float viewAngleY = SafeReadF32(g_cachedClientBase + cs2_dumper::offsets::client_dll::dwViewAngles + 0x4);
            float closedTarget = 999999.0f;
            float targetX = 0.0f, targetY = 0.0f;
            bool findOut = false;
            for (int i = 0; i < 64; i++) {
                uint64_t targetPtr = GetSilentAimPlayer(i);
                if (!targetPtr) continue;
                if (targetPtr == localPtr) continue;
                uint8_t targetTid = SafeReadU8(targetPtr + cs2_dumper::schemas::client_dll::C_BaseEntity::m_iTeamNum);
                if (targetTid == myTid && !aimFriends) continue;
                uint8_t lifeState = SafeReadU8(targetPtr + cs2_dumper::schemas::client_dll::C_BaseEntity::m_lifeState);
                if (lifeState != 0) continue;
                if (visibleAim) {
                    bool spotted = IsSpottedByLocal(static_cast<uintptr_t>(targetPtr));
                    if (!spotted) continue;
                }
                vec2 getAngle = GetSilentAimTargetAngle(targetPtr, localPtr);
                float diffX = NormalizeAngle(getAngle.x - viewAngleX);
                float diffY = NormalizeAngle(getAngle.y - viewAngleY);
                float dist = std::sqrt(diffX * diffX + diffY * diffY);
                if (dist < closedTarget && dist < aimFOV) {
                    findOut = true;
                    closedTarget = dist;
                    targetX = getAngle.x;
                    targetY = getAngle.y;
                }
            }
            if (findOut) {
                if (targetX > 89.0f) targetX = 89.0f;
                if (targetX < -89.0f) targetX = -89.0f;
                targetY = NormalizeAngle(targetY);
                a1[7] = *(int*)&targetX;
                a1[8] = *(int*)&targetY;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return ((SilentAimFunc)g_SilentAimOrigin)(a1, a2, a3, arg1, arg2, arg3);
    }

    bool InitSilentAimHook() {
        if (g_SilentAimOrigin) { g_SilentAimHookReady = true; return true; }
        try {
            g_SilentAimHookReady = false;
            g_SilentAimHookTarget = nullptr;
            void* hookTarget = Scanner::PatternScan("client.dll", cs2_patterns::WriteSubtickFromEntry);
            if (!hookTarget) return false;
            if (MH_CreateHook(hookTarget, SilentAimHook, &g_SilentAimOrigin) != MH_OK) return false;
            if (MH_EnableHook(hookTarget) != MH_OK) {
                MH_RemoveHook(hookTarget);
                g_SilentAimOrigin = nullptr;
                return false;
            }
            g_SilentAimHookTarget = hookTarget;
            g_SilentAimHookReady = true;
            return true;
        }
        catch (...) { g_SilentAimHookReady = false; return false; }
    }

    void CleanupSilentAimHook() {
        if (g_SilentAimHookTarget) {
            MH_DisableHook(g_SilentAimHookTarget);
            MH_RemoveHook(g_SilentAimHookTarget);
        }
        g_SilentAimHookTarget = nullptr;
        g_SilentAimOrigin = nullptr;
        g_SilentAimHookReady = false;
    }

    void* g_OverrideViewOrigin = nullptr;
    void* g_OverrideViewTarget = nullptr;
    using OverrideViewFn = void(__fastcall*)(void*, void*);

    void OverrideViewHook(void* thisptr, void* viewSetup) {
        __try {
            if (g_OverrideViewOrigin) ((OverrideViewFn)g_OverrideViewOrigin)(thisptr, viewSetup);
            if (!g_MenuData.bThirdPerson || !viewSetup) { g_ThirdPersonProgress = 0.0f; return; }
            bool active = IsThirdPersonActive();
            constexpr float transitionStep = 6.0f / 64.0f;
            g_ThirdPersonProgress = std::clamp(g_ThirdPersonProgress + (active ? transitionStep : -transitionStep), 0.0f, 1.0f);
            if (g_ThirdPersonProgress <= 0.0f) return;
            constexpr uintptr_t viewOriginOffset = cs2_runtime::layouts::CViewSetup_m_vecOrigin;
            constexpr uintptr_t viewAnglesOffset = cs2_runtime::layouts::CViewSetup_m_angViewAngles;
            vec3 viewAngles = memory::Read<vec3>((uintptr_t)viewSetup + viewAnglesOffset);
            vec3 cameraPos, target;
            vec2 cameraAngles;
            if (!GetThirdPersonCamera(viewAngles, g_ThirdPersonProgress, cameraPos, target, cameraAngles)) { g_ThirdPersonProgress = 0.0f; return; }
            vec3 angles = { cameraAngles.x, cameraAngles.y, 0.0f };
            memory::Write<vec3>((uintptr_t)viewSetup + viewOriginOffset, cameraPos);
            memory::Write<vec3>((uintptr_t)viewSetup + viewAnglesOffset, angles);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    bool InitThirdPersonHook() {
        if (g_OverrideViewOrigin) { g_ThirdPersonHookReady = true; return true; }
        try {
            void* hookTarget = Scanner::PatternScan("client.dll", cs2_patterns::OverrideView);
            if (!hookTarget) { g_ThirdPersonHookReady = false; return false; }
            if (MH_CreateHook(hookTarget, OverrideViewHook, &g_OverrideViewOrigin) != MH_OK) { g_ThirdPersonHookReady = false; return false; }
            if (MH_EnableHook(hookTarget) != MH_OK) {
                MH_RemoveHook(hookTarget);
                g_OverrideViewOrigin = nullptr;
                g_ThirdPersonHookReady = false;
                return false;
            }
            g_OverrideViewTarget = hookTarget;
            g_ThirdPersonHookReady = true;
            return true;
        }
        catch (...) { g_ThirdPersonHookReady = false; return false; }
    }

    void CleanupThirdPersonHook() {
        if (g_OverrideViewTarget) {
            MH_DisableHook(g_OverrideViewTarget);
            MH_RemoveHook(g_OverrideViewTarget);
        }
        g_OverrideViewTarget = nullptr;
        g_OverrideViewOrigin = nullptr;
        g_ThirdPersonProgress = 0.0f;
        g_ThirdPersonHookReady = false;
    }
}