#pragma once

#include <string>

namespace ps5discord {

struct Config {
    std::string token;
    std::string api_base = "https://discord.com/api/v10";
    std::string gateway_url = "wss://gateway.discord.gg/?v=10&encoding=json";
    std::string token_file;
};

std::string default_token_file();
bool load_config(Config& out, std::string* error = nullptr);
bool load_token_file(const std::string& path, std::string& token, std::string* error = nullptr);

} // namespace ps5discord
