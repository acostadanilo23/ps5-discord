#include "ps5discord/json.hpp"
#include "ps5discord/discord/models.hpp"
#include "ps5discord/voice/protocol.hpp"

#include <iostream>
#include <string>

static int failures = 0;

static void check(bool value, const char* name) {
    if (!value) {
        std::cerr << "FAIL: " << name << "\n";
        ++failures;
    }
}

int main() {
    using namespace ps5discord;

    json::Document doc;
    check(doc.parse(R"({"name":"A\nB","n":42,"ok":true,"items":[1,2,3],"u":"caf\u00e9"})"), "parse object");
    check(doc.string(doc.object_get(doc.root(), "name")) == "A\nB", "decode escape");
    check(doc.integer(doc.object_get(doc.root(), "n")) == 42, "integer");
    check(doc.boolean(doc.object_get(doc.root(), "ok")), "boolean");
    check(doc.array_size(doc.object_get(doc.root(), "items")) == 3, "array size");
    check(doc.string(doc.object_get(doc.root(), "u")) == "café", "unicode escape");
    check(json::escape("a\n\"b") == "a\\n\\\"b", "json escape");

    json::Document message_doc;
    check(message_doc.parse(R"({"id":"100","channel_id":"200","guild_id":"300","content":"hello","timestamp":"now","author":{"id":"1","username":"dan","global_name":"Dan"},"attachments":[{"id":"9","filename":"x.png","url":"https://x","size":12}]})"), "message parse json");
    auto message = discord::parse_message(message_doc, message_doc.root());
    check(message.id == "100", "message id");
    check(message.author.global_name == "Dan", "message author");
    check(message.attachments.size() == 1 && message.attachments[0].filename == "x.png", "attachment");

    json::Document channel_doc;
    check(channel_doc.parse(R"({"id":"10","type":2,"name":"General"})"), "voice channel json");
    check(discord::parse_channel(channel_doc, channel_doc.root()).is_voice(), "voice channel type");

    voice::GatewayEvent event;
    check(voice::parse_gateway_event(R"({"op":2,"d":{"ssrc":123}})", event) &&
          event.opcode == 2 && event.payload == R"({"ssrc":123})", "voice gateway event");
    const std::uint8_t binary[] = {0, 4, 30, 1, 2};
    check(voice::parse_gateway_binary(binary, event) && event.binary && event.sequence == 4 &&
          event.opcode == 30 && event.payload.size() == 2, "voice binary event");
    check(voice::identify("g", "u", "s", "t", 1).find("max_dave_protocol_version\":1") != std::string::npos,
          "voice identifies DAVE capability");
    const std::uint8_t opaque[] = {1, 2, 3};
    auto packet = voice::make_rtp({7, 960, 42}, opaque);
    voice::RtpHeader header;
    std::span<const std::uint8_t> payload;
    check(voice::parse_rtp(packet, header, payload) && header.sequence == 7 &&
          header.timestamp == 960 && header.ssrc == 42 && payload.size() == 3,
          "RTP header round trip");
    auto discovery = voice::discovery_request(42);
    check(discovery.size() == 74 && discovery[0] == 0 && discovery[1] == 1 &&
          discovery[7] == 42, "UDP discovery request");

    if (failures == 0) std::cout << "all tests passed\n";
    return failures == 0 ? 0 : 1;
}
