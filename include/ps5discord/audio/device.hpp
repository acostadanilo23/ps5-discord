#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace ps5discord::audio {

enum class OutputRoute { system, dualsense_speaker };

// Native PS5 AudioIn/AudioOut block I/O. The system route follows the console's
// current HDMI/TV or headset selection. The pad speaker is explicitly optional.
class Device {
public:
    Device() = default;
    ~Device();
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    bool open_input(std::string* error = nullptr);
    bool open_output(OutputRoute route, std::string* error = nullptr);
    void close_input();
    void close_output();
    bool capture(std::array<std::int16_t, 256>& mono);
    bool play(std::span<const std::int16_t> pcm);
    OutputRoute output_route() const { return route_; }

private:
    int input_ = -1;
    int output_ = -1;
    OutputRoute route_ = OutputRoute::system;
};

} // namespace ps5discord::audio
