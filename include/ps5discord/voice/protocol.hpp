#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ps5discord::voice {

// Voice signaling runs on its own websocket. Media travels on a separate UDP socket.
struct GatewayEvent {
    int opcode = -1;
    std::string payload;
    std::uint16_t sequence = 0;
    bool binary = false;
};

bool parse_gateway_event(std::string_view json, GatewayEvent& out);
bool parse_gateway_binary(std::span<const std::uint8_t> bytes, GatewayEvent& out);
std::string identify(std::string_view guild, std::string_view user,
                     std::string_view session, std::string_view token, int dave_version);
std::string heartbeat(std::uint64_t nonce, std::uint16_t last_sequence);
std::string select_protocol(std::string_view address, std::uint16_t port, std::string_view mode);
std::string speaking(bool active, std::uint32_t ssrc);

struct RtpHeader {
    std::uint16_t sequence = 0;
    std::uint32_t timestamp = 0;
    std::uint32_t ssrc = 0;
};

// The returned packet contains an unencrypted RTP header and an opaque media payload.
// Callers must DAVE-encrypt and transport-encrypt the Opus payload before sending.
std::vector<std::uint8_t> make_rtp(RtpHeader header, std::span<const std::uint8_t> payload);
bool parse_rtp(std::span<const std::uint8_t> packet, RtpHeader& header,
               std::span<const std::uint8_t>& payload);

// Discord's 74-byte UDP IP discovery packet, not an RTP media packet.
std::array<std::uint8_t, 74> discovery_request(std::uint32_t ssrc);
bool parse_discovery_response(std::span<const std::uint8_t> packet,
                              std::string& address, std::uint16_t& port);

} // namespace ps5discord::voice
