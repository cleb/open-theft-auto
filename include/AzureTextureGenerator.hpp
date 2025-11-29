#pragma once

#include <string>

class AzureTextureGenerator {
public:
    struct Config {
        std::string endpoint;
        std::string deployment;
        std::string apiKey;
        std::string apiVersion = "2024-02-01";
        int imageSize = 512;
    };

    bool generateTexture(const std::string& prompt,
                         const std::string& outputPath,
                         const Config& config,
                         std::string& outError) const;

    bool isConfigured(const Config& config) const;

private:
    static std::string escapeJson(const std::string& value);
};

