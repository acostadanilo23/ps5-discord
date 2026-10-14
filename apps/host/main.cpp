#include "ps5discord/config.hpp"
#include "ps5discord/discord/service.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using ps5discord::discord::Service;
using ps5discord::discord::ServiceState;

static void print_snapshot(const ps5discord::discord::Snapshot& s) {
    std::cout << "\n";
    if (!s.me.id.empty()) {
        std::cout << "Logged in as " << (s.me.global_name.empty() ? s.me.username : s.me.global_name)
                  << " (@" << s.me.username << ")\n";
    }
    std::cout << "Status: " << s.status;
    if (!s.error.empty()) std::cout << " | " << s.error;
    std::cout << "\n";

    if (!s.selected_channel_id.empty()) {
        std::cout << "Messages (latest " << s.messages.size() << "):\n";
        for (const auto& m : s.messages) {
            std::string author = m.author.global_name.empty() ? m.author.username : m.author.global_name;
            std::cout << "  " << author << ": " << m.content;
            if (!m.attachments.empty()) std::cout << " [" << m.attachments.size() << " attachment(s)]";
            std::cout << "\n";
        }
        return;
    }

    if (!s.selected_guild_id.empty()) {
        std::cout << "Channels:\n";
        for (std::size_t i = 0; i < s.channels.size(); ++i)
            std::cout << "  [" << i << "] #" << s.channels[i].name << " (" << s.channels[i].id << ")\n";
        return;
    }

    std::cout << "Servers:\n";
    for (std::size_t i = 0; i < s.guilds.size(); ++i)
        std::cout << "  [" << i << "] " << s.guilds[i].name << " (" << s.guilds[i].id << ")\n";
}

int main() {
    ps5discord::Config config;
    std::string error;
    if (!ps5discord::load_config(config, &error)) {
        std::cerr << error << "\n\n";
        std::cerr << "Set PS5_DISCORD_TOKEN for a one-off host test or create: "
                  << ps5discord::default_token_file() << "\n";
        return 2;
    }

    Service service(std::move(config));
    service.start();

    for (int i = 0; i < 200; ++i) {
        auto s = service.snapshot();
        if (s.state == ServiceState::ready || s.state == ServiceState::error) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::uint64_t last_generation = ~std::uint64_t{0};
    std::cout << "ps5-discord host console\n"
              << "Commands: p=print, g <index>=server, c <index>=channel, s <text>=send, r=refresh, q=quit\n";

    std::string line;
    while (true) {
        auto snapshot = service.snapshot();
        if (snapshot.generation != last_generation) {
            print_snapshot(snapshot);
            last_generation = snapshot.generation;
        }
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line == "q") break;
        if (line == "p") continue;
        if (line == "r") { service.refresh_guilds(); continue; }
        if (line.rfind("g ", 0) == 0) {
            std::size_t index = static_cast<std::size_t>(std::stoul(line.substr(2)));
            snapshot = service.snapshot();
            if (index < snapshot.guilds.size()) service.select_guild(snapshot.guilds[index].id);
            else std::cout << "invalid server index\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            continue;
        }
        if (line.rfind("c ", 0) == 0) {
            std::size_t index = static_cast<std::size_t>(std::stoul(line.substr(2)));
            snapshot = service.snapshot();
            if (index < snapshot.channels.size()) service.select_channel(snapshot.channels[index].id);
            else std::cout << "invalid channel index\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            continue;
        }
        if (line.rfind("s ", 0) == 0) {
            service.send_message(line.substr(2));
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            continue;
        }
        std::cout << "unknown command\n";
    }

    service.stop();
    return 0;
}
