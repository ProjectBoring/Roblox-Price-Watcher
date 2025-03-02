#ifndef WEB_HANDLING_H
#define WEB_HANDLING_H

#include <string>
#include <curl/curl.h>

class WebHandler {
public:
    WebHandler();
    ~WebHandler();
    std::string post(const std::string& url, const std::string& json_body, const std::string& cookie = "", const std::string& xsrf_token = "");
    std::string get(const std::string& url); // Added GET method
    std::string getXSRFToken(const std::string& cookie);

private:
    CURL* curl;
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* data);
    static size_t headerCallback(void* contents, size_t size, size_t nmemb, std::string* data);
};

#endif // WEB_HANDLING_H