#include "ps5discord/discord/service.hpp"

#include "ps5discord/discord/rest_client.hpp"

#include <chrono>
#include <thread>
#include <utility>

namespace ps5discord::discord {

Service::Service(Config config) : config_(std::move(config)) {}

Service::~Service() { stop(); }

void Service::start() {
    if (running_.exchange(true)) return;
    stop_.store(false);
    {
        std::lock_guard lock(mutex_);
        snapshot_ = {};
        snapshot_.state = ServiceState::starting;
        snapshot_.status = "Connecting to Discord";
        ++snapshot_.generation;
    }
    worker_ = std::thread(&Service::worker_main, this);
}

void Service::stop() {
    stop_.store(true);
    if (worker_.joinable()) worker_.join();
    running_.store(false);
    std::lock_guard lock(mutex_);
    snapshot_.state = ServiceState::stopped;
    snapshot_.status = "Stopped";
    ++snapshot_.generation;
}

void Service::refresh_guilds() { enqueue(CommandType::refresh_guilds); }
void Service::select_guild(std::string guild_id) { enqueue(CommandType::select_guild, std::move(guild_id)); }
void Service::select_channel(std::string channel_id) { enqueue(CommandType::select_channel, std::move(channel_id)); }
void Service::send_message(std::string content) { enqueue(CommandType::send_message, std::move(content)); }

Snapshot Service::snapshot() const {
    std::lock_guard lock(mutex_);
    return snapshot_;
}

void Service::enqueue(CommandType type, std::string value) {
    std::lock_guard lock(mutex_);
    commands_.push_back(Command{type, std::move(value)});
}

bool Service::dequeue(Command& command) {
    std::lock_guard lock(mutex_);
    if (commands_.empty()) return false;
    command = std::move(commands_.front());
    commands_.pop_front();
    return true;
}

void Service::set_error(std::string message) {
    std::lock_guard lock(mutex_);
    snapshot_.state = ServiceState::error;
    snapshot_.error = std::move(message);
    snapshot_.status = "Discord request failed";
    ++snapshot_.generation;
}

void Service::bump() {
    std::lock_guard lock(mutex_);
    ++snapshot_.generation;
}

void Service::worker_main() {
    RestClient rest(config_);
    User me;
    std::string error;
    if (!rest.current_user(me, &error)) {
        set_error(error);
        running_.store(false);
        return;
    }

    std::vector<Guild> guilds;
    if (!rest.guilds(guilds, &error)) {
        set_error(error);
        running_.store(false);
        return;
    }

    {
        std::lock_guard lock(mutex_);
        snapshot_.me = std::move(me);
        snapshot_.guilds = std::move(guilds);
        snapshot_.state = ServiceState::ready;
        snapshot_.status = "Connected";
        snapshot_.error.clear();
        ++snapshot_.generation;
    }

    auto next_poll = std::chrono::steady_clock::now();
    while (!stop_.load()) {
        Command command;
        if (dequeue(command)) {
            error.clear();
            if (command.type == CommandType::refresh_guilds) {
                std::vector<Guild> fresh;
                if (rest.guilds(fresh, &error)) {
                    std::lock_guard lock(mutex_);
                    snapshot_.guilds = std::move(fresh);
                    snapshot_.status = "Servers refreshed";
                    snapshot_.error.clear();
                    ++snapshot_.generation;
                } else set_error(error);
            } else if (command.type == CommandType::select_guild) {
                std::vector<Channel> channels;
                if (rest.channels(command.value, channels, &error)) {
                    std::lock_guard lock(mutex_);
                    snapshot_.selected_guild_id = command.value;
                    snapshot_.selected_channel_id.clear();
                    snapshot_.channels = std::move(channels);
                    snapshot_.messages.clear();
                    snapshot_.status = "Select a channel";
                    snapshot_.error.clear();
                    snapshot_.state = ServiceState::ready;
                    ++snapshot_.generation;
                } else set_error(error);
            } else if (command.type == CommandType::select_channel) {
                std::vector<Message> messages;
                if (rest.messages(command.value, messages, 50, {}, &error)) {
                    std::lock_guard lock(mutex_);
                    snapshot_.selected_channel_id = command.value;
                    snapshot_.messages = std::move(messages);
                    snapshot_.status = "Chat loaded";
                    snapshot_.error.clear();
                    snapshot_.state = ServiceState::ready;
                    ++snapshot_.generation;
                    next_poll = std::chrono::steady_clock::now() + std::chrono::milliseconds(1200);
                } else set_error(error);
            } else if (command.type == CommandType::send_message) {
                std::string channel;
                {
                    std::lock_guard lock(mutex_);
                    channel = snapshot_.selected_channel_id;
                }
                if (!channel.empty()) {
                    Message sent;
                    if (rest.send_message(channel, command.value, sent, &error)) {
                        std::lock_guard lock(mutex_);
                        snapshot_.messages.push_back(std::move(sent));
                        snapshot_.status = "Message sent";
                        snapshot_.error.clear();
                        snapshot_.state = ServiceState::ready;
                        ++snapshot_.generation;
                        next_poll = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
                    } else set_error(error);
                }
            }
            continue;
        }

        if (std::chrono::steady_clock::now() >= next_poll) {
            std::string channel;
            {
                std::lock_guard lock(mutex_);
                channel = snapshot_.selected_channel_id;
            }
            if (!channel.empty()) {
                std::vector<Message> messages;
                error.clear();
                if (rest.messages(channel, messages, 50, {}, &error)) {
                    std::lock_guard lock(mutex_);
                    if (channel == snapshot_.selected_channel_id) {
                        snapshot_.messages = std::move(messages);
                        snapshot_.status = "Live polling";
                        snapshot_.error.clear();
                        snapshot_.state = ServiceState::ready;
                        ++snapshot_.generation;
                    }
                } else {
                    std::lock_guard lock(mutex_);
                    snapshot_.error = error;
                    snapshot_.status = "Polling retry";
                    ++snapshot_.generation;
                }
            }
            next_poll = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    running_.store(false);
}

} // namespace ps5discord::discord
