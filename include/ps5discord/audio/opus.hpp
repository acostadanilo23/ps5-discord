#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

struct OpusEncoder;
struct OpusDecoder;

namespace ps5discord::audio {

// 20 ms voice frames: 320 samples from the 16 kHz microphone, 960 samples
// after decoding at the 48 kHz output rate. RTP timestamps always use 48 kHz.
constexpr int kCaptureRate = 16000;
constexpr int kOutputRate = 48000;
constexpr int kCaptureFrame = 320;
constexpr int kOutputFrame = 960;

class OpusVoiceEncoder {
public:
    OpusVoiceEncoder();
    ~OpusVoiceEncoder();
    OpusVoiceEncoder(const OpusVoiceEncoder&) = delete;
    OpusVoiceEncoder& operator=(const OpusVoiceEncoder&) = delete;
    bool ready() const { return encoder_ != nullptr; }
    bool encode(std::span<const std::int16_t> pcm, std::vector<std::uint8_t>& packet);
private:
    ::OpusEncoder* encoder_ = nullptr;
};

class OpusVoiceDecoder {
public:
    OpusVoiceDecoder();
    ~OpusVoiceDecoder();
    OpusVoiceDecoder(const OpusVoiceDecoder&) = delete;
    OpusVoiceDecoder& operator=(const OpusVoiceDecoder&) = delete;
    bool ready() const { return decoder_ != nullptr; }
    // Produces mono PCM. Pass an empty packet for Opus packet-loss concealment.
    bool decode(std::span<const std::uint8_t> packet, std::vector<std::int16_t>& pcm);
private:
    ::OpusDecoder* decoder_ = nullptr;
};

} // namespace ps5discord::audio
