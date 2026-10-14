#include "ps5discord/config.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace ps5discord {

static void trim(std::string& value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) value.pop_back();
    std::size_t start = 0;
    while (start < value.size() && (value[start] == ' ' || value[start] == '\t' || value[start] == '\n' || value[start] == '\r')) ++start;
    if (start) value.erase(0, start);
}

std::string default_token_file() {
#if defined(__ORBIS__) || defined(__PROSPERO__)
    return "/download0/ps5-discord/token.txt";
#else
    if (const char* explicit_path = std::getenv("PS5_DISCORD_TOKEN_FILE")) return explicit_path;
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) return std::string(xdg) + "/ps5-discord/token.txt";
    if (const char* home = std::getenv("HOME")) return std::string(home) + "/.config/ps5-discord/token.txt";
    return "token.txt";
#endif
}

bool load_token_file(const std::string& path, std::string& token, std::string* error) {
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) {
        if (error) *error = "could not open token file: " + path;
        return false;
    }

    std::string value;
    char buffer[1024];
    for (;;) {
        std::size_t n = std::fread(buffer, 1, sizeof(buffer), file);
        if (n) value.append(buffer, n);
        if (n < sizeof(buffer)) break;
        if (value.size() > 16384) break;
    }
    std::fclose(file);

    trim(value);
    if (value.empty()) {
        if (error) *error = "token file is empty: " + path;
        return false;
    }
    token = std::move(value);
    return true;
}

bool load_config(Config& out, std::string* error) {
#if !defined(__ORBIS__) && !defined(__PROSPERO__)
    if (const char* api = std::getenv("PS5_DISCORD_API_BASE")) out.api_base = api;
    if (const char* gateway = std::getenv("PS5_DISCORD_GATEWAY_URL")) out.gateway_url = gateway;
    if (const char* env_token = std::getenv("PS5_DISCORD_TOKEN")) {
        out.token = env_token;
        trim(out.token);
        if (!out.token.empty()) return true;
    }
#endif
    out.token_file = default_token_file();
    return load_token_file(out.token_file, out.token, error);
}

} // namespace ps5discord
