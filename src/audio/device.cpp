#include "ps5discord/audio/device.hpp"

#include <array>
#include <cstdio>

#if defined(PS5_DISCORD_PS5)
extern "C" {
int sceUserServiceGetInitialUser(int* user);
int sceAudioInInit();
int sceAudioInOpen(int user, unsigned type, unsigned index, unsigned frames,
                   unsigned rate, unsigned format);
int sceAudioInInput(int handle, void* samples);
int sceAudioInClose(int handle);
int sceAudioOutInit();
int sceAudioOutOpen(int user, int type, int index, unsigned frames,
                    unsigned rate, unsigned format);
int sceAudioOutOutput(int handle, const void* samples);
int sceAudioOutClose(int handle);
}
#endif

namespace ps5discord::audio {
namespace {
void set_error(std::string* error, const char* operation, int code) {
    if (!error) return;
    char message[128];
    std::snprintf(message, sizeof(message), "%s failed (0x%08x)", operation,
                  static_cast<unsigned>(code));
    *error = message;
}
}

Device::~Device() { close_input(); close_output(); }

bool Device::open_input(std::string* error) {
    close_input();
#if defined(PS5_DISCORD_PS5)
    int user = -1;
    int rc = sceUserServiceGetInitialUser(&user);
    if (rc < 0) { set_error(error, "sceUserServiceGetInitialUser", rc); return false; }
    // Type 1, 16 kHz mono and a 256-frame grain are used by a published
    // PS5 DualSense microphone capture proof of concept.
    sceAudioInInit();
    input_ = sceAudioInOpen(user, 1, 0, 256, 16000, 0);
    if (input_ < 0) { set_error(error, "sceAudioInOpen", input_); return false; }
    return true;
#else
    set_error(error, "PS5 microphone unavailable on host", -1);
    return false;
#endif
}

bool Device::open_output(OutputRoute route, std::string* error) {
    close_output();
#if defined(PS5_DISCORD_PS5)
    constexpr int kAlreadyInitialized = static_cast<int>(0x8026000e);
    int rc = sceAudioOutInit();
    if (rc != 0 && rc != kAlreadyInitialized) {
        set_error(error, "sceAudioOutInit", rc); return false;
    }
    int user = 0xff;
    int port = 0; // MAIN: TV/HDMI or headset according to PS5 system routing.
    int format = 1; // signed 16-bit stereo
    if (route == OutputRoute::dualsense_speaker) {
        rc = sceUserServiceGetInitialUser(&user);
        if (rc < 0) { set_error(error, "sceUserServiceGetInitialUser", rc); return false; }
        port = 4; // PADSPK
        format = 0; // signed 16-bit mono
    }
    output_ = sceAudioOutOpen(user, port, 0, 256, 48000, format);
    if (output_ < 0) { set_error(error, "sceAudioOutOpen", output_); return false; }
    route_ = route;
    return true;
#else
    (void)route;
    set_error(error, "PS5 speaker unavailable on host", -1);
    return false;
#endif
}

void Device::close_input() {
#if defined(PS5_DISCORD_PS5)
    if (input_ >= 0) sceAudioInClose(input_);
#endif
    input_ = -1;
}

void Device::close_output() {
#if defined(PS5_DISCORD_PS5)
    if (output_ >= 0) sceAudioOutClose(output_);
#endif
    output_ = -1;
}

bool Device::capture(std::array<std::int16_t, 256>& mono) {
#if defined(PS5_DISCORD_PS5)
    return input_ >= 0 && sceAudioInInput(input_, mono.data()) >= 0;
#else
    (void)mono;
    return false;
#endif
}

bool Device::play(std::span<const std::int16_t> pcm) {
    const std::size_t channels = route_ == OutputRoute::system ? 2 : 1;
    if (pcm.size() != 256 * channels) return false;
#if defined(PS5_DISCORD_PS5)
    return output_ >= 0 && sceAudioOutOutput(output_, pcm.data()) >= 0;
#else
    return false;
#endif
}

} // namespace ps5discord::audio
