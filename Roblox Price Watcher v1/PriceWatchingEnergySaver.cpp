#include "PriceWatchingEnergySaver.h"
#include "WebHandling.h" // Assuming this is where WebHandler is defined
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <chrono>

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

bool SaveAssetsCache(const std::vector<Asset>& assets) {
    std::wstring exeDir = GetExecutableDirectory();
    if (exeDir.empty()) return false;

    std::wstring cachePath = exeDir + L"\\AssetsCache.txt";
    std::ofstream outFile(cachePath, std::ios::trunc);
    if (!outFile) {
        std::cerr << "Failed to open assets cache file for writing." << std::endl;
        return false;
    }

    for (const auto& asset : assets) {
        outFile << asset.id << "," << asset.name << "," << asset.price_threshold << "," << asset.thumbnail_url << "\n";
    }
    outFile.close();
    return true;
}

std::vector<Asset> LoadAssetsCache(const std::string& cookie, const std::string& xsrf_token) {
    std::vector<Asset> assets;
    std::wstring exeDir = GetExecutableDirectory();
    if (exeDir.empty()) return assets;

    std::wstring cachePath = exeDir + L"\\AssetsCache.txt";
    std::ifstream inFile(cachePath);
    if (!inFile) return assets;

    std::string line;
    while (std::getline(inFile, line)) {
        std::stringstream ss(line);
        std::string id_str, name, price_str, thumbnail;
        std::getline(ss, id_str, ',');
        std::getline(ss, name, ',');
        std::getline(ss, price_str, ',');
        std::getline(ss, thumbnail);

        Asset asset;
        asset.id = std::stoll(id_str);
        asset.name = name;
        asset.price_threshold = std::stoi(price_str);
        asset.thumbnail_url = thumbnail;
        asset.current_price = -1; // Initialize; fetch below
        assets.push_back(asset);
    }
    inFile.close();

    // Fetch current prices for all loaded assets
    for (auto& asset : assets) {
        FetchCurrentPrice(asset, cookie, xsrf_token);
    }

    return assets;
}

bool SaveCookieCache(const std::string& cookie) {
    std::wstring exeDir = GetExecutableDirectory();
    if (exeDir.empty()) return false;

    std::wstring cachePath = exeDir + L"\\CookieCache.txt";
    std::ofstream outFile(cachePath, std::ios::trunc);
    if (!outFile) {
        std::cerr << "Failed to open cookie cache file for writing." << std::endl;
        return false;
    }

    outFile << cookie;
    outFile.close();
    return true;
}

std::string LoadCookieCache() {
    std::wstring exeDir = GetExecutableDirectory();
    if (exeDir.empty()) return "";

    std::wstring cachePath = exeDir + L"\\CookieCache.txt";
    std::ifstream inFile(cachePath);
    if (!inFile) return "";

    std::stringstream ss;
    ss << inFile.rdbuf();
    inFile.close();
    return ss.str();
}

// New function to save webhook messages to a file
void SaveWebhookToFile(const std::string& message) {
    std::wstring exeDir = GetExecutableDirectory();
    if (exeDir.empty()) {
        std::cerr << "Failed to get executable directory for webhook log." << std::endl;
        return;
    }

    std::wstring logPath = exeDir + L"\\WebhookLog.txt";
    std::ofstream outFile(logPath, std::ios::app); // Append mode
    if (!outFile) {
        std::cerr << "Failed to open WebhookLog.txt for writing." << std::endl;
        return;
    }

    // Get current timestamp
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    struct tm time_info; // Declare time_info here
    char time_buffer[32]; // Buffer size large enough for custom format
    localtime_s(&time_info, &now_time); // Populate time_info
    strftime(time_buffer, sizeof(time_buffer), "%I:%M:%S %p %d-%b-%Y", &time_info); // Format timestamp
    std::string timestamp(time_buffer);

    outFile << "[" << timestamp << "] " << message << "\n";
    outFile.close();
}

// New function to save the webhook URL to cache
bool SaveWebhookCache(const std::string& webhook_url) {
    std::wstring exeDir = GetExecutableDirectory();
    if (exeDir.empty()) return false;

    std::wstring cachePath = exeDir + L"\\WebhookCache.txt";
    std::ofstream outFile(cachePath, std::ios::trunc); // Overwrite mode
    if (!outFile) {
        std::cerr << "Failed to open WebhookCache.txt for writing." << std::endl;
        return false;
    }

    outFile << webhook_url;
    outFile.close();
    return true;
}

// New function to load the webhook URL from cache
std::string LoadWebhookCache() {
    std::wstring exeDir = GetExecutableDirectory();
    if (exeDir.empty()) return "";

    std::wstring cachePath = exeDir + L"\\WebhookCache.txt";
    std::ifstream inFile(cachePath);
    if (!inFile) return ""; // Return empty string if file doesn't exist

    std::stringstream ss;
    ss << inFile.rdbuf();
    inFile.close();
    return ss.str();
}

PriceWatcher::PriceWatcher(const std::string& cookie, const std::string& token, const std::string& webhook, std::vector<Asset>& assets_ref)
    : roblox_cookie(cookie), xsrf_token(token), webhook_url(webhook), assets(assets_ref), running(false) {
}

PriceWatcher::~PriceWatcher() {
    Stop();
}

void PriceWatcher::Start() {
    if (!running && !xsrf_token.empty()) {
        running = true;
        checker_thread = std::thread(&PriceWatcher::PriceCheckLoop, this);
        checker_thread.detach();
    }
}

void PriceWatcher::Stop() {
    running = false;
}

std::string PriceWatcher::GetDebugOutput() const {
    std::lock_guard<std::mutex> lock(debug_mutex);
    return debug_output;
}

std::string PriceWatcher::GetLastCheckTimestamp() const {
    std::lock_guard<std::mutex> lock(debug_mutex);
    return last_check_timestamp;
}

void PriceWatcher::PriceCheckLoop() {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(25)); // Check every minute
        WebHandler web;
        std::ostringstream debug_stream;

        // Get current timestamp in 12-hour format
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        struct tm time_info;
        char time_buffer[32];
        localtime_s(&time_info, &now_time);
        strftime(time_buffer, sizeof(time_buffer), "%I:%M:%S %p %d-%b-%Y", &time_info);
        std::string timestamp = time_buffer;

        debug_stream << "Price Check at " << timestamp << ":\n";

        for (size_t i = 0; i < assets.size(); ++i) {
            Asset& asset = assets[i];
            int previous_price = asset.current_price;

            std::string url = "https://catalog.roblox.com/v1/catalog/items/details";
            std::string json_body = R"({"items":[{"itemType":1,"id":)" + std::to_string(asset.id) + R"(}]})";
            std::string response = web.post(url, json_body, roblox_cookie, xsrf_token);

            if (!response.empty()) {
                try {
                    nlohmann::json json_response = nlohmann::json::parse(response);
                    if (json_response.contains("data") && !json_response["data"].empty()) {
                        asset.current_price = json_response["data"][0]["lowestPrice"].get<int>();

                        std::string price_status;
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
                            // Updated webhook format with catalog link and proper escaping
                            std::string webhook_content = "@here\\nAsset '" + asset.name +
                                "' is below threshold!\\nPrice Lowered To: R$ " +
                                std::to_string(asset.current_price) +
                                " (Threshold: R$ " + std::to_string(asset.price_threshold) +
                                ")\\nCatalog Link: https://www.roblox.com/catalog/" +
                                std::to_string(asset.id) + "/";
                            std::string webhook_body = R"({"content":")" + webhook_content + R"("})";
                            web.post(webhook_url, webhook_body, "", "");
                            SaveWebhookToFile(webhook_body);
                            asset.notified = true;
                            debug_stream << "*** Notification sent for '" << asset.name << "'\n";
                        }
                        else if (asset.current_price >= asset.price_threshold) {
                            asset.notified = false;
                        }
                    }
                    else {
                        debug_stream << "Asset '" << asset.name << "' (ID: " << asset.id << "): No price data available\n";
                    }
                }
                catch (const std::exception& e) {
                    debug_stream << "Asset '" << asset.name << "' (ID: " << asset.id << "): JSON parsing error - " << e.what() << "\n";
                }
            }
            else {
                debug_stream << "Asset '" << asset.name << "' (ID: " << asset.id << "): Failed to fetch price data\n";
            }
        }

        {
            std::lock_guard<std::mutex> lock(debug_mutex);
            debug_output = debug_stream.str();
            last_check_timestamp = timestamp;
        }
    }
}

void FetchCurrentPrice(Asset& asset, const std::string& cookie, const std::string& xsrf_token) {
    WebHandler web; // Assuming WebHandler is your class for HTTP requests
    std::string url = "https://catalog.roblox.com/v1/catalog/items/details";
    std::string json_body = R"({"items":[{"itemType":1,"id":)" + std::to_string(asset.id) + R"(}]})";

    std::string response = web.post(url, json_body, cookie, xsrf_token);

    if (!response.empty()) {
        try {
            nlohmann::json json_response = nlohmann::json::parse(response);
            if (json_response.contains("data") && !json_response["data"].empty()) {
                int lowest_price = json_response["data"][0]["lowestPrice"].get<int>();
                asset.current_price = lowest_price;
            }
            else {
                std::cerr << "No price data found for asset ID: " << asset.id << std::endl;
                asset.current_price = -1; // Reset to -1 if no data
            }
        }
        catch (const std::exception& e) {
            std::cerr << "JSON parsing error: " << e.what() << std::endl;
            asset.current_price = -1; // Reset on error
        }
    }
    else {
        std::cerr << "Failed to fetch price for asset ID: " << asset.id << std::endl;
        asset.current_price = -1; // Reset on failure
    }
}