#include <Windows.h>
#include <d3d11.h>
#include "..\minhook_debug_x64\include\MinHook.h"
#include "..\imgui_d11\imgui.h"
#include "..\imgui_d11\imgui_impl_win32.h"
#include "..\imgui_d11\imgui_impl_dx11.h"
#include "ImGui_Menu.h"
#include "core/memory/memory.hpp"
#include "features/cheat.hpp"

#include <wininet.h>
#include <sstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")

extern uint32_t WIDTH, HEIGHT, WINDOW_W, WINDOW_H;

// ========== 软件版本定义 ==========
#define SOFTWARE_VERSION "202607021"  // 当前软件版本，格式：YYYYMMDD

// ========== 下载链接配置 ==========
#define DOWNLOAD_URL "https://qncy.lanzoue.com/b004jj7lfc"
#define DOWNLOAD_PASSWORD "Sero"

// ================================================================
// ========== 新增：全局网络开关 ==========
// 1 = 启用网络请求（版本、公告、维护检查）
// 0 = 禁用网络请求（跳过所有相关接口）
// ================================================================
int g_EnableNetwork = 0;   // 默认开启

// ========== UTF-8 转 UTF-16 (用于 MessageBoxW) ==========
std::wstring Utf8ToWide(const std::string& utf8Str)
{
    if (utf8Str.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), (int)utf8Str.size(), NULL, 0);
    if (len <= 0) return L"";
    std::wstring wideStr(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Str.c_str(), (int)utf8Str.size(), &wideStr[0], len);
    return wideStr;
}

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
static bool dataLoaded = false;
static bool versionChecked = false;
static bool maintenanceChecked = false;
static bool maintenanceBlocked = false;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ========== JSON 解析辅助函数 ==========
std::string JsonGetString(const std::string& json, const std::string& key)
{
    std::string searchKey = "\"" + key + "\":";
    size_t start = json.find(searchKey);
    if (start == std::string::npos) {
        searchKey = "\"" + key + "\" :";
        start = json.find(searchKey);
        if (start == std::string::npos) return "";
    }
    start += searchKey.length();

    while (start < json.length() && (json[start] == ' ' || json[start] == '\t')) start++;

    if (start < json.length() && json[start] == '"') {
        start++;
        size_t end = json.find('"', start);
        if (end == std::string::npos) return "";
        return json.substr(start, end - start);
    }
    else {
        size_t end = json.find_first_of(",}\n\r", start);
        if (end == std::string::npos) end = json.length();
        std::string value = json.substr(start, end - start);
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n')) value.pop_back();
        while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' || value.front() == '\n')) value.erase(0, 1);
        return value;
    }
}

int JsonGetInt(const std::string& json, const std::string& key)
{
    std::string value = JsonGetString(json, key);
    if (value.empty()) return 0;
    return atoi(value.c_str());
}

// ========== HTTP GET 请求 ==========
std::string HttpGet(const std::string& url)
{
    // 如果网络开关关闭，直接返回空（不发起请求）
    if (g_EnableNetwork == 0) return "";

    std::string result;
    HINTERNET hInternet = InternetOpenA("SeroMenu", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!hInternet) return result;

    HINTERNET hConnect = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (hConnect) {
        char buffer[4096];
        DWORD bytesRead = 0;
        while (InternetReadFile(hConnect, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            result += buffer;
        }
        InternetCloseHandle(hConnect);
    }
    InternetCloseHandle(hInternet);
    return result;
}

// ========== 检查维护状态（添加网络开关判断） ==========
bool CheckMaintenanceStatus()
{
    if (maintenanceChecked) return maintenanceBlocked;

    // 如果网络被禁用，直接标记为已检查，并认为不处于维护状态
    if (g_EnableNetwork == 0) {
        maintenanceChecked = true;
        maintenanceBlocked = false;
        return false;
    }

    maintenanceChecked = true;

    std::string response = HttpGet("http://www.api.88828.win/api/api.php?action=maintain_status&secret=Kj9sdf23JkLsa890KeyApi2026");
    if (response.empty()) {
        maintenanceBlocked = false;
        return false;
    }

    int status = JsonGetInt(response, "status");
    std::string reason = JsonGetString(response, "reason");

    size_t pos = 0;
    while ((pos = reason.find("\\r\\n", pos)) != std::string::npos) {
        reason.replace(pos, 4, "\n");
        pos += 1;
    }
    while ((pos = reason.find("\\n", pos)) != std::string::npos) {
        reason.replace(pos, 2, "\n");
        pos += 1;
    }
    while ((pos = reason.find("\\r", pos)) != std::string::npos) {
        reason.replace(pos, 2, "\n");
        pos += 1;
    }

    if (status == 1) {
        maintenanceBlocked = true;

        std::string title = "系统维护通知";
        std::string content = "═══════════════════════════════════\n";
        content += "         系统正在维护中\n";
        content += "═══════════════════════════════════\n\n";
        content += "维护原因:\n";
        content += "─────────────────────────────\n";
        content += (reason.empty() ? "暂无具体说明，请稍后再试。" : reason) + "\n";
        content += "─────────────────────────────\n\n";
        content += "请耐心等待维护完成。";

        std::wstring wTitle = Utf8ToWide(title);
        std::wstring wContent = Utf8ToWide(content);
        MessageBoxW(NULL, wContent.c_str(), wTitle.c_str(), MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
        return true;
    }

    maintenanceBlocked = false;
    return false;
}

// ========== 打开浏览器并自动复制提取码 ==========
void OpenDownloadWithPassword()
{
    ShellExecuteA(NULL, "open", DOWNLOAD_URL, NULL, NULL, SW_SHOWNORMAL);

    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        size_t len = strlen(DOWNLOAD_PASSWORD) + 1;
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
        if (hMem) {
            char* pMem = (char*)GlobalLock(hMem);
            if (pMem) {
                memcpy(pMem, DOWNLOAD_PASSWORD, len);
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);
            }
        }
        CloseClipboard();

        std::string msg = "已打开下载页面，提取码已自动复制到剪贴板！\n\n";
        msg += "提取码: " + std::string(DOWNLOAD_PASSWORD) + "\n";
        msg += "如果未自动复制，请手动输入。";

        std::wstring wMsg = Utf8ToWide(msg);
        MessageBoxW(NULL, wMsg.c_str(), L"下载提示", MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
    }
    else {
        std::string msg = "已打开下载页面，请手动输入提取码：\n\n";
        msg += "提取码: " + std::string(DOWNLOAD_PASSWORD);

        std::wstring wMsg = Utf8ToWide(msg);
        MessageBoxW(NULL, wMsg.c_str(), L"下载提示", MB_OK | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND);
    }
}

// ========== 解析版本号 ==========
std::string ParseVersionNumber(const std::string& raw)
{
    if (raw.empty()) return "";

    size_t pipePos = raw.find('|');
    if (pipePos != std::string::npos) {
        return raw.substr(0, pipePos);
    }

    size_t spacePos = raw.find_first_of(" \t\n\r");
    if (spacePos != std::string::npos) {
        return raw.substr(0, spacePos);
    }

    return raw;
}

// ========== 格式化版本信息 ==========
std::string FormatVersionInfo(const std::string& raw)
{
    if (raw.empty()) return "";

    std::istringstream stream(raw);
    std::string line;
    std::string formatted;
    bool firstBlock = true;

    while (std::getline(stream, line)) {
        if (line.empty()) continue;

        size_t pipePos = line.find('|');
        if (pipePos == std::string::npos) {
            if (!firstBlock) formatted += "\n\n";
            formatted += line;
            firstBlock = false;
            continue;
        }

        std::string version = line.substr(0, pipePos);
        std::string logPart = line.substr(pipePos + 1);

        std::vector<std::string> items;
        size_t pos = 0;
        while (pos < logPart.length()) {
            size_t numStart = logPart.find_first_of("0123456789", pos);
            if (numStart == std::string::npos) break;
            if (numStart + 1 >= logPart.length() || logPart[numStart + 1] != '.') {
                pos = numStart + 1;
                continue;
            }
            size_t itemEnd = logPart.find_first_of("0123456789", numStart + 2);
            while (itemEnd != std::string::npos) {
                if (itemEnd + 1 < logPart.length() && logPart[itemEnd + 1] == '.') {
                    break;
                }
                itemEnd = logPart.find_first_of("0123456789", itemEnd + 1);
            }
            if (itemEnd == std::string::npos) itemEnd = logPart.length();

            std::string item = logPart.substr(numStart, itemEnd - numStart);
            while (!item.empty() && item.back() == ' ') item.pop_back();
            while (!item.empty() && item.front() == ' ') item.erase(0, 1);
            if (!item.empty()) items.push_back(item);

            pos = itemEnd;
        }

        if (items.empty() && !logPart.empty()) {
            while (!logPart.empty() && logPart.back() == ' ') logPart.pop_back();
            while (!logPart.empty() && logPart.front() == ' ') logPart.erase(0, 1);
            items.push_back(logPart);
        }

        if (!firstBlock) formatted += "\n\n";
        formatted += version;
        for (const auto& item : items) {
            formatted += "\n" + item;
        }
        firstBlock = false;
    }

    while (!formatted.empty() && formatted.back() == '\n') formatted.pop_back();
    return formatted;
}

// ========== 版本检查与更新提示（添加网络开关判断） ==========
void CheckVersionAndNotify()
{
    if (versionChecked) return;

    // 如果网络被禁用，直接标记为已检查，不执行后续
    if (g_EnableNetwork == 0) {
        versionChecked = true;
        return;
    }

    versionChecked = true;

    std::string versionData = HttpGet("http://www.api.88828.win/api/version.php?act=read");
    if (versionData.empty()) {
        return;
    }

    std::string latestVersion = ParseVersionNumber(versionData);
    if (latestVersion.empty()) {
        return;
    }

    if (latestVersion != SOFTWARE_VERSION) {
        std::string formattedVersion = FormatVersionInfo(versionData);
        std::string title = "发现新版本: " + latestVersion;
        std::string content = "═══════════════════════════════════\n";
        content += "  发现新版本: " + latestVersion + "\n";
        content += std::string("  当前版本: ") + SOFTWARE_VERSION + "\n";
        content += "═══════════════════════════════════\n\n";
        content += "更新内容:\n";
        content += "─────────────────────────────\n";
        content += formattedVersion + "\n";
        content += "─────────────────────────────\n\n";
        content += "是否前往下载最新版本？\n";
        content += "点击「是」将打开下载页面并自动复制提取码，并继续注入；\n";
        content += "点击「否」则继续注入，不进行下载。";

        std::wstring wTitle = Utf8ToWide(title);
        std::wstring wContent = Utf8ToWide(content);
        int result = MessageBoxW(
            NULL,
            wContent.c_str(),
            wTitle.c_str(),
            MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST | MB_SETFOREGROUND
        );

        if (result == IDYES) {
            OpenDownloadWithPassword();
        }
        // 无论点击是或否，函数结束后都会继续执行，不影响注入
    }
}

// ========== 解析多行版本信息（菜单使用） ==========
void ParseVersionInfo(const std::string& raw, MenuData& data)
{
    if (raw.empty()) {
        data.versionInfo = "";
        data.versionNumber = "";
        return;
    }

    data.versionInfo = FormatVersionInfo(raw);

    if (!data.versionInfo.empty()) {
        size_t firstNewline = data.versionInfo.find('\n');
        if (firstNewline != std::string::npos) {
            data.versionNumber = data.versionInfo.substr(0, firstNewline);
        }
        else {
            data.versionNumber = data.versionInfo;
        }
    }
    else {
        data.versionNumber = "";
    }
}

// ========== 解析公告 JSON ==========
void ParseAnnouncement(const std::string& json, MenuData& data)
{
    if (json.empty()) {
        data.announcement = "网络请求失败，请检查网络后点击刷新重试。";
        return;
    }

    std::string result = JsonGetString(json, "announcement");
    if (result.empty()) {
        data.announcement = "解析公告失败，原始数据：\n" + json;
        return;
    }

    size_t pos = 0;
    while ((pos = result.find("\\r\\n", pos)) != std::string::npos) {
        result.replace(pos, 4, "\n");
        pos += 1;
    }
    while ((pos = result.find("\\n", pos)) != std::string::npos) {
        result.replace(pos, 2, "\n");
        pos += 1;
    }
    while ((pos = result.find("\\r", pos)) != std::string::npos) {
        result.replace(pos, 2, "\n");
        pos += 1;
    }
    while (!result.empty() && result.back() == '\n') result.pop_back();

    data.announcement = result.empty() ? "公告内容为空" : result;
}

// ========== 刷新远程数据（添加网络开关判断） ==========
void RefreshRemoteData()
{
    // 如果网络被禁用，直接显示提示信息，不发起任何请求
    if (g_EnableNetwork == 0) {
        g_MenuData.versionInfo = "网络功能已禁用，无法获取版本信息。";
        g_MenuData.versionNumber = "";
        g_MenuData.announcement = "网络功能已禁用，无法获取公告。";
        return;
    }

    std::string versionRaw = HttpGet("http://www.api.88828.win/api/version.php?act=read");
    if (!versionRaw.empty()) {
        ParseVersionInfo(versionRaw, g_MenuData);
    }
    else {
        g_MenuData.versionInfo = "版本信息加载失败，请检查网络。";
        g_MenuData.versionNumber = "";
    }

    std::string announceRaw = HttpGet("http://www.api.88828.win/api/api.php?action=announcement&secret=Kj9sdf23JkLsa890KeyApi2026");
    if (!announceRaw.empty()) {
        ParseAnnouncement(announceRaw, g_MenuData);
    }
    else {
        g_MenuData.announcement = "公告加载失败，请点击刷新重试。";
    }
}

// ========== 原有代码继续 ==========

static bool __declspec(nothrow) RunCheatLogicSafe()
{
    if (maintenanceBlocked || !cs2_runtime::IsInitialized()) return false;

    __try {
        Cheat::RunThirdPerson();
        Cheat::UpdateEntities();

        if (g_MenuData.bMainESP) {
            ImDrawList* drawList = ImGui::GetBackgroundDrawList();
            if (drawList) {
                Cheat::DrawESP(drawList);
            }
        }

        Cheat::RunAimbot();
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
    dataLoaded = false;
    versionChecked = false;
    maintenanceChecked = false;
    maintenanceBlocked = false;
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

    if (!maintenanceChecked) {
        if (CheckMaintenanceStatus()) {
            return CallOriginalPresent(_this, a, b);
        }
    }

    if (maintenanceBlocked) {
        return CallOriginalPresent(_this, a, b);
    }

    EnterCriticalSection(&presentCriticalSection);

    __try {
        if (!inited)
        {
            ID3D11Device* currentDevice = nullptr;
            HRESULT hr = _this->GetDevice(__uuidof(ID3D11Device), (void**)&currentDevice);
            if (FAILED(hr) || !currentDevice) return CallOriginalPresent(_this, a, b);
            
            g_pd3dDevice = currentDevice;
            g_pd3dDevice->AddRef();

            g_pd3dDevice->GetImmediateContext(&g_pd3dContext);
            if (!g_pd3dContext) {
                ResetImGuiState();
                return CallOriginalPresent(_this, a, b);
            }

            DXGI_SWAP_CHAIN_DESC sd;
            hr = _this->GetDesc(&sd);
            if (FAILED(hr) || !sd.OutputWindow) {
                ResetImGuiState();
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
            if (!initOnce) {
                origin_wndProc_saved = origin_wndProc;
                initOnce = true;
            }
            ImGui::CreateContext();
            if (!ImGui::GetCurrentContext()) {
                ResetImGuiState();
                return CallOriginalPresent(_this, a, b);
            }
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
            if (!ImGui_ImplWin32_Init(g_hwnd) || !ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dContext)) {
                ResetImGuiState();
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
                for (const char* path : cnFontPaths)
                {
                    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
                    {
                        font = io.Fonts->AddFontFromFileTTF(path, 18.0f, nullptr, cjkRanges);
                        if (font) break;
                    }
                }
                if (!font)
                    io.Fonts->AddFontDefault();
            }

            inited = true;
            initFrameCount = 0;
            espReady = false;
            dataLoaded = false;
            versionChecked = false;
        }

        if (inited && !dataLoaded) {
            RefreshRemoteData();
            dataLoaded = true;
        }

        if (inited && dataLoaded && !versionChecked) {
            static int delayCounter = 0;
            delayCounter++;
            if (delayCounter > 5) {
                CheckVersionAndNotify();
            }
        }

        if (!g_pd3dDevice || !g_pd3dContext) {
            return CallOriginalPresent(_this, a, b);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (g_ShowMenu && !maintenanceBlocked)
            RenderMainMenu();

        if (initFrameCount < 5) {
            initFrameCount++;
        }
        else {
            espReady = true;
        }

        if (espReady && !maintenanceBlocked) {
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
    if (minHookStatus != MH_OK && minHookStatus != MH_ERROR_ALREADY_INITIALIZED) {
        return 0;
    }

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

    if (tempSwap)
    {
        void*** vtable_ptr = (void***)tempSwap;
        void** vtable = *vtable_ptr;
        void* presentFunc = vtable[8];
        if (MH_CreateHook(presentFunc, my_present, &origin_present) == MH_OK) {
            MH_EnableHook(presentFunc);
        }
        tempDev->Release();
        tempSwap->Release();
    }
    // Wait for all client modules and SchemaSystem registrations. This work
    // must not run from DllMain while the loader lock is held.
    for (int attempt = 0; attempt < 120; ++attempt) {
        memory::Initialize();
        if (Cheat::Init(memory::clientBase)) {
            Cheat::InitSilentAimHook();
            Cheat::InitThirdPersonHook();
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

        // Defer all module scanning, MinHook setup, and hook registration to
        // the worker thread so DLL_PROCESS_ATTACH stays loader-lock safe.
        if (HANDLE worker = CreateThread(NULL, 0, create, NULL, 0, NULL)) {
            CloseHandle(worker);
        }
        break;
    case DLL_PROCESS_DETACH:
        Cheat::CleanupSilentAimHook();
        Cheat::CleanupThirdPersonHook();
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
