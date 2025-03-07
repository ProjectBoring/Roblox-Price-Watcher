#ifndef PRICE_WATCHING_ENERGY_SAVER_H
#define PRICE_WATCHING_ENERGY_SAVER_H

#include <string>
#include <vector>
#include <windows.h>
#include <thread>
#include <nlohmann/json.hpp>
#include <mutex>

struct Asset {
    long long id;
    std::string name;
    int price_threshold;
    std::string thumbnail_url;
    int current_price = -1; // -1 means price not yet fetched
    bool notified = false;
};

// Get the executable's directory
std::wstring GetExecutableDirectory();

// Save assets to a cache file next to the executable
bool SaveAssetsCache(const std::vector<Asset>& assets);

// Load assets from the cache file
std::vector<Asset> LoadAssetsCache();

// Save the Roblox cookie to a cache file
bool SaveCookieCache(const std::string& cookie);

// Load the Roblox cookie from the cache file
std::string LoadCookieCache();

bool SaveWebhookCache(const std::string& webhook_url);

std::string LoadWebhookCache();

std::vector<Asset> LoadAssetsCache(const std::string& cookie, const std::string& xsrf_token);

class PriceWatcher {
public:
    PriceWatcher(const std::string& cookie, const std::string& token, const std::string& webhook, std::vector<Asset>& assets_ref);
    ~PriceWatcher();
    void Start();
    void Stop();
    std::string GetDebugOutput() const; // New method to retrieve debug output
    std::string GetLastCheckTimestamp() const; // New method for timestamp

private:
    std::string roblox_cookie;
    std::string xsrf_token;
    std::string webhook_url;
    std::vector<Asset>& assets;
    bool running;
    std::thread checker_thread;
    mutable std::mutex debug_mutex; // Protect debug data
    std::string debug_output; // Store debug messages
    std::string last_check_timestamp; // Store last check time

    void PriceCheckLoop();
};

void FetchCurrentPrice(Asset& asset, const std::string& cookie, const std::string& xsrf_token);
#endif // PRICE_WATCHING_ENERGY_SAVER_H