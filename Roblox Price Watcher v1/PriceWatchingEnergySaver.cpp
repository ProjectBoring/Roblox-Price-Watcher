#include "PriceWatchingEnergySaver.h"
#include "WebHandling.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <nlohmann/json.hpp>
#include <future> // For std::async
#include <vector>

// Helper function to get the executable directory
std::wstring GetExecutableDirectory() {
    wchar_t path[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(NULL, path, MAX_PATH) == 0) {
        std::wcerr << L"Failed to get executable directory." << std::endl;
        return L"";
    }
    std::wstring fullPath(path);
    size_t pos = fullPath.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? fullPath.substr(0, pos) : fullPath;
}

// Cache the executable directory to avoid repeated system calls
static const std::wstring exeDir = []() {
    wchar_t path[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(NULL, path, MAX_PATH) == 0) {
        std::wcerr << L"Failed to get executable directory." << std::endl;
        return std::wstring(L"");
    }
    std::wstring fullPath(path);
    size_t pos = fullPath.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? fullPath.substr(0, pos) : fullPath;
    }();

// Helper function to combine paths safely
std::wstring CombinePaths(const std::wstring& dir, const std::wstring& file) {
    wchar_t buffer[MAX_PATH];
    if (PathCombineW(buffer, dir.c_str(), file.c_str()) == NULL) {
        std::wcerr << L"Failed to combine paths." << std::endl;
        return L"";
    }
    return std::wstring(buffer);
}

// Save and load functions
bool SaveAssetsCache(const std::vector<Asset>& assets) {
    std::wstring exeDir = GetExecutableDirectory();
    if (exeDir.empty()) return false;
    std::wstring cachePath = CombinePaths(exeDir, L"AssetsCache.txt");
    std::ofstream outFile(cachePath, std::ios::trunc);
    if (!outFile) {
        std::cerr << "Failed to open assets cache file for writing." << std::endl;
        return false;
    }
    for (const auto& asset : assets) {
        outFile << asset.id << "," << asset.name << "," << asset.price_threshold << "," << asset.thumbnail_url;
        // Append price history
        outFile << ",";
        for (size_t i = 0; i < asset.price_history.size(); ++i) {
            outFile << asset.price_history[i].first << ":" << asset.price_history[i].second;
            if (i < asset.price_history.size() - 1) outFile << ";";
        }
        outFile << "\n";
    }
    return true;
}

std::vector<Asset> LoadAssetsCache(const std::string& cookie, const std::string& xsrf_token) {
    std::vector<Asset> assets;
    std::wstring exeDir = GetExecutableDirectory();
    if (exeDir.empty()) return assets;
    std::wstring cachePath = CombinePaths(exeDir, L"AssetsCache.txt");
    std::ifstream inFile(cachePath);
    if (!inFile) return assets;
    std::string line;
    while (std::getline(inFile, line)) {
        std::stringstream ss(line);
        std::string id_str, name, price_str, thumbnail, history_str;
        std::getline(ss, id_str, ',');
        std::getline(ss, name, ',');
        std::getline(ss, price_str, ',');
        std::getline(ss, thumbnail, ',');
        std::getline(ss, history_str); // Remaining part is price history

        Asset asset{ std::stoll(id_str), name, std::stoi(price_str), thumbnail, -1, false, {} };

        // Parse price history
        if (!history_str.empty()) {
            std::stringstream history_ss(history_str);
            std::string entry;
            while (std::getline(history_ss, entry, ';')) {
                size_t colon_pos = entry.find(':');
                if (colon_pos != std::string::npos) {
                    std::string timestamp = entry.substr(0, colon_pos);
                    int price = std::stoi(entry.substr(colon_pos + 1));
                    asset.price_history.push_back({ timestamp, price });
                }
            }
        }

        assets.push_back(asset);
    }
    return assets;
}

bool SaveCookieCache(const std::string& cookie) {
    std::wstring exeDir = GetExecutableDirectory();
    if (exeDir.empty()) return false;
    std::wstring cachePath = CombinePaths(exeDir, L"CookieCache.txt");
    std::ofstream outFile(cachePath, std::ios::trunc);
    if (!outFile) {
        std::cerr << "Failed to save cookie cache." << std::endl;
        return false;
    }
    outFile << cookie;
    return true;
}

bool SaveWebhookCache(const std::string& webhook) {
    std::wstring exeDir = GetExecutableDirectory();
    if (exeDir.empty()) return false;
    std::wstring cachePath = CombinePaths(exeDir, L"WebhookCache.txt");
    std::ofstream outFile(cachePath, std::ios::trunc);
    if (!outFile) {
        std::cerr << "Failed to save webhook cache." << std::endl;
        return false;
    }
    outFile << webhook;
    return true;
}

// Load the Roblox cookie from the cache file
std::string LoadCookieCache() {
    if (exeDir.empty()) return "";
    std::wstring cachePath = CombinePaths(exeDir, L"CookieCache.txt");
    std::ifstream inFile(cachePath);
    if (!inFile) return "";
    std::stringstream ss;
    ss << inFile.rdbuf();
    return ss.str();
}

// Save webhook messages to a file
void SaveWebhookToFile(const std::string& message) {
    if (exeDir.empty()) {
        std::cerr << "Failed to get executable directory for webhook log." << std::endl;
        return;
    }
    std::wstring logPath = CombinePaths(exeDir, L"WebhookLog.txt");
    std::ofstream outFile(logPath, std::ios::app);
    if (!outFile) {
        std::cerr << "Failed to open WebhookLog.txt for writing." << std::endl;
        return;
    }
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    struct tm time_info;
    char time_buffer[32];
    localtime_s(&time_info, &now_time);
    strftime(time_buffer, sizeof(time_buffer), "[%I:%M:%S %p %d-%b-%Y]", &time_info); // Matches [11:32:33 PM 01-Mar-2025]
    outFile << time_buffer << " " << message << "\n";
}

// Load the webhook URL from cache
std::string LoadWebhookCache() {
    if (exeDir.empty()) return "";
    std::wstring cachePath = CombinePaths(exeDir, L"WebhookCache.txt");
    std::ifstream inFile(cachePath);
    if (!inFile) return "";
    std::stringstream ss;
    ss << inFile.rdbuf();
    return ss.str();
}

// PriceWatcher implementation
PriceWatcher::PriceWatcher(const std::string& cookie, const std::string& token, const std::string& webhook, std::vector<Asset>& assets_ref, int check_interval)
    : roblox_cookie(cookie), xsrf_token(token), webhook_url(webhook), assets(assets_ref), running(false), check_interval_seconds(check_interval) {
}

PriceWatcher::~PriceWatcher() {
    Stop(); // Ensures the thread is stopped and joined before destruction
}

void PriceWatcher::Start() {
    if (!running && !xsrf_token.empty()) {
        running = true;
        {
            std::lock_guard<std::mutex> lock(debug_mutex);
            next_check_time = std::chrono::steady_clock::now() + std::chrono::seconds(check_interval_seconds);
        }
        checker_thread = std::thread(&PriceWatcher::PriceCheckLoop, this);
        // Note: Thread is NOT detached; it remains joinable for proper cleanup
    }
}

void PriceWatcher::Stop() {
    running = false;
    cv.notify_all(); // Notify the condition variable to wake up the waiting thread
    if (checker_thread.joinable()) {
        checker_thread.join(); // Wait for the thread to finish execution
    }
}

std::string PriceWatcher::GetDebugOutput() const {
    std::lock_guard<std::mutex> lock(debug_mutex);
    return debug_output;
}

std::string PriceWatcher::GetLastCheckTimestamp() const {
    std::lock_guard<std::mutex> lock(debug_mutex);
    return last_check_timestamp;
}

struct PriceUpdate {
    long long id;
    int price;
};

void PriceWatcher::PriceCheckLoop() {
    std::cerr << "PriceCheckLoop: Entering function" << std::endl;
    while (running) {
        {
            std::lock_guard<std::mutex> lock(debug_mutex);
            next_check_time = std::chrono::steady_clock::now() + std::chrono::seconds(check_interval_seconds);
        }
        {
            std::unique_lock<std::mutex> lock(cv_mutex);
            if (cv.wait_for(lock, std::chrono::seconds(check_interval_seconds), [this] { return !running; })) {
                break;
            }
        }
        if (!running) break;

        std::ostringstream debug_stream;
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        struct tm time_info;
        char time_buffer[32];
        localtime_s(&time_info, &now_time);
        strftime(time_buffer, sizeof(time_buffer), "%I:%M:%S %p %d-%b-%Y", &time_info);
        std::string timestamp = time_buffer;
        debug_stream << "Price Check at " << timestamp << ":\n";

        std::vector<std::future<PriceUpdate>> futures;
        {
            std::lock_guard<std::mutex> lock(assets_mutex);
            for (const auto& asset : assets) {
                long long asset_id = asset.id;
                futures.push_back(std::async(std::launch::async, [asset_id, this]() {
                    WebHandler web;
                    std::string url = "https://catalog.roblox.com/v1/catalog/items/details";
                    std::string json_body = R"({"items":[{"itemType":1,"id":)" + std::to_string(asset_id) + R"(}]})";
                    std::string response;
                    try {
                        response = web.post(url, json_body, roblox_cookie, xsrf_token);
                        if (!response.empty()) {
                            nlohmann::json json_response = nlohmann::json::parse(response);
                            if (json_response.contains("data") && !json_response["data"].empty() &&
                                json_response["data"][0].contains("lowestPrice")) {
                                return PriceUpdate{ asset_id, json_response["data"][0]["lowestPrice"].get<int>() };
                            }
                        }
                    }
                    catch (const std::exception& e) {
                        std::cerr << "Error fetching price for asset " << asset_id << ": " << e.what() << std::endl;
                    }
                    return PriceUpdate{ asset_id, -1 };
                    }));
            }
        }

        {
            std::lock_guard<std::mutex> lock(assets_mutex);
            for (auto& future : futures) {
                PriceUpdate update = future.get();
                for (auto& asset : assets) {
                    if (asset.id == update.id) {
                        int previous_price = asset.current_price;
                        asset.current_price = update.price;
                        std::string price_status;

                        // Log price history if price is valid and different from the last recorded price
                        if (update.price >= 0) {
                            if (asset.price_history.empty() || asset.price_history.back().second != update.price) {
                                asset.price_history.push_back({ timestamp, update.price });
                                SaveAssetsCache(assets);
                            }
                        }

                        if (previous_price == -1) {
                            price_status = "Initial fetch";
                        }
                        else if (asset.current_price < previous_price) {
                            price_status = "Price dropped from " + std::to_string(previous_price) + " to " + std::to_string(asset.current_price);
                        }
                        else if (asset.current_price > previous_price) {
                            price_status = "Price increased from " + std::to_string(previous_price) + " to " + std::to_string(asset.current_price);
                        }
                        else {
                            price_status = "No change";
                        }
                        debug_stream << "Asset '" << asset.name << "' (ID: " << asset.id << "): "
                            << "Current Price: " << asset.current_price << ", "
                            << "Threshold: " << asset.price_threshold << ", "
                            << "Status: " << price_status << "\n";

                        if (asset.current_price > 0 && asset.current_price < asset.price_threshold && !asset.notified) {
                            // Format the webhook message with proper escaping for Discord
                            std::string webhook_content = "@everyone\\nAsset '" + asset.name +
                                "' is below threshold!\\nPrice Lowered To: R$ " +
                                std::to_string(asset.current_price) +
                                " (Threshold: R$ " + std::to_string(asset.price_threshold) +
                                ")\\nCatalog Link: https://www.roblox.com/catalog/" +
                                std::to_string(asset.id) + "/";
                            std::string webhook_body = R"({"content": ")" + webhook_content + R"("})";
                            WebHandler web;
                            web.post(webhook_url, webhook_body, "", "");
                            SaveWebhookToFile(webhook_body); // Log in the specified format
                            asset.notified = true;
                            debug_stream << "*** Notification sent for '" << asset.name << "'\n";
                        }
                        else if (asset.current_price >= asset.price_threshold) {
                            asset.notified = false;
                        }
                        break;
                    }
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(debug_mutex);
            debug_output = debug_stream.str();
            last_check_timestamp = timestamp;
        }
    }
    std::cerr << "PriceCheckLoop: Exiting function" << std::endl;
}

void FetchCurrentPrice(Asset& asset, const std::string& cookie, const std::string& xsrf_token) {
    // This function is no longer used directly in PriceCheckLoop, but kept for compatibility
    WebHandler web;
    std::string url = "https://catalog.roblox.com/v1/catalog/items/details";
    std::string json_body = R"({"items":[{"itemType":1,"id":)" + std::to_string(asset.id) + R"(}]})";
    try {
        std::string response = web.post(url, json_body, cookie, xsrf_token);
        if (!response.empty()) {
            nlohmann::json json_response = nlohmann::json::parse(response);
            if (json_response.contains("data") && !json_response["data"].empty() &&
                json_response["data"][0].contains("lowestPrice")) {
                asset.current_price = json_response["data"][0]["lowestPrice"].get<int>();
            }
            else {
                asset.current_price = -1;
            }
        }
        else {
            asset.current_price = -1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "FetchCurrentPrice: Exception: " << e.what() << std::endl;
        asset.current_price = -1;
    }
}
