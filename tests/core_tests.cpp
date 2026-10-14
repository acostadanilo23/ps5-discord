#include "ps5discord/json.hpp"
#include "ps5discord/discord/models.hpp"

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
    check(doc.parse(R"({"name":"A\\nB","n":42,"ok":true,"items":[1,2,3],"u":"caf\u00e9"})"), "parse object");
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

    if (failures == 0) std::cout << "all tests passed\n";
    return failures == 0 ? 0 : 1;
}
