#ifndef PRICE_WATCHING_ENERGY_SAVER_H
#define PRICE_WATCHING_ENERGY_SAVER_H

#include <string>
#include <vector>
#include <windows.h>
#include <thread>
#include <nlohmann/json.hpp>
#include <mutex>
#include <chrono>          // Already included for std::chrono::seconds
#include <condition_variable> // Added for std::condition_variable
#include <shlwapi.h>       // For PathCombine
#pragma comment(lib, "shlwapi.lib")

// Assume these are defined in PriceWatchingEnergySaver.h
struct Asset {
    long long id;
    std::string name;
    int price_threshold;
    std::string thumbnail_url;
    int current_price;
    bool notified;
    std::vector<std::pair<std::string, int>> price_history; // timestamp, price
};

extern std::mutex assets_mutex;

extern std::wstring CombinePaths(const std::wstring& dir, const std::wstring& file);

// Get the executable's directory
std::wstring GetExecutableDirectory();

// Save assets to a cache file next to the executable
bool SaveAssetsCache(const std::vector<Asset>& assets);

// Load assets from the cache file
std::vector<Asset> LoadAssetsCache(const std::string& cookie, const std::string& xsrf_token);

// Save the Roblox cookie to a cache file
bool SaveCookieCache(const std::string& cookie);

// Load the Roblox cookie from the cache file
std::string LoadCookieCache();

bool SaveWebhookCache(const std::string& webhook_url);

std::string LoadWebhookCache();

class PriceWatcher {
public:
    PriceWatcher(const std::string& cookie, const std::string& token, const std::string& webhook, std::vector<Asset>& assets_ref, int check_interval);
    ~PriceWatcher();
    void Start();
    void Stop();
    std::string GetDebugOutput() const;
    std::string GetLastCheckTimestamp() const;
    std::chrono::steady_clock::time_point next_check_time;
    bool running;
    mutable std::mutex debug_mutex;
    int check_interval_seconds;
private:
    std::string roblox_cookie;
    std::string xsrf_token;
    std::string webhook_url;
    std::vector<Asset>& assets;
    std::thread checker_thread;
    std::string debug_output;
    std::string last_check_timestamp;
    std::mutex cv_mutex;          // Added for condition variable synchronization
    std::condition_variable cv;   // Added to manage thread waiting and stopping
    void PriceCheckLoop();
};

void FetchCurrentPrice(Asset& asset, const std::string& cookie, const std::string& xsrf_token);

#endif // PRICE_WATCHING_ENERGY_SAVER_H
