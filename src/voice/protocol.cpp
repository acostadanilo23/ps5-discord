#include "ps5discord/voice/protocol.hpp"

#include "ps5discord/json.hpp"

#include <algorithm>

namespace ps5discord::voice {
namespace {
void put16(std::uint8_t* p, std::uint16_t n) {
    p[0] = static_cast<std::uint8_t>(n >> 8); p[1] = static_cast<std::uint8_t>(n);
}
void put32(std::uint8_t* p, std::uint32_t n) {
    p[0] = static_cast<std::uint8_t>(n >> 24); p[1] = static_cast<std::uint8_t>(n >> 16);
    p[2] = static_cast<std::uint8_t>(n >> 8); p[3] = static_cast<std::uint8_t>(n);
}
std::uint16_t get16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}
std::uint32_t get32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | p[3];
}
}

bool parse_gateway_event(std::string_view input, GatewayEvent& out) {
    json::Document doc;
    if (!doc.parse(std::string(input))) return false;
    int root = doc.root();
    int op = doc.object_get(root, "op");
    if (doc.type(op) != json::Type::number) return false;
    out = {};
    out.opcode = static_cast<int>(doc.integer(op));
    out.payload = std::string(doc.raw(doc.object_get(root, "d")));
    return !out.payload.empty();
}

bool parse_gateway_binary(std::span<const std::uint8_t> bytes, GatewayEvent& out) {
    if (bytes.size() < 3) return false;
    out = {};
    out.binary = true;
    out.sequence = get16(bytes.data());
    out.opcode = bytes[2];
    out.payload.assign(reinterpret_cast<const char*>(bytes.data() + 3), bytes.size() - 3);
    return true;
}

std::string identify(std::string_view guild, std::string_view user,
                     std::string_view session, std::string_view token, int dave_version) {
    return "{\"op\":0,\"d\":{\"server_id\":\"" + json::escape(guild) +
           "\",\"user_id\":\"" + json::escape(user) + "\",\"session_id\":\"" +
           json::escape(session) + "\",\"token\":\"" + json::escape(token) +
           "\",\"max_dave_protocol_version\":" + std::to_string(dave_version) + "}}";
}

std::string heartbeat(std::uint64_t nonce, std::uint16_t last_sequence) {
    return "{\"op\":3,\"d\":{\"t\":" + std::to_string(nonce) +
           ",\"seq_ack\":" + std::to_string(last_sequence) + "}}";
}

std::string select_protocol(std::string_view address, std::uint16_t port, std::string_view mode) {
    return "{\"op\":1,\"d\":{\"protocol\":\"udp\",\"data\":{\"address\":\"" +
           json::escape(address) + "\",\"port\":" + std::to_string(port) +
           ",\"mode\":\"" + json::escape(mode) + "\"}}}";
}

std::string speaking(bool active, std::uint32_t ssrc) {
    return "{\"op\":5,\"d\":{\"speaking\":" + std::to_string(active ? 1 : 0) +
           ",\"delay\":0,\"ssrc\":" + std::to_string(ssrc) + "}}";
}

std::vector<std::uint8_t> make_rtp(RtpHeader header, std::span<const std::uint8_t> payload) {
    std::vector<std::uint8_t> packet(12 + payload.size());
    packet[0] = 0x80; packet[1] = 0x78;
    put16(packet.data() + 2, header.sequence);
    put32(packet.data() + 4, header.timestamp);
    put32(packet.data() + 8, header.ssrc);
    std::copy(payload.begin(), payload.end(), packet.begin() + 12);
    return packet;
}

bool parse_rtp(std::span<const std::uint8_t> packet, RtpHeader& header,
               std::span<const std::uint8_t>& payload) {
    if (packet.size() < 12 || (packet[0] >> 6) != 2 || (packet[0] & 0x10) != 0 ||
        (packet[0] & 0x0f) != 0 || (packet[1] & 0x7f) != 0x78) return false;
    header.sequence = get16(packet.data() + 2);
    header.timestamp = get32(packet.data() + 4);
    header.ssrc = get32(packet.data() + 8);
    payload = packet.subspan(12);
    return true;
}

std::array<std::uint8_t, 74> discovery_request(std::uint32_t ssrc) {
    std::array<std::uint8_t, 74> packet{};
    put16(packet.data(), 1);
    put16(packet.data() + 2, 70);
    put32(packet.data() + 4, ssrc);
    return packet;
}

bool parse_discovery_response(std::span<const std::uint8_t> packet,
                              std::string& address, std::uint16_t& port) {
    if (packet.size() != 74 || get16(packet.data()) != 2 || get16(packet.data() + 2) != 70) return false;
    auto start = packet.begin() + 8;
    auto end = std::find(start, packet.begin() + 72, 0);
    if (end == packet.begin() + 72 || end == start) return false;
    address.assign(start, end);
    port = get16(packet.data() + 72);
    return port != 0;
}

} // namespace ps5discord::voice
