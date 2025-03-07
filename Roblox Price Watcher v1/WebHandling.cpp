#include "WebHandling.h"
#include <iostream>
#include "PriceWatchingEnergySaver.h"

WebHandler::WebHandler() {
    curl = curl_easy_init();
}

WebHandler::~WebHandler() {
    if (curl) curl_easy_cleanup(curl);
}

std::string WebHandler::post(const std::string& url, const std::string& json_body, const std::string& cookie, const std::string& xsrf_token) {
    std::string response;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "Accept: application/json");
        if (!xsrf_token.empty()) {
            headers = curl_slist_append(headers, ("X-CSRF-TOKEN: " + xsrf_token).c_str());
        }
        if (!cookie.empty()) {
            headers = curl_slist_append(headers, ("Cookie: " + cookie).c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        if (res != CURLE_OK) {
            std::cerr << "CURL error (post): " << curl_easy_strerror(res) << std::endl;
            return "Error: Failed to fetch data.";
        }
    }
    return response;
}

std::string WebHandler::get(const std::string& url) {
    std::string response;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L); // Set to GET explicitly
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects if any

        // Minimal headers for GET (optional, adjust as needed)
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Accept: application/octet-stream"); // For binary image data
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        if (res != CURLE_OK) {
            std::cerr << "CURL error (get): " << curl_easy_strerror(res) << std::endl;
            return "";
        }
    }
    return response;
}

std::string WebHandler::getXSRFToken(const std::string& cookie) {
    std::string token;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://auth.roblox.com/v2/login");
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &token);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, headerCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &token);

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, ("Cookie: " + cookie).c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);

        if (res == CURLE_OK) {
            size_t pos = token.find("x-csrf-token: ");
            if (pos != std::string::npos) {
                pos += 14;
                size_t end_pos = token.find("\r\n", pos);
                token = token.substr(pos, end_pos - pos);
            }
        }
        else {
            std::cerr << "CURL error (getXSRFToken): " << curl_easy_strerror(res) << std::endl;
        }
    }
    return token;
}

Asset WebHandler::fetchAssetInfo(long long asset_id, const std::string& cookie, const std::string& xsrf_token) {
    Asset asset;
    asset.id = asset_id;
    asset.current_price = -1; // Initialize to -1

    std::string url = "https://catalog.roblox.com/v1/catalog/items/details";
    std::string json_body = R"({"items":[{"itemType":1,"id":)" + std::to_string(asset_id) + R"(}]})";

    std::string response = post(url, json_body, cookie, xsrf_token);

    if (!response.empty()) {
        try {
            nlohmann::json json_response = nlohmann::json::parse(response);
            if (json_response.contains("data") && !json_response["data"].empty()) {
                auto& data = json_response["data"][0];

                asset.name = data.value("name", "Unknown Asset"); // Use value() with default

                // Get the price.  Prioritize "lowestPrice", then "price", then default to -1.
                if (data.contains("lowestPrice") && !data["lowestPrice"].is_null()) {
                    asset.current_price = data["lowestPrice"].get<int>();
                }
                else if (data.contains("price") && !data["price"].is_null()) {
                    asset.current_price = data["price"].get<int>();
                }
                else {
                    asset.current_price = -1; // No price available
                }

                asset.thumbnail_url = "https://thumbnails.roblox.com/v1/assets?assetIds=" + std::to_string(asset_id) + "&size=150x150&format=Png";
                asset.notified = false; // Initialize to false
            }
            else
            {
                std::cerr << "fetchAssetInfo: No data found in response for asset ID: " << asset_id << std::endl;
            }
        }
        catch (const std::exception& e) {
            std::cerr << "fetchAssetInfo: JSON parsing error: " << e.what() << std::endl;
            //  Return the asset with default values (id set, name "Unknown", price -1)
        }
    }
    else
    {
        std::cerr << "fetchAssetInfo: Empty response from post request for asset ID: " << asset_id << std::endl;
    }

    return asset;
}

size_t WebHandler::writeCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t totalSize = size * nmemb;
    data->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

size_t WebHandler::headerCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t totalSize = size * nmemb;
    data->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}
