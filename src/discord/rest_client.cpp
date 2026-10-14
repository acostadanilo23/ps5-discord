#include "ps5discord/discord/rest_client.hpp"

#include "ps5discord/json.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <curl/curl.h>
#include <string>
#include <thread>

namespace ps5discord::discord {

namespace {

std::size_t write_body(char* data, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    std::size_t n = size * nmemb;
    out->append(data, n);
    return n;
}

std::string lower_ascii(std::string_view input) {
    std::string out(input);
    for (char& c : out) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return out;
}

void trim_header_value(std::string& value) {
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ' || value.back() == '\t')) value.pop_back();
    std::size_t first = 0;
    while (first < value.size() && (value[first] == ' ' || value[first] == '\t')) ++first;
    if (first) value.erase(0, first);
}

std::size_t write_header(char* data, std::size_t size, std::size_t nmemb, void* userdata) {
    auto* response = static_cast<HttpResponse*>(userdata);
    std::size_t n = size * nmemb;
    std::string_view line(data, n);
    auto colon = line.find(':');
    if (colon == std::string_view::npos) return n;
    std::string name = lower_ascii(line.substr(0, colon));
    std::string value(line.substr(colon + 1));
    trim_header_value(value);
    if (name == "retry-after") response->retry_after = value;
    else if (name == "x-ratelimit-remaining") response->rate_remaining = value;
    else if (name == "x-ratelimit-reset-after") response->rate_reset_after = value;
    return n;
}

bool parse_document(const HttpResponse& response, json::Document& doc, std::string* error) {
    if (!doc.parse(response.body)) {
        if (error) *error = "invalid Discord JSON: " + doc.error();
        return false;
    }
    return true;
}

std::string response_error_text(const HttpResponse& response) {
    if (!response.error.empty()) return response.error;
    json::Document doc;
    if (doc.parse(response.body)) {
        std::string message = doc.string(doc.object_get(doc.root(), "message"));
        if (!message.empty()) return "Discord HTTP " + std::to_string(response.status) + ": " + message;
    }
    return "Discord HTTP " + std::to_string(response.status);
}

} // namespace

RestClient::RestClient(const Config& config) : config_(config) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_ = curl_easy_init();
}

RestClient::~RestClient() {
    if (curl_) curl_easy_cleanup(static_cast<CURL*>(curl_));
    curl_global_cleanup();
}

HttpResponse RestClient::get(std::string_view route) {
    return request("GET", route, {}, true);
}

HttpResponse RestClient::post_json(std::string_view route, std::string_view json_body) {
    return request("POST", route, json_body, true);
}

HttpResponse RestClient::request(const char* method, std::string_view route, std::string_view body,
                                 bool allow_retry) {
    HttpResponse response;
    CURL* curl = static_cast<CURL*>(curl_);
    if (!curl) {
        response.error = "curl_easy_init failed";
        return response;
    }

    std::string url = config_.api_base;
    url.append(route.data(), route.size());
    std::string authorization = "Authorization: " + config_.token;
    curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, authorization.c_str());
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "User-Agent: ps5-discord/0.1 (+personal-homebrew)");

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 12L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    if (std::strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    } else {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }

    CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK) response.error = curl_easy_strerror(code);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status);
    curl_slist_free_all(headers);

    if (response.status == 429 && allow_retry) {
        double seconds = 0.0;
        json::Document doc;
        if (doc.parse(response.body)) {
            int retry = doc.object_get(doc.root(), "retry_after");
            std::string_view raw = doc.raw(retry);
            if (!raw.empty()) seconds = std::strtod(std::string(raw).c_str(), nullptr);
        }
        if (seconds <= 0.0 && !response.retry_after.empty()) seconds = std::strtod(response.retry_after.c_str(), nullptr);
        if (seconds <= 0.0) seconds = 1.0;
        if (seconds > 30.0) seconds = 30.0;
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(seconds * 1000.0) + 50));
        return request(method, route, body, false);
    }

    return response;
}

bool RestClient::parse_error(const HttpResponse& response, std::string* error) const {
    if (response.ok()) return false;
    if (error) *error = response_error_text(response);
    return true;
}

bool RestClient::current_user(User& out, std::string* error) {
    HttpResponse r = get("/users/@me");
    if (parse_error(r, error)) return false;
    json::Document doc;
    if (!parse_document(r, doc, error)) return false;
    out = parse_user(doc, doc.root());
    return !out.id.empty();
}

bool RestClient::guilds(std::vector<Guild>& out, std::string* error) {
    HttpResponse r = get("/users/@me/guilds?with_counts=true");
    if (parse_error(r, error)) return false;
    json::Document doc;
    if (!parse_document(r, doc, error)) return false;
    if (doc.type(doc.root()) != json::Type::array) {
        if (error) *error = "guild response is not an array";
        return false;
    }
    out.clear();
    out.reserve(doc.array_size(doc.root()));
    for (std::size_t i = 0; i < doc.array_size(doc.root()); ++i) out.push_back(parse_guild(doc, doc.array_at(doc.root(), i)));
    return true;
}

bool RestClient::channels(std::string_view guild_id, std::vector<Channel>& out, std::string* error) {
    std::string route = "/guilds/" + std::string(guild_id) + "/channels";
    HttpResponse r = get(route);
    if (parse_error(r, error)) return false;
    json::Document doc;
    if (!parse_document(r, doc, error)) return false;
    if (doc.type(doc.root()) != json::Type::array) {
        if (error) *error = "channel response is not an array";
        return false;
    }
    out.clear();
    for (std::size_t i = 0; i < doc.array_size(doc.root()); ++i) {
        Channel channel = parse_channel(doc, doc.array_at(doc.root(), i));
        if (channel.is_textual()) out.push_back(std::move(channel));
    }
    std::sort(out.begin(), out.end(), [](const Channel& a, const Channel& b) { return a.position < b.position; });
    return true;
}

bool RestClient::messages(std::string_view channel_id, std::vector<Message>& out, int limit,
                          std::string_view before, std::string* error) {
    if (limit < 1) limit = 1;
    if (limit > 100) limit = 100;
    std::string route = "/channels/" + std::string(channel_id) + "/messages?limit=" + std::to_string(limit);
    if (!before.empty()) route += "&before=" + std::string(before);
    HttpResponse r = get(route);
    if (parse_error(r, error)) return false;
    json::Document doc;
    if (!parse_document(r, doc, error)) return false;
    if (doc.type(doc.root()) != json::Type::array) {
        if (error) *error = "message response is not an array";
        return false;
    }
    out.clear();
    out.reserve(doc.array_size(doc.root()));
    for (std::size_t i = 0; i < doc.array_size(doc.root()); ++i) out.push_back(parse_message(doc, doc.array_at(doc.root(), i)));
    std::reverse(out.begin(), out.end());
    return true;
}

bool RestClient::send_message(std::string_view channel_id, std::string_view content, Message& out,
                              std::string* error) {
    if (content.empty()) {
        if (error) *error = "message is empty";
        return false;
    }
    std::string route = "/channels/" + std::string(channel_id) + "/messages";
    std::string body = "{\"content\":\"" + json::escape(content) + "\",\"tts\":false}";
    HttpResponse r = post_json(route, body);
    if (parse_error(r, error)) return false;
    json::Document doc;
    if (!parse_document(r, doc, error)) return false;
    out = parse_message(doc, doc.root());
    return !out.id.empty();
}

} // namespace ps5discord::discord
