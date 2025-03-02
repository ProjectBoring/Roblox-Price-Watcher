#include "resource.h"
#include "imgui_physical_store/imgui.h"
#include "imgui_physical_store/imgui_impl_win32.h"
#include "imgui_physical_store/imgui_impl_dx11.h"
#include "imgui_physical_store/imgui_impl_glfw.h"
#include "imgui_physical_store/imgui_impl_opengl3.h"
#include "imgui_physical_store/imgui_internal.h"
#include <d3d11.h>
#include <tchar.h>
#include <TlHelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <Psapi.h>
#include <codecvt>
#include <windows.h> // Added for completeness
#include <algorithm>
#include "PriceWatchingEnergySaver.h" // Caching header
#define STB_IMAGE_IMPLEMENTATION
#include "WebHandling.h" // Include the WebHandler class
#include "ImageLoading.h"

#pragma comment(lib, "d3d11.lib") // Link DirectX11 library

// Global Variables for DirectX11
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool g_SwapChainOccluded = false;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Forward Declarations of Helper Functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Console Allocation Function
void AllocateAndRedirectConsole() {
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        if (!AllocConsole()) {
            MessageBox(nullptr, L"Failed to allocate console.", L"Error", MB_OK | MB_ICONERROR);
            return;
        }
    }

    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    std::cout.clear();
    freopen_s(&fp, "CONOUT$", "w", stderr);
    std::cerr.clear();
    freopen_s(&fp, "CONIN$", "r", stdin);
    std::cin.clear();

    std::cout << "Console allocated or attached successfully.\n";
    std::cout << "Press Enter to close the console...\n";
}

// Assuming these are defined elsewhere in your codebase
extern IDXGISwapChain* g_pSwapChain;
extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dDeviceContext;
extern ID3D11RenderTargetView* g_mainRenderTargetView;
extern bool g_SwapChainOccluded;
extern UINT g_ResizeWidth, g_ResizeHeight;
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void AllocateAndRedirectConsole();

// Setup Default Docking Layout
void SetupDefaultDockingLayout() {
    static bool initialized = false;
    if (!initialized) {
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockBuilderRemoveNode(dockspace_id); // Clear any previous layout
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

        ImGuiID dock_id_left, dock_id_right, dock_id_bottom;
        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.4f, &dock_id_left, &dock_id_right); // Split left
        ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Down, 0.3f, &dock_id_bottom, &dock_id_right); // Split right part to bottom

        ImGui::DockBuilderDockWindow("Assets", dock_id_left);
        ImGui::DockBuilderDockWindow("Settings", dock_id_right);
        ImGui::DockBuilderDockWindow("Debug", dock_id_bottom);

        ImGui::DockBuilderFinish(dockspace_id);
        initialized = true;
    }
}

int main(int argc, char** argv) {
    std::cout << "IMGUI UI Starting now!" << std::endl;

    // AllocateAndRedirectConsole(); // DON'T REMOVE!!! For debugging purposes.

    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASSEXW wc = {
        sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance,
        LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ROBLOXPRICEWATCHERV1)),
        LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr,
        L"Roblox Price Watcher v1",
        LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION))
    };
    ::RegisterClassExW(&wc);

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = 1280;
    int windowHeight = 720;
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"Roblox Price Watcher v1",
        WS_OVERLAPPEDWINDOW,
        posX, posY, windowWidth, windowHeight,
        nullptr, nullptr, wc.hInstance, nullptr
    );

    if (!hwnd) {
        std::cerr << "Failed to create window!" << std::endl;
        UnregisterClassW(wc.lpszClassName, hInstance);
        FreeConsole();
        return 1;
    }

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        DestroyWindow(hwnd);
        UnregisterClassW(wc.lpszClassName, hInstance);
        FreeConsole();
        return 1;
    }

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_DockingEnable;
    io.FontGlobalScale = 1.0f;
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 16.0f);
    ImGui::StyleColorsClassic();

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    HDC hdc = GetDC(hwnd);
    if (hdc) {
        int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(hwnd, hdc);
        float scaleFactor = static_cast<float>(dpiX) / 96.0f;
        io.FontGlobalScale = scaleFactor;
        ImGui::GetStyle().ScaleAllSizes(scaleFactor);
        ImGui::GetStyle().WindowMinSize = ImVec2(200 * scaleFactor, 100 * scaleFactor);
    }

    ImageLoading image_loader(g_pd3dDevice);

    bool done = false;
    static char roblox_cookie[2048] = "";
    static char asset_id_input[32] = "";
    static std::string response_text = "";
    static std::string xsrf_token = "";
    static std::string debug_info = "";
    static std::vector<Asset> assets;
    static std::string cached_cookie = LoadCookieCache();
    static char webhook_url[256] = ""; // Initialize empty; load from cache below
    static int selected_asset = -1;
    static bool checker_enabled = false;
    static bool show_add_asset = false;
    static PriceWatcher* price_watcher = nullptr;
    static std::vector<ID3D11ShaderResourceView*> asset_thumbnails;
    static bool show_credits = false; // Flag for showing credits window

    // Load cached webhook URL at startup
    std::string cached_webhook = LoadWebhookCache();
    if (!cached_webhook.empty()) {
        strncpy_s(webhook_url, sizeof(webhook_url), cached_webhook.c_str(), cached_webhook.size());
    }
    else {
        // Use default if no cache exists
        strncpy_s(webhook_url, sizeof(webhook_url), "https://discord.com/api/webhooks/your_webhook_here", strlen("https://discord.com/api/webhooks/your_webhook_here"));
    }

    if (!cached_cookie.empty()) {
        strncpy_s(roblox_cookie, sizeof(roblox_cookie), cached_cookie.c_str(), cached_cookie.size());
    }

    if (strlen(roblox_cookie) > 0) {
        WebHandler web;
        xsrf_token = web.getXSRFToken(roblox_cookie);
        if (!xsrf_token.empty()) {
            assets = LoadAssetsCache(roblox_cookie, xsrf_token);
            for (const auto& asset : assets) {
                ID3D11ShaderResourceView* texture = image_loader.LoadTextureFromURL(asset.thumbnail_url);
                asset_thumbnails.push_back(texture);
            }
        }
        else {
            std::cerr << "Failed to fetch XSRF token; assets not loaded yet." << std::endl;
        }
    }

    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport(ImGui::GetID("MyDockSpace"), ImGui::GetMainViewport(), ImGuiDockNodeFlags_AutoHideTabBar);
        SetupDefaultDockingLayout();

        RECT windowRect;
        GetClientRect(hwnd, &windowRect);
        float windowWidth = static_cast<float>(windowRect.right - windowRect.left);
        float windowHeight = static_cast<float>(windowRect.bottom - windowRect.top);

        float assetsWindowWidth = windowWidth * 0.4f;
        float settingsWindowWidth = windowWidth * 0.6f;
        float assetsWindowHeight = windowHeight * 0.7f;
        float settingsWindowHeight = assetsWindowHeight;
        float debugWindowHeight = windowHeight * 0.3f;

        // Assets Window
        ImGui::SetNextWindowSize(ImVec2(assetsWindowWidth, assetsWindowHeight), ImGuiCond_Always);
        ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_NoMove); // Removed ImGuiWindowFlags_NoCollapse
        ImGui::Text("Welcome to Roblox Price Watcher v1!");

        std::vector<std::string> asset_names;
        for (const auto& asset : assets) {
            asset_names.push_back(asset.name + " (ID: " + std::to_string(asset.id) + ")");
        }
        if (ImGui::BeginCombo("##AssetsCombo", selected_asset >= 0 ? asset_names[selected_asset].c_str() : "Select Asset")) {
            for (int n = 0; n < asset_names.size(); n++) {
                const bool is_selected = (selected_asset == n);
                if (ImGui::Selectable(asset_names[n].c_str(), is_selected))
                    selected_asset = n;
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (selected_asset >= 0 && selected_asset < assets.size()) {
            Asset& asset = assets[selected_asset];
            ImGui::Text("Selected Asset: %s (ID: %lld)", asset.name.c_str(), asset.id);
            if (ImGui::InputInt("Price Threshold", &asset.price_threshold, 1, 100)) {
                SaveAssetsCache(assets);
            }
            ImGui::Text("Current Price: %d", asset.current_price);
            if (selected_asset < asset_thumbnails.size() && asset_thumbnails[selected_asset]) {
                ImGui::Image((ImTextureID)asset_thumbnails[selected_asset], ImVec2(200, 200));
            }
            else {
                ImGui::Text("Thumbnail not loaded.");
            }
            if (ImGui::Button("Remove Asset", ImVec2(200, 30))) {
                if (selected_asset < asset_thumbnails.size() && asset_thumbnails[selected_asset]) {
                    asset_thumbnails[selected_asset]->Release();
                }
                assets.erase(assets.begin() + selected_asset);
                asset_thumbnails.erase(asset_thumbnails.begin() + selected_asset);
                SaveAssetsCache(assets);
                selected_asset = assets.empty() ? -1 : min(selected_asset, static_cast<int>(assets.size()) - 1);
            }
        }

        if (ImGui::Button("Add New Asset", ImVec2(200, 30))) show_add_asset = true;
        ImGui::End();

        // Settings Window
        ImGui::SetNextWindowSize(ImVec2(settingsWindowWidth, settingsWindowHeight), ImGuiCond_Always);
        ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoMove); // Removed ImGuiWindowFlags_NoCollapse
        ImGui::Text("Roblox Cookie (.ROBLOSECURITY):");
        ImGui::InputTextMultiline("##Cookie", roblox_cookie, sizeof(roblox_cookie), ImVec2(-1.0f, ImGui::GetTextLineHeight() * 4));
        if (ImGui::Button("Save Cookie")) {
            SaveCookieCache(roblox_cookie);
            cached_cookie = roblox_cookie;
            if (!xsrf_token.empty() && assets.empty()) {
                WebHandler web;
                xsrf_token = web.getXSRFToken(roblox_cookie);
                if (!xsrf_token.empty()) {
                    assets = LoadAssetsCache(roblox_cookie, xsrf_token);
                    for (const auto& asset : assets) {
                        ID3D11ShaderResourceView* texture = image_loader.LoadTextureFromURL(asset.thumbnail_url);
                        asset_thumbnails.push_back(texture);
                    }
                }
            }
        }

        // Webhook URL input with Enter key saving
        if (ImGui::InputText("Discord Webhook URL", webhook_url, sizeof(webhook_url))) {
            SaveWebhookCache(webhook_url); // Save new URL to cache when Enter is pressed
            if (price_watcher) {
                // Restart PriceWatcher with updated webhook URL if it’s running
                price_watcher->Stop();
                delete price_watcher;
                price_watcher = new PriceWatcher(roblox_cookie, xsrf_token, webhook_url, assets);
                if (checker_enabled) {
                    price_watcher->Start();
                }
            }
        }

        if (ImGui::Checkbox("Enable Price Checker", &checker_enabled)) {
            if (checker_enabled && !price_watcher) {
                price_watcher = new PriceWatcher(roblox_cookie, xsrf_token, webhook_url, assets);
                price_watcher->Start();
            }
            else if (!checker_enabled && price_watcher) {
                price_watcher->Stop();
                delete price_watcher;
                price_watcher = nullptr;
            }
        }
        ImGui::End();

        ImGui::SetNextWindowSize(ImVec2(windowWidth, debugWindowHeight), ImGuiCond_Always);
        ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoMove);
        ImGui::TextWrapped("Response: %s", response_text.c_str());
        ImGui::TextWrapped("Debug Info: %s", debug_info.c_str());
        if (price_watcher) {
            std::string checker_debug = price_watcher->GetDebugOutput();
            std::string last_check = price_watcher->GetLastCheckTimestamp();
            ImGui::Text("Last Price Check: %s", last_check.c_str());
            ImGui::TextWrapped("Price Checker Debug Output:\n%s", checker_debug.c_str());
        }
        else {
            ImGui::Text("Price Checker: Not running");
        }
        if (ImGui::Button("Test Button")) {
            std::cout << "Button clicked!\n";
        }

        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 buttonSize = ImVec2(30, 30);
        ImGui::SetCursorPos(ImVec2(windowSize.x - buttonSize.x - 10, windowSize.y - buttonSize.y - 10));
        if (ImGui::Button("?", buttonSize)) {
            show_credits = true;
        }

        ImGui::End();

        // Credits Window
        if (show_credits) {
            ImGui::Begin("Credits", &show_credits, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("Roblox Price Watcher v1");

            // --- Discord Name: mothwa ---
            ImGui::Text("Discord: ");
            ImGui::SameLine();

            // Use a selectable, but *don't* draw over it.  Style the *text* directly.
            if (ImGui::Selectable("mothwa##discordname", false, ImGuiSelectableFlags_None, ImVec2(ImGui::CalcTextSize("mothwa").x, ImGui::GetTextLineHeight()))) {
                ImGui::SetClipboardText("mothwa");
                ImGui::LogText("Copied 'mothwa' to clipboard\n");
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Click to copy Discord name: mothwa");
            }

            // --- Discord Server Link ---
            ImGui::Text("Server: ");
            ImGui::SameLine();

            if (ImGui::Selectable("https://discord.gg/mmStmEYdTV##discordlink", false, ImGuiSelectableFlags_None, ImVec2(ImGui::CalcTextSize("https://discord.gg/mmStmEYdTV").x, ImGui::GetTextLineHeight()))) {
                ImGui::SetClipboardText("https://discord.gg/mmStmEYdTV");
                ImGui::LogText("Copied 'https://discord.gg/mmStmEYdTV' to clipboard\n");
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Click to copy server link: https://discord.gg/mmStmEYdTV");
            }


            ImGui::Text("Date: March 2, 2025");
            ImGui::Separator();
            if (ImGui::Button("Close")) {
                show_credits = false;
            }
            ImGui::End();
        }

        if (show_add_asset) {
            ImGui::Begin("Add New Asset", &show_add_asset);
            ImGui::InputText("Asset ID", asset_id_input, sizeof(asset_id_input));
            if (ImGui::Button("Fetch and Add")) {
                std::string cookie_str(roblox_cookie);
                std::string id_str(asset_id_input);

                if (!cookie_str.empty() && !id_str.empty() && std::all_of(id_str.begin(), id_str.end(), ::isdigit)) {
                    WebHandler web;
                    xsrf_token = web.getXSRFToken(cookie_str);
                    if (!xsrf_token.empty()) {
                        std::string url = "https://catalog.roblox.com/v1/catalog/items/details";
                        std::string json_body = R"({"items":[{"itemType":1,"id":)" + id_str + R"(}]})";
                        response_text = web.post(url, json_body, cookie_str, xsrf_token);
                        debug_info = "URL: " + url + "\nBody: " + json_body + "\nXSRF Token: " + xsrf_token;
                        std::cout << "Response: " << response_text << std::endl;

                        size_t name_start = response_text.find("\"name\":\"");
                        if (name_start != std::string::npos) {
                            name_start += 8;
                            size_t name_end = response_text.find("\"", name_start);
                            std::string name = response_text.substr(name_start, name_end - name_start);

                            Asset new_asset;
                            new_asset.id = std::stoll(id_str);
                            new_asset.name = name;
                            new_asset.price_threshold = 1000;
                            new_asset.thumbnail_url = "https://thumbnails.roblox.com/v1/assets?assetIds=" + id_str + "&size=150x150&format=Png";
                            FetchCurrentPrice(new_asset, cookie_str, xsrf_token);
                            assets.push_back(new_asset);
                            asset_thumbnails.push_back(image_loader.LoadTextureFromURL(new_asset.thumbnail_url));
                            SaveAssetsCache(assets);
                            show_add_asset = false;
                        }
                    }
                    else {
                        response_text = "Failed to retrieve XSRF token.";
                    }
                }
                else {
                    response_text = "Invalid cookie or asset ID.";
                }
            }
            ImGui::End();
        }

        ImGui::Render();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
        const float clear_color_with_alpha[4] = { 0.45f, 0.55f, 0.60f, 1.00f };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        HRESULT hr = g_pSwapChain->Present(1, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    if (price_watcher) {
        price_watcher->Stop();
        delete price_watcher;
    }
    for (auto* texture : asset_thumbnails) {
        if (texture) texture->Release();
    }
    FreeConsole();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, hInstance);

    return 0;
}

// **Helper Functions**

bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
        &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK) return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;

    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_ResizeWidth = LOWORD(lParam);
        g_ResizeHeight = HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}