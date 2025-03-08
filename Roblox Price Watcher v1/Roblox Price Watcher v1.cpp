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

// Global Application Variables
std::vector<Asset> assets;                            // List of tracked assets
std::vector<ID3D11ShaderResourceView*> asset_thumbnails; // Thumbnails for assets
PriceWatcher* price_watcher = nullptr;                // Price monitoring object
int selected_asset = -1;                              // Index of selected asset
bool show_add_asset = false;                          // Show add asset popup
bool show_credits = false;                            // Show credits popup
bool show_notification = false;                       // Show notification popup
bool show_price_history = false;                      // Show price history popup
bool checker_enabled = false;                         // Price checker toggle
char roblox_cookie[2048] = "";                        // Roblox cookie input
char webhook_url[1024] = "";                          // Discord webhook URL
char asset_id_input[64] = "";                         // Asset ID input
int check_interval_input = 30;                        // Price check interval (seconds)
std::string response_text;                            // Web response text
std::string debug_info;                               // Debug information
std::string notification_title;                       // Notification title
std::string notification_message;                     // Notification message
std::string xsrf_token;                               // XSRF token for Roblox API
std::string cached_cookie;                            // Cached Roblox cookie
std::mutex assets_mutex;                              // Mutex for thread-safe asset access

// Note: ImageLoading is already in ImageLoading.h and ImageLoading.cpp which I can send you Grok, if you want it. Do not remove this comment.

// Main function
int main(int argc, char** argv) {
    //AllocateAndRedirectConsole(); // DON'T REMOVE!!! For debugging purposes.

    HINSTANCE hInstance = GetModuleHandle(NULL);

    WNDCLASSEXW wc = {
        sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, hInstance,
        LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ROBLOXPRICEWATCHERV1)),
        LoadCursor(nullptr, IDC_ARROW), nullptr, nullptr,
        L"Roblox Price Watcher v1.1.1",
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
        L"Roblox Price Watcher v1.1.1",
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

    // Initialize WebHandler
    WebHandler web_handler;

    // ImGui Initialization with modern theme
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_DockingEnable;
    io.FontGlobalScale = 1.0f;

    // Load custom fonts
    ImFont* largeFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 22.0f);
    ImFont* regularFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 16.0f);
    ImFont* smallFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\Arial.ttf", 14.0f);

    // Modern dark theme with blue accents
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.FramePadding = ImVec2(8, 4);
    style.ItemSpacing = ImVec2(10, 8);
    style.ItemInnerSpacing = ImVec2(6, 6);
    style.WindowPadding = ImVec2(12, 12);

    // Define colors (abbreviated for brevity, same as in the original)
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
    colors[ImGuiCol_Border] = ImVec4(0.26f, 0.26f, 0.26f, 0.26f);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Initialize ImageLoading with the DirectX device
    ImageLoading image_loader(g_pd3dDevice); // Assumes ImageLoading is defined in ImageLoading.h

    // Adjust for DPI scaling
    HDC hdc = GetDC(hwnd);
    if (hdc) {
        int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(hwnd, hdc);
        float scaleFactor = static_cast<float>(dpiX) / 96.0f;
        io.FontGlobalScale = scaleFactor;
        ImGui::GetStyle().ScaleAllSizes(scaleFactor);
        ImGui::GetStyle().WindowMinSize = ImVec2(200 * scaleFactor, 100 * scaleFactor);
    }

    // Load the saved cookie from cache
    std::string loaded_cookie = LoadCookieCache();
    if (!loaded_cookie.empty()) {
        strncpy_s(roblox_cookie, loaded_cookie.c_str(), sizeof(roblox_cookie) - 1);
        roblox_cookie[sizeof(roblox_cookie) - 1] = '\0'; // Ensure null-termination
        // Fetch XSRF token if cookie is loaded
        xsrf_token = web_handler.getXSRFToken(roblox_cookie);
    }

    // Load the saved webhook URL from cache (optional, for consistency)
    std::string loaded_webhook = LoadWebhookCache();
    if (!loaded_webhook.empty()) {
        strncpy_s(webhook_url, loaded_webhook.c_str(), sizeof(webhook_url) - 1);
        webhook_url[sizeof(webhook_url) - 1] = '\0'; // Ensure null-termination
    }

    // Load initial assets and thumbnails
    assets = LoadAssetsCache(roblox_cookie, xsrf_token);
    for (const auto& asset : assets) {
        ID3D11ShaderResourceView* texture = image_loader.LoadTextureFromURL(asset.thumbnail_url);
        asset_thumbnails.push_back(texture);
    }

    // Main loop
    bool done = false;
    while (!done) {
        MSG msg;
        while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        // Handle window resize
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

        // **Assets Window**
        ImGui::PushFont(largeFont);
        ImGui::Begin("Assets", nullptr, ImGuiWindowFlags_NoMove);
        ImGui::PopFont();

        ImGui::PushFont(regularFont);
        ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "Welcome to Roblox Price Watcher!");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        std::vector<std::string> asset_names;
        for (const auto& asset : assets) {
            asset_names.push_back(asset.name + " (ID: " + std::to_string(asset.id) + ")");
        }

        float max_width = 300.0f; // Minimum width
        for (const auto& name : asset_names) {
            max_width = max(max_width, ImGui::CalcTextSize(name.c_str()).x + 50.0f); // Add padding
        }

        ImGui::SetNextItemWidth(max_width);

        if (ImGui::BeginCombo("##AssetsCombo", selected_asset >= 0 ? asset_names[selected_asset].c_str() : "Select Asset")) {
            for (int n = 0; n < asset_names.size(); n++) {
                if (ImGui::Selectable(asset_names[n].c_str(), selected_asset == n)) {
                    selected_asset = n;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();

        if (selected_asset >= 0 && selected_asset < assets.size()) {
            std::lock_guard<std::mutex> lock(assets_mutex);
            Asset& asset = assets[selected_asset];

            ImGui::BeginChild("SelectedAssetDetails", ImVec2(0, 250), true, ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.27f, 0.46f, 0.4f));
            ImGui::BeginChild("AssetNameBanner", ImVec2(0, 40), true, ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            // Format the text and calculate its size
            const char* text = "Selected Asset: %s";
            char buffer[256]; // Adjust size if asset.name could be longer
            sprintf_s(buffer, sizeof(buffer), text, asset.name.c_str());
            ImVec2 text_size = ImGui::CalcTextSize(buffer);

            // Get the available space in the child window (accounting for padding)
            ImVec2 content_region = ImGui::GetContentRegionAvail();

            // Calculate centered position
            float pos_x = (content_region.x - text_size.x) * 0.5f; // Center horizontally
            float pos_y = (40.0f - text_size.y) * 0.5f;           // Center vertically (40 is the child window height)

            // Set the cursor position
            ImGui::SetCursorPos(ImVec2(pos_x, pos_y));

            // Render the centered text
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Selected Asset: %s", asset.name.c_str());
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Columns(2, "assetDetailsColumns", false);

            if (selected_asset < asset_thumbnails.size() && asset_thumbnails[selected_asset]) {
                ImGui::Image((ImTextureID)asset_thumbnails[selected_asset], ImVec2(150, 150));
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
                ImGui::BeginChild("NoThumbnail", ImVec2(150, 150), true);
                ImGui::SetCursorPos(ImVec2(30, 65));
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No Image");
                ImGui::EndChild();
                ImGui::PopStyleColor();
            }

            ImGui::NextColumn();

            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Asset ID:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%lld", asset.id);

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Current Price:");
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "%d R$", asset.current_price);

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Price Threshold:");
            ImGui::SetNextItemWidth(200);
            if (ImGui::InputInt("##PriceThreshold", &asset.price_threshold, 50, 100)) {
                SaveAssetsCache(assets);
            }

            ImGui::Spacing();
            if (price_watcher && price_watcher->IsRunning()) { // Use accessor method
                auto now = std::chrono::steady_clock::now();
                auto next_check = price_watcher->GetNextCheckTime(); // Use accessor method
                auto duration = std::chrono::duration_cast<std::chrono::seconds>(next_check - now);
                long long seconds_left = duration.count(); // Use long long to avoid conversion warning
                int frame = ImGui::GetFrameCount() / 10 % 4;

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.29f, 0.48f, 0.4f));
                ImGui::BeginChild("StatusIndicator", ImVec2(0, 40), true);

                // Prepare the text based on seconds_left and calculate its size
                char buffer[64]; // Adjust size if needed
                if (seconds_left > 0) {
                    sprintf_s(buffer, sizeof(buffer), " %c Updating in %lld seconds", "-\\|/"[frame], seconds_left);
                }
                else {
                    sprintf_s(buffer, sizeof(buffer), " %c Updating now", "-\\|/"[frame]);
                }
                ImVec2 text_size = ImGui::CalcTextSize(buffer);

                // Get the available space in the child window (accounting for padding)
                ImVec2 content_region = ImGui::GetContentRegionAvail();

                // Calculate centered position
                float pos_x = (content_region.x - text_size.x) * 0.5f; // Center horizontally
                float pos_y = (40.0f - text_size.y) * 0.5f;           // Center vertically (40 is the child window height)

                // Set the cursor position
                ImGui::SetCursorPos(ImVec2(pos_x, pos_y));

                // Render the centered text with appropriate color
                if (seconds_left > 0) {
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), " %c Updating in %lld seconds", "-\\|/"[frame], seconds_left);
                }
                else {
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), " %c Updating now", "-\\|/"[frame]);
                }

                ImGui::EndChild();
                ImGui::PopStyleColor();
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.4f, 0.1f, 0.1f, 0.4f));
                ImGui::BeginChild("StatusIndicator", ImVec2(0, 40), true);

                // Calculate the text size
                const char* text = "Price checker is not running";
                ImVec2 text_size = ImGui::CalcTextSize(text);

                // Get the available space in the child window (accounting for padding)
                ImVec2 content_region = ImGui::GetContentRegionAvail();

                // Calculate centered position
                float pos_x = (content_region.x - text_size.x) * 0.5f; // Center horizontally
                float pos_y = (40.0f - text_size.y) * 0.5f;           // Center vertically (40 is the child window height)

                // Set the cursor position
                ImGui::SetCursorPos(ImVec2(pos_x, pos_y));

                // Render the centered text
                ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.6f, 1.0f), "Price checker is not running");

                ImGui::EndChild();
                ImGui::PopStyleColor();
            }

            ImGui::Columns(1);
            ImGui::EndChild();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.25f, 0.25f, 1.0f));
            if (ImGui::Button("Remove Asset", ImVec2(200, 30))) {
                if (selected_asset < asset_thumbnails.size() && asset_thumbnails[selected_asset]) {
                    asset_thumbnails[selected_asset]->Release();
                }
                assets.erase(assets.begin() + selected_asset);
                asset_thumbnails.erase(asset_thumbnails.begin() + selected_asset);
                SaveAssetsCache(assets);
                selected_asset = assets.empty() ? -1 : min(selected_asset, static_cast<int>(assets.size()) - 1);
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            if (ImGui::Button("Show Price History", ImVec2(200, 30))) {
                show_price_history = true;
            }
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
        if (ImGui::Button("Add New Asset", ImVec2(200, 35))) show_add_asset = true;
        ImGui::PopStyleColor(3);

        ImGui::PopFont();
        ImGui::End();

        // **Settings Window**
        ImGui::PushFont(largeFont);
        ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoMove);
        ImGui::PopFont();

        ImGui::PushFont(regularFont);
        ImGui::Spacing();
        ImGui::BeginChild("SettingsArea", ImVec2(0, 0), true);

        ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "Roblox Cookie (.ROBLOSECURITY):");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.15f, 0.2f, 1.00f));
        ImGui::InputTextMultiline("##Cookie", roblox_cookie, sizeof(roblox_cookie), ImVec2(-1.0f, ImGui::GetTextLineHeight() * 4));
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.63f, 0.98f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.53f, 0.94f, 1.0f));
        if (ImGui::Button("Save Cookie", ImVec2(150, 28))) {
            SaveCookieCache(roblox_cookie);
            cached_cookie = roblox_cookie;
            std::string new_xsrf_token = web_handler.getXSRFToken(roblox_cookie);
            if (new_xsrf_token != xsrf_token) {
                xsrf_token = new_xsrf_token;
                if (!xsrf_token.empty()) {
                    // Don’t clear assets or thumbnails
                    if (price_watcher) {
                        price_watcher->Stop();
                        delete price_watcher;
                    }
                    price_watcher = new PriceWatcher(roblox_cookie, xsrf_token, webhook_url, assets, check_interval_input);
                    if (checker_enabled) {
                        price_watcher->Start();
                    }
                }
            }
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "Discord Webhook URL:");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.15f, 0.2f, 1.00f));
        if (ImGui::InputText("##WebhookURL", webhook_url, sizeof(webhook_url), ImGuiInputTextFlags_EnterReturnsTrue)) {
            SaveWebhookCache(webhook_url);
            if (price_watcher) {
                price_watcher->Stop();
                delete price_watcher;
                price_watcher = new PriceWatcher(roblox_cookie, xsrf_token, webhook_url, assets, check_interval_input);
                if (checker_enabled) {
                    price_watcher->Start();
                }
            }
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "Check Interval (seconds):");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.15f, 0.2f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.26f, 0.59f, 0.98f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
        ImGui::SliderInt("##CheckIntervalSlider", &check_interval_input, 1, 60, "%d sec");
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.63f, 0.98f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.53f, 0.94f, 1.0f));
        if (ImGui::Button("Apply", ImVec2(80, 0))) {
            if (check_interval_input <= 0) check_interval_input = 1;
            if (price_watcher) {
                price_watcher->Stop(); // Now stops and joins the thread
                delete price_watcher;   // Safe to delete after thread is joined
                price_watcher = new PriceWatcher(roblox_cookie, xsrf_token, webhook_url, assets, check_interval_input);
                if (checker_enabled) {
                    price_watcher->Start();
                }
            }
        }
        ImGui::PopStyleColor(3);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "Price Checker Status:");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, checker_enabled ? ImVec4(0.2f, 0.7f, 0.2f, 0.8f) : ImVec4(0.7f, 0.2f, 0.2f, 0.8f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, checker_enabled ? ImVec4(0.3f, 0.8f, 0.3f, 0.9f) : ImVec4(0.8f, 0.3f, 0.3f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, checker_enabled ? ImVec4(0.2f, 0.9f, 0.2f, 1.0f) : ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button(checker_enabled ? "ENABLED" : "DISABLED", ImVec2(150, 40))) {
            checker_enabled = !checker_enabled;
            if (checker_enabled && !price_watcher) {
                price_watcher = new PriceWatcher(roblox_cookie, xsrf_token, webhook_url, assets, check_interval_input);
                price_watcher->Start();
            }
            else if (!checker_enabled && price_watcher) {
                price_watcher->Stop(); // Stops and joins the thread
                delete price_watcher;  // Safe deletion
                price_watcher = nullptr;
            }
        }
        ImGui::PopStyleColor(3);

        ImGui::EndChild();
        ImGui::PopFont();
        ImGui::End();

        // **Debug Window**
        ImGui::PushFont(regularFont);
        ImGui::Begin("Debug", nullptr, ImGuiWindowFlags_NoMove);
        ImGui::BeginChild("DebugScroll", ImVec2(0, 0), true);
        ImGui::PushFont(smallFont);

        if (!response_text.empty()) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.15f, 0.20f, 0.5f));
            ImGui::BeginChild("ResponseSection", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 3), true);
            ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.5f, 1.0f), "Response:");
            ImGui::TextWrapped("%s", response_text.c_str());
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        if (!debug_info.empty()) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.15f, 0.20f, 0.5f));
            ImGui::BeginChild("DebugInfoSection", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 3), true);
            ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.5f, 1.0f), "Debug Info:");
            ImGui::TextWrapped("%s", debug_info.c_str());
            ImGui::EndChild();
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        if (price_watcher) {
            std::string checker_debug = price_watcher->GetDebugOutput();
            std::string last_check = price_watcher->GetLastCheckTimestamp();
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.15f, 0.20f, 0.5f));
            ImGui::BeginChild("PriceCheckerSection", ImVec2(0, 0), true);
            ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Price Checker Status");
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Text("Status: ");
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, checker_enabled ? ImVec4(0.2f, 0.7f, 0.2f, 0.8f) : ImVec4(0.7f, 0.2f, 0.2f, 0.8f));
            ImGui::Button(checker_enabled ? "RUNNING" : "STOPPED", ImVec2(80, 20));
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::Text("Last Check: %s", last_check.c_str());
            ImGui::Spacing();
            ImGui::Text("Debug Output:");
            ImGui::Separator();
            ImGui::TextWrapped("%s", checker_debug.c_str());
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        else {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.15f, 0.15f, 0.5f));
            ImGui::BeginChild("NoPriceCheckerSection", ImVec2(0, 60), true);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20);
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Price Checker: Not running");
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        ImGui::PopFont();
        ImGui::EndChild();

        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 buttonSize = ImVec2(30, 30);
        ImGui::SetCursorPos(ImVec2(windowSize.x - buttonSize.x - 10, windowSize.y - buttonSize.y - 10));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.35f, 0.6f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.45f, 0.7f, 0.9f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.5f, 0.75f, 1.0f));
        if (ImGui::Button("?", buttonSize)) {
            show_credits = true;
        }
        ImGui::PopStyleColor(3);

        ImGui::PopFont();
        ImGui::End();

        // **Add New Asset Popup**
        if (show_add_asset) {
            ImGui::PushFont(regularFont);
            ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
            ImGui::Begin("Add New Asset", &show_add_asset, ImGuiWindowFlags_AlwaysAutoResize);

            ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "Enter Roblox Asset ID:");
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.15f, 0.2f, 1.00f));
            ImGui::InputText("##AssetIDInput", asset_id_input, sizeof(asset_id_input));
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.63f, 0.98f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.53f, 0.94f, 1.0f));
            if (ImGui::Button("Fetch and Add", ImVec2(200, 35))) {
                if (strlen(asset_id_input) > 0 && strlen(roblox_cookie) > 0) {
                    try {
                        long long asset_id = std::stoll(asset_id_input);
                        Asset new_asset = web_handler.fetchAssetInfo(asset_id, roblox_cookie, xsrf_token);

                        bool already_exists = false;
                        for (const auto& asset : assets) {
                            if (asset.id == new_asset.id) {
                                already_exists = true;
                                break;
                            }
                        }

                        if (!already_exists) {
                            new_asset.price_threshold = new_asset.current_price - 50;
                            // Log initial price if valid
                            if (new_asset.current_price >= 0) {
                                auto now = std::chrono::system_clock::now();
                                std::time_t now_time = std::chrono::system_clock::to_time_t(now);
                                struct tm time_info;
                                char time_buffer[32];
                                localtime_s(&time_info, &now_time);
                                strftime(time_buffer, sizeof(time_buffer), "%I:%M:%S %p %d-%b-%Y", &time_info);
                                new_asset.price_history.push_back({ std::string(time_buffer), new_asset.current_price });
                            }
                            std::lock_guard<std::mutex> lock(assets_mutex);
                            assets.push_back(new_asset);
                            ID3D11ShaderResourceView* texture = image_loader.LoadTextureFromURL(new_asset.thumbnail_url);
                            asset_thumbnails.push_back(texture);

                            selected_asset = assets.size() - 1;
                            SaveAssetsCache(assets);

                            if (price_watcher) {
                                price_watcher->Stop();
                                delete price_watcher;
                                price_watcher = new PriceWatcher(roblox_cookie, xsrf_token, webhook_url, assets, check_interval_input);
                                if (checker_enabled) {
                                    price_watcher->Start();
                                }
                            }

                            memset(asset_id_input, 0, sizeof(asset_id_input));
                            show_add_asset = false;
                            debug_info = "Added new asset: " + new_asset.name;
                        }
                        else {
                            debug_info = "Asset already exists in your list";
                        }
                    }
                    catch (const std::exception& e) {
                        debug_info = "Error: " + std::string(e.what());
                    }
                }
                else {
                    debug_info = "Please enter an asset ID and ensure your cookie is valid";
                }
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.25f, 0.25f, 1.0f));
            if (ImGui::Button("Cancel", ImVec2(100, 35))) {
                show_add_asset = false;
                memset(asset_id_input, 0, sizeof(asset_id_input));
            }
            ImGui::PopStyleColor(3);

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "ℹ️ Enter the asset ID from the Roblox URL");
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "   Example: 1117747196 (from roblox.com/catalog/1117747196)");

            ImGui::PopFont();
            ImGui::End();
        }

        // **Credits/About Popup**
        if (show_credits) {
            // Push any custom font if needed (optional)
            ImGui::PushFont(regularFont);

            // Get the size of the main application window
            ImVec2 main_size = ImGui::GetMainViewport()->Size;

            // Calculate 3/5ths of the main window's width
            float desired_width = main_size.x * 3.0f / 5.0f;

            // Set the window size only when it appears
            ImGui::SetNextWindowSize(ImVec2(desired_width, 340), ImGuiCond_Appearing);

            ImGui::Begin("About Roblox Price Watcher", &show_credits); // Removed ImGuiWindowFlags_AlwaysAutoResize

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.2f, 0.4f, 1.0f));
            ImGui::BeginChild("LogoBanner", ImVec2(0, 80), true);
            ImGui::PushFont(largeFont);
            ImGui::SetCursorPos(ImVec2(20, 25));
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Roblox Price Watcher v1.1.1");
            ImGui::PopFont();
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::BeginChild("InfoSection", ImVec2(0, 150), true);
            ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "How to use:");
            ImGui::BulletText("Enter your .ROBLOSECURITY cookie in the Settings tab");
            ImGui::BulletText("Add Roblox limited items using their Asset IDs");
            ImGui::BulletText("Set price thresholds for each item");
            ImGui::BulletText("Enable the price checker to start monitoring");
            ImGui::BulletText("Receive Discord notifications when prices drop below threshold");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "Disclaimer:");
            ImGui::TextWrapped("This application is for educational purposes only. Use at your own risk. Not affiliated with Roblox Corporation.");
            ImGui::EndChild();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.63f, 0.98f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.53f, 0.94f, 1.0f));
            if (ImGui::Button("Close", ImVec2(100, 30))) {
                show_credits = false;
            }
            ImGui::PopStyleColor(3);

            ImGui::SameLine(ImGui::GetWindowWidth() - 120);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.35f, 0.6f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.45f, 0.7f, 0.9f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.5f, 0.75f, 1.0f));
            if (ImGui::Button("GitHub", ImVec2(100, 30))) {
                std::string url = "https://github.com/ProjectBoring/Roblox-Price-Watcher";

                // Convert std::string (UTF-8) to std::wstring (UTF-16)
                int size_needed = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), static_cast<int>(url.size()), nullptr, 0);
                std::wstring wurl(size_needed, 0);
                MultiByteToWideChar(CP_UTF8, 0, url.c_str(), static_cast<int>(url.size()), &wurl[0], size_needed);

                ShellExecuteW(NULL, L"open", wurl.c_str(), NULL, NULL, SW_SHOWNORMAL);
            }
            ImGui::PopStyleColor(3);

            ImGui::PopFont();
            ImGui::End();
        }

        // **Notification Popup**
        if (show_notification) {
            ImGui::PushFont(regularFont);
            ImGui::SetNextWindowSize(ImVec2(400, 180), ImGuiCond_FirstUseEver);
            ImGui::Begin("Notification", &show_notification, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.15f, 0.3f, 0.5f, 0.5f));
            ImGui::BeginChild("NotificationHeader", ImVec2(0, 50), true);
            ImGui::SetCursorPos(ImVec2(20, 15));
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "ℹ️ %s", notification_title.c_str());
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::TextWrapped("%s", notification_message.c_str());
            ImGui::Spacing();
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 100) / 2);

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.63f, 0.98f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.53f, 0.94f, 1.0f));
            if (ImGui::Button("OK", ImVec2(100, 30))) {
                show_notification = false;
            }
            ImGui::PopStyleColor(3);

            ImGui::PopFont();
            ImGui::End();
        }

        // **Price History Graph Popup**
        if (show_price_history && selected_asset >= 0 && selected_asset < assets.size()) {
            ImGui::PushFont(regularFont);
            ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
            ImGui::Begin("Price History", &show_price_history, ImGuiWindowFlags_NoCollapse);

            Asset& asset = assets[selected_asset];
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.27f, 0.46f, 0.4f));
            ImGui::BeginChild("GraphTitle", ImVec2(0, 40), true);

            // Format the text and calculate its size
            const char* text = "%s - Price History";
            char buffer[256]; // Adjust size if asset.name could be longer
            sprintf_s(buffer, sizeof(buffer), text, asset.name.c_str());
            ImVec2 text_size = ImGui::CalcTextSize(buffer);

            // Get the available space in the child window (accounting for padding)
            ImVec2 content_region = ImGui::GetContentRegionAvail();

            // Calculate centered position
            float pos_x = (content_region.x - text_size.x) * 0.5f; // Center horizontally
            float pos_y = (40.0f - text_size.y) * 0.5f;           // Center vertically (40 is the child window height)

            // Set the cursor position
            ImGui::SetCursorPos(ImVec2(pos_x, pos_y));

            // Render the centered text
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%s - Price History", asset.name.c_str());

            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::BeginChild("GraphArea", ImVec2(0, 0), true);

            if (!asset.price_history.empty()) {
                int min_price = INT_MAX;
                int max_price = 0;
                for (const auto& entry : asset.price_history) {
                    min_price = min(min_price, entry.second);
                    max_price = max(max_price, entry.second);
                }

                if (min_price == max_price) {
                    min_price = min_price > 50 ? min_price - 50 : 0;
                    max_price += 50;
                }
                else {
                    int range = max_price - min_price;
                    min_price = max(0, min_price - range / 10);
                    max_price += range / 10;
                }

                float graph_height = ImGui::GetContentRegionAvail().y - 50;
                float graph_width = ImGui::GetContentRegionAvail().x - 60;
                ImVec2 graph_pos = ImGui::GetCursorScreenPos();
                graph_pos.x += 50;

                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                ImU32 grid_color = IM_COL32(100, 100, 100, 40);
                ImU32 axis_color = IM_COL32(150, 150, 150, 255);

                int y_step = (max_price - min_price) / 5;
                if (y_step <= 0) y_step = 50;

                for (int y = 0; y <= 5; y++) {
                    float y_pos = graph_pos.y + graph_height * (1.0f - (float)y / 5.0f);
                    int price_label = min_price + y_step * y;
                    draw_list->AddLine(ImVec2(graph_pos.x, y_pos), ImVec2(graph_pos.x + graph_width, y_pos), grid_color);
                    char price_text[32];
                    sprintf_s(price_text, "%d R$", price_label);
                    draw_list->AddText(ImVec2(graph_pos.x - 45, y_pos - 7), IM_COL32(200, 200, 200, 255), price_text);
                }

                draw_list->AddLine(ImVec2(graph_pos.x, graph_pos.y), ImVec2(graph_pos.x, graph_pos.y + graph_height), axis_color);
                draw_list->AddLine(ImVec2(graph_pos.x, graph_pos.y + graph_height), ImVec2(graph_pos.x + graph_width, graph_pos.y + graph_height), axis_color);

                if (asset.price_history.size() > 1) {
                    for (size_t i = 0; i < asset.price_history.size() - 1; i++) {
                        float x1 = graph_pos.x + (i * graph_width) / (asset.price_history.size() - 1);
                        float y1 = graph_pos.y + graph_height * (1.0f - (float)(asset.price_history[i].second - min_price) / (max_price - min_price));
                        float x2 = graph_pos.x + ((i + 1) * graph_width) / (asset.price_history.size() - 1);
                        float y2 = graph_pos.y + graph_height * (1.0f - (float)(asset.price_history[i + 1].second - min_price) / (max_price - min_price));
                        draw_list->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(41, 151, 255, 255), 2.0f);
                        draw_list->AddCircleFilled(ImVec2(x1, y1), 4.0f, IM_COL32(66, 150, 250, 255));
                    }
                    int last_idx = asset.price_history.size() - 1;
                    float x = graph_pos.x + (last_idx * graph_width) / (asset.price_history.size() - 1);
                    float y = graph_pos.y + graph_height * (1.0f - (float)(asset.price_history[last_idx].second - min_price) / (max_price - min_price));
                    draw_list->AddCircleFilled(ImVec2(x, y), 4.0f, IM_COL32(66, 150, 250, 255));
                }

                if (asset.price_history.size() > 1) {
                    int num_labels = min(5, static_cast<int>(asset.price_history.size()));
                    for (int i = 0; i < num_labels; i++) {
                        int idx = (i * (asset.price_history.size() - 1)) / (num_labels - 1);
                        float x_pos = graph_pos.x + (idx * graph_width) / (asset.price_history.size() - 1);
                        std::string date = asset.price_history[idx].first.substr(0, 10);
                        draw_list->AddText(ImVec2(x_pos - 20, graph_pos.y + graph_height + 10), IM_COL32(200, 200, 200, 255), date.c_str());
                    }
                }

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + graph_height + 50);
            }
            else {
                ImGui::SetCursorPos(ImVec2((ImGui::GetWindowWidth() - 200) / 2, ImGui::GetWindowHeight() / 2 - 20));
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "No price history data available.");
            }

            ImGui::EndChild();
            ImGui::PopFont();
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        float clear_color[] = { 0.1f, 0.1f, 0.1f, 1.0f };
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    // Cleanup
    if (price_watcher) {
        price_watcher->Stop();
        delete price_watcher;
    }
    for (auto* texture : asset_thumbnails) {
        if (texture) texture->Release();
    }
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

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
