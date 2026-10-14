#pragma once

#include "ps5discord/config.hpp"
#include "ps5discord/discord/models.hpp"

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ps5discord::discord {

enum class ServiceState { stopped, starting, ready, error };

struct Snapshot {
    ServiceState state = ServiceState::stopped;
    User me;
    std::vector<Guild> guilds;
    std::vector<Channel> channels;
    std::vector<Message> messages;
    std::string selected_guild_id;
    std::string selected_channel_id;
    std::string status;
    std::string error;
    std::uint64_t generation = 0;
};

class Service {
public:
    explicit Service(Config config);
    ~Service();

    Service(const Service&) = delete;
    Service& operator=(const Service&) = delete;

    void start();
    void stop();
    bool running() const { return running_.load(); }

    void refresh_guilds();
    void select_guild(std::string guild_id);
    void select_channel(std::string channel_id);
    void send_message(std::string content);
    Snapshot snapshot() const;

private:
    enum class CommandType { refresh_guilds, select_guild, select_channel, send_message };
    struct Command { CommandType type; std::string value; };

    void enqueue(CommandType type, std::string value = {});
    bool dequeue(Command& command);
    void worker_main();
    void set_error(std::string message);
    void bump();

    Config config_;
    mutable std::mutex mutex_;
    Snapshot snapshot_;
    std::deque<Command> commands_;
    std::atomic_bool stop_{false};
    std::atomic_bool running_{false};
    std::thread worker_;
};

} // namespace ps5discord::discord
