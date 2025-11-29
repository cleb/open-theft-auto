#include "AzureTextureGenerator.hpp"

#include "Base64.hpp"

#include <curl/curl.h>

#include <fstream>
#include <sstream>
#include <string>

namespace {
size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string buildRequestUrl(const AzureTextureGenerator::Config& config) {
    std::string url = config.endpoint;
    if (!url.empty() && url.back() == '/') {
        url.pop_back();
    }
    url += "/openai/deployments/" + config.deployment + "/images/generations?api-version=" + config.apiVersion;
    return url;
}
}

bool AzureTextureGenerator::isConfigured(const Config& config) const {
    return !config.endpoint.empty() && !config.deployment.empty() && !config.apiKey.empty();
}

std::string AzureTextureGenerator::escapeJson(const std::string& value) {
    std::ostringstream escaped;
    for (char c : value) {
        switch (c) {
            case '\\': escaped << "\\\\"; break;
            case '"': escaped << "\\\""; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default: escaped << c; break;
        }
    }
    return escaped.str();
}

bool AzureTextureGenerator::generateTexture(const std::string& prompt,
                                            const std::string& outputPath,
                                            const Config& config,
                                            std::string& outError) const {
    if (!isConfigured(config)) {
        outError = "Azure Foundry configuration is incomplete. Please set endpoint, deployment, and API key.";
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        outError = "Failed to initialize HTTP client.";
        return false;
    }

    const std::string url = buildRequestUrl(config);
    std::string response;

    std::ostringstream body;
    body << "{\"prompt\":\"" << escapeJson(prompt) << "\",";
    body << "\"n\":1,";
    body << "\"size\":\"" << config.imageSize << "x" << config.imageSize << "\"}";
    const std::string bodyStr = body.str();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string apiKeyHeader = "api-key: " + config.apiKey;
    headers = curl_slist_append(headers, apiKeyHeader.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    CURLcode res = curl_easy_perform(curl);
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        outError = std::string("Request failed: ") + curl_easy_strerror(res);
        return false;
    }

    if (responseCode < 200 || responseCode >= 300) {
        outError = "Azure Foundry returned HTTP " + std::to_string(responseCode) + ": " + response;
        return false;
    }

    const std::string key = "\"b64_json\":\"";
    const std::size_t keyPos = response.find(key);
    if (keyPos == std::string::npos) {
        outError = "Unexpected response from Azure Foundry (missing b64_json).";
        return false;
    }

    const std::size_t start = keyPos + key.size();
    const std::size_t end = response.find('"', start);
    if (end == std::string::npos || end <= start) {
        outError = "Unexpected response from Azure Foundry (malformed image payload).";
        return false;
    }

    std::string encoded = response.substr(start, end - start);
    auto decoded = base64::decode(encoded);
    if (decoded.empty()) {
        outError = "Failed to decode generated image.";
        return false;
    }

    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile) {
        outError = "Unable to open output path: " + outputPath;
        return false;
    }

    outFile.write(reinterpret_cast<const char*>(decoded.data()), static_cast<std::streamsize>(decoded.size()));
    if (!outFile) {
        outError = "Failed to write generated image to disk.";
        return false;
    }

    return true;
}

