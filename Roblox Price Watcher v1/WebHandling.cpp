#include "WebHandling.h"
#include <iostream>

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