#pragma once

#include "ps5discord/config.hpp"
#include "ps5discord/discord/models.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ps5discord::discord {

struct HttpResponse {
    long status = 0;
    std::string body;
    std::string retry_after;
    std::string rate_remaining;
    std::string rate_reset_after;
    std::string error;

    bool ok() const { return status >= 200 && status < 300 && error.empty(); }
};

class RestClient {
public:
    explicit RestClient(const Config& config);
    ~RestClient();

    RestClient(const RestClient&) = delete;
    RestClient& operator=(const RestClient&) = delete;

    HttpResponse get(std::string_view route);
    HttpResponse post_json(std::string_view route, std::string_view json_body);

    bool current_user(User& out, std::string* error = nullptr);
    bool guilds(std::vector<Guild>& out, std::string* error = nullptr);
    bool channels(std::string_view guild_id, std::vector<Channel>& out, std::string* error = nullptr);
    bool messages(std::string_view channel_id, std::vector<Message>& out, int limit = 50,
                  std::string_view before = {}, std::string* error = nullptr);
    bool send_message(std::string_view channel_id, std::string_view content, Message& out,
                      std::string* error = nullptr);

private:
    HttpResponse request(const char* method, std::string_view route, std::string_view body,
                         bool allow_retry);
    bool parse_error(const HttpResponse& response, std::string* error) const;

    Config config_;
    void* curl_ = nullptr;
};

} // namespace ps5discord::discord
