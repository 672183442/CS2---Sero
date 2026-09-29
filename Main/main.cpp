#include <Windows.h>
#include <d3d11.h>
#include "..\minhook_debug_x64\include\MinHook.h"
#include "..\imgui_d11\imgui.h"
#include "..\imgui_d11\imgui_impl_win32.h"
#include "..\imgui_d11\imgui_impl_dx11.h"
#include "menu.h"
#include "ui.h"
#include "theme.h"
#include "effects.h"
#include "core/memory/memory.hpp"
#include "features/cheat.hpp"

#include <cmath>
#include <thread>
#include <chrono>

extern uint32_t WIDTH, HEIGHT, WINDOW_W, WINDOW_H;

static UI::AppState g_UIState;
static bool g_UIBound = false;

bool g_ShowMenu = true;
static int initFrameCount = 0;
static bool espReady = false;

static ID3D11Device* g_pd3dDevice = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11DeviceContext* g_pd3dContext = nullptr;
static ID3D11RenderTargetView* view = nullptr;
static HWND g_hwnd = nullptr;
void* origin_present = nullptr;
using Present = HRESULT(__stdcall*)(IDXGISwapChain*, UINT, UINT);
WNDPROC origin_wndProc;
WNDPROC origin_wndProc_saved = nullptr;
bool inited = false;
bool initOnce = false;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ============================================================
// 游戏逻辑
// ============================================================
static bool __declspec(nothrow) RunCheatLogicSafe()
{
    if (!cs2_runtime::IsInitialized()) return false;
    __try {
        Cheat::RunThirdPerson();
        Cheat::UpdateEntities();
        if (g_MenuData.bMainESP) {
            ImDrawList* drawList = ImGui::GetBackgroundDrawList();
            if (drawList) Cheat::DrawESP(drawList);
        }
        Cheat::RunAimbot();
        Cheat::RunBunnyHop();   // ← 新增
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

static HRESULT CallOriginalPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
{
    if (!origin_present) return DXGI_ERROR_INVALID_CALL;
    return ((Present)origin_present)(swapChain, syncInterval, flags);
}

static void ResetImGuiState()
{
    if (inited) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    if (view) { view->Release(); view = nullptr; }
    if (g_pd3dContext) { g_pd3dContext->Release(); g_pd3dContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
    inited = false;
    espReady = false;
    initFrameCount = 0;
}

LRESULT __stdcall WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_KEYDOWN && wParam == VK_F4)
        g_ShowMenu = !g_ShowMenu;

    if (uMsg == WM_SIZE || uMsg == WM_ACTIVATE) {
        RECT rect;
        if (GetClientRect(g_hwnd, &rect)) {
            WINDOW_W = rect.right - rect.left;
            WINDOW_H = rect.bottom - rect.top;
            WIDTH = WINDOW_W;
            HEIGHT = WINDOW_H;
        }
    }

    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
        return true;
    WNDPROC targetWndProc = origin_wndProc_saved ? origin_wndProc_saved : origin_wndProc;
    if (targetWndProc && targetWndProc != WndProc)
        return CallWindowProc(targetWndProc, hwnd, uMsg, wParam, lParam);
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

static CRITICAL_SECTION presentCriticalSection;
static bool csInitialized = false;

long __stdcall my_present(IDXGISwapChain* _this, UINT a, UINT b)
{
    if (!_this) return CallOriginalPresent(_this, a, b);

    if (!csInitialized) {
        InitializeCriticalSection(&presentCriticalSection);
        csInitialized = true;
    }

    EnterCriticalSection(&presentCriticalSection);

    __try {
        if (!inited)
        {
            ID3D11Device* currentDevice = nullptr;
            HRESULT hr = _this->GetDevice(__uuidof(ID3D11Device), (void**)&currentDevice);
            if (FAILED(hr) || !currentDevice) {
                LeaveCriticalSection(&presentCriticalSection);
                return CallOriginalPresent(_this, a, b);
            }

            g_pd3dDevice = currentDevice;
            g_pd3dDevice->AddRef();
            g_pd3dDevice->GetImmediateContext(&g_pd3dContext);
            if (!g_pd3dContext) {
                ResetImGuiState();
                LeaveCriticalSection(&presentCriticalSection);
                return CallOriginalPresent(_this, a, b);
            }

            DXGI_SWAP_CHAIN_DESC sd;
            hr = _this->GetDesc(&sd);
            if (FAILED(hr) || !sd.OutputWindow) {
                ResetImGuiState();
                LeaveCriticalSection(&presentCriticalSection);
                return CallOriginalPresent(_this, a, b);
            }
            g_hwnd = sd.OutputWindow;

            RECT rect;
            if (GetClientRect(g_hwnd, &rect)) {
                WINDOW_W = rect.right - rect.left;
                WINDOW_H = rect.bottom - rect.top;
                WIDTH = WINDOW_W;
                HEIGHT = WINDOW_H;
            }

            ID3D11Texture2D* buf = nullptr;
            hr = _this->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&buf);
            if (SUCCEEDED(hr) && buf) {
                hr = g_pd3dDevice->CreateRenderTargetView(buf, nullptr, &view);
                buf->Release();
                if (FAILED(hr)) view = nullptr;
            }

            origin_wndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
            if (!initOnce) { origin_wndProc_saved = origin_wndProc; initOnce = true; }

            ImGui::CreateContext();
            if (!ImGui::GetCurrentContext()) {
                ResetImGuiState();
                LeaveCriticalSection(&presentCriticalSection);
                return CallOriginalPresent(_this, a, b);
            }

            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

            if (!ImGui_ImplWin32_Init(g_hwnd) || !ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext)) {
                ResetImGuiState();
                LeaveCriticalSection(&presentCriticalSection);
                return CallOriginalPresent(_this, a, b);
            }

            {
                static const ImWchar cjkRanges[] = {
                    0x0020, 0x00FF,
                    0x2000, 0x206F,
                    0x3000, 0x30FF,
                    0x31F0, 0x31FF,
                    0xFF00, 0xFFEF,
                    0x4E00, 0x9FFF,
                    0,
                };

                ImFont* font = nullptr;
                const char* cnFontPaths[] = {
                    "C:\\Windows\\Fonts\\msyh.ttc",
                    "C:\\Windows\\Fonts\\simsun.ttc",
                    "C:\\Windows\\Fonts\\simhei.ttf",
                    "C:\\Windows\\Fonts\\msyhbd.ttc",
                };
                for (const char* path : cnFontPaths) {
                    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
                        font = io.Fonts->AddFontFromFileTTF(path, 18.0f, nullptr, cjkRanges);
                        if (font) break;
                    }
                }
                if (!font) io.Fonts->AddFontDefault();
            }



            inited = true;
            initFrameCount = 0;
            espReady = false;
        }

        if (!g_pd3dDevice || !g_pd3dContext) {
            LeaveCriticalSection(&presentCriticalSection);
            return CallOriginalPresent(_this, a, b);
        }



        // ★ 右 Shift 边缘检测切换菜单
        {
            static bool prevRShift = false;
            bool nowRShift = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
            if (nowRShift && !prevRShift) {
                g_ShowMenu = !g_ShowMenu;
            }
            prevRShift = nowRShift;
        }

        // ★ 菜单显示/隐藏动画：从中心向外放大 / 向中心缩小
        {
            float target = g_ShowMenu ? 1.0f : 0.0f;
            float dt = ImGui::GetIO().DeltaTime;
            if (dt <= 0.0f || dt > 0.2f) dt = 0.016f;

            float speed = 18.0f;
            float t = 1.0f - expf(-speed * dt);
            g_UIState.menuScale += (target - g_UIState.menuScale) * t;

            if (g_UIState.menuScale < 0.005f) g_UIState.menuScale = 0.0f;
            if (g_UIState.menuScale > 0.995f) g_UIState.menuScale = 1.0f;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        Theme::Palette pal = Theme::Lerp(g_UIState.themeT);
        Theme::ApplyStyle(pal);

        if (g_UIState.menuScale > 0.005f) {
            if (!g_UIBound) {
                g_UIState.menu = &g_MenuData;
                g_UIBound = true;
            }
            UI::Draw(g_UIState, pal, ImGui::GetIO().DeltaTime, nullptr);
        }
        if (initFrameCount < 5) {
            initFrameCount++;
        }
        else {
            espReady = true;
        }

        if (espReady) {
            if (!RunCheatLogicSafe()) {
                espReady = false;
                initFrameCount = 0;
            }
        }

        RenderHUD();
        ImGui::Render();

        if (!view) {
            ID3D11Texture2D* buf = nullptr;
            HRESULT hr = _this->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&buf);
            if (SUCCEEDED(hr) && buf) {
                if (g_pd3dDevice) g_pd3dDevice->CreateRenderTargetView(buf, nullptr, &view);
                buf->Release();
            }
        }

        if (view && g_pd3dContext) {
            g_pd3dContext->OMSetRenderTargets(1, &view, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        ResetImGuiState();
    }

    LeaveCriticalSection(&presentCriticalSection);

    HRESULT presentResult = CallOriginalPresent(_this, a, b);

    if (presentResult == DXGI_ERROR_DEVICE_REMOVED || presentResult == DXGI_ERROR_DEVICE_RESET) {
        ResetImGuiState();
    }

    return presentResult;
}

DWORD WINAPI create(LPVOID)
{
    const MH_STATUS minHookStatus = MH_Initialize();
    if (minHookStatus != MH_OK && minHookStatus != MH_ERROR_ALREADY_INITIALIZED) return 0;

    const unsigned level_count = 2;
    D3D_FEATURE_LEVEL levels[level_count] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 1;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = GetForegroundWindow();
    sd.SampleDesc.Count = 1;
    sd.Windowed = true;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    IDXGISwapChain* tempSwap = nullptr;
    ID3D11Device* tempDev = nullptr;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
        levels, level_count, D3D11_SDK_VERSION,
        &sd, &tempSwap, &tempDev, nullptr, nullptr
    );

    if (tempSwap) {
        void*** vtable_ptr = (void***)tempSwap;
        void** vtable = *vtable_ptr;
        void* presentFunc = vtable[8];
        if (MH_CreateHook(presentFunc, my_present, &origin_present) == MH_OK) {
            MH_EnableHook(presentFunc);
        }
        tempDev->Release();
        tempSwap->Release();
    }

    for (int attempt = 0; attempt < 120; ++attempt) {
        memory::Initialize();
        if (Cheat::Init(memory::clientBase)) {
            Cheat::InitThirdPersonHook();
            Cheat::InitSilentAimAngleHook();   // ★ 新增：静默自瞄弹道 Hook
            break;
        }
        Sleep(500);
    }
    return 0;
}

BOOL __stdcall DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(hModule);
        if (HANDLE worker = CreateThread(NULL, 0, create, NULL, 0, NULL)) {
            CloseHandle(worker);
        }
        break;
    case DLL_PROCESS_DETACH:
        Cheat::CleanupSilentAimHook();
        Cheat::CleanupThirdPersonHook();
        Cheat::CleanupSilentAimAngleHook();    // ★ 新增
        if (origin_present) {
            MH_DisableHook(MH_ALL_HOOKS);
            MH_RemoveHook(MH_ALL_HOOKS);
        }
        MH_Uninitialize();
        if (inited) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplWin32_Shutdown();
            ImGui::DestroyContext();
        }
        if (view) { view->Release(); view = nullptr; }
        if (g_pd3dContext) { g_pd3dContext->Release(); g_pd3dContext = nullptr; }
        if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
        break;
    }
    return TRUE;
}