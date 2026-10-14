#include "ps5discord/audio/opus.hpp"

#include <opus.h>

namespace ps5discord::audio {

OpusVoiceEncoder::OpusVoiceEncoder() {
    int error = OPUS_OK;
    encoder_ = opus_encoder_create(kCaptureRate, 1, OPUS_APPLICATION_VOIP, &error);
    if (error != OPUS_OK) encoder_ = nullptr;
    if (encoder_) {
        opus_encoder_ctl(encoder_, OPUS_SET_BITRATE(32000));
        opus_encoder_ctl(encoder_, OPUS_SET_INBAND_FEC(1));
    }
}

OpusVoiceEncoder::~OpusVoiceEncoder() {
    if (encoder_) opus_encoder_destroy(encoder_);
}

bool OpusVoiceEncoder::encode(std::span<const std::int16_t> pcm,
                              std::vector<std::uint8_t>& packet) {
    if (!encoder_ || pcm.size() != kCaptureFrame) return false;
    packet.resize(1275);
    int bytes = opus_encode(encoder_, pcm.data(), kCaptureFrame, packet.data(),
                            static_cast<opus_int32>(packet.size()));
    if (bytes < 0) { packet.clear(); return false; }
    packet.resize(static_cast<std::size_t>(bytes));
    return true;
}

OpusVoiceDecoder::OpusVoiceDecoder() {
    int error = OPUS_OK;
    decoder_ = opus_decoder_create(kOutputRate, 1, &error);
    if (error != OPUS_OK) decoder_ = nullptr;
}

OpusVoiceDecoder::~OpusVoiceDecoder() {
    if (decoder_) opus_decoder_destroy(decoder_);
}

bool OpusVoiceDecoder::decode(std::span<const std::uint8_t> packet,
                              std::vector<std::int16_t>& pcm) {
    if (!decoder_ || packet.size() > 1275) return false;
    pcm.resize(kOutputFrame);
    int frames = opus_decode(decoder_, packet.empty() ? nullptr : packet.data(),
                             static_cast<opus_int32>(packet.size()), pcm.data(),
                             kOutputFrame, 0);
    if (frames < 0) { pcm.clear(); return false; }
    pcm.resize(static_cast<std::size_t>(frames));
    return true;
}

} // namespace ps5discord::audio
