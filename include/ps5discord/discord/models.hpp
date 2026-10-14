#pragma once

#include "ps5discord/json.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ps5discord::discord {

struct User {
    std::string id;
    std::string username;
    std::string global_name;
    std::string avatar;
};

struct Guild {
    std::string id;
    std::string name;
    std::string icon;
    bool owner = false;
};

struct Channel {
    std::string id;
    std::string guild_id;
    std::string parent_id;
    std::string name;
    std::int64_t type = -1;
    std::int64_t position = 0;

    bool is_textual() const {
        return type == 0 || type == 5 || type == 10 || type == 11 || type == 12 || type == 15;
    }
    bool is_voice() const { return type == 2; }
};

struct Attachment {
    std::string id;
    std::string filename;
    std::string url;
    std::string proxy_url;
    std::string content_type;
    std::int64_t size = 0;
};

struct Message {
    std::string id;
    std::string channel_id;
    std::string guild_id;
    std::string content;
    std::string timestamp;
    std::string edited_timestamp;
    User author;
    std::vector<Attachment> attachments;
    std::string referenced_message_id;
};

inline User parse_user(const json::Document& doc, int node) {
    User out;
    out.id = doc.string(doc.object_get(node, "id"));
    out.username = doc.string(doc.object_get(node, "username"));
    out.global_name = doc.string(doc.object_get(node, "global_name"));
    out.avatar = doc.string(doc.object_get(node, "avatar"));
    return out;
}

inline Guild parse_guild(const json::Document& doc, int node) {
    Guild out;
    out.id = doc.string(doc.object_get(node, "id"));
    out.name = doc.string(doc.object_get(node, "name"));
    out.icon = doc.string(doc.object_get(node, "icon"));
    out.owner = doc.boolean(doc.object_get(node, "owner"));
    return out;
}

inline Channel parse_channel(const json::Document& doc, int node) {
    Channel out;
    out.id = doc.string(doc.object_get(node, "id"));
    out.guild_id = doc.string(doc.object_get(node, "guild_id"));
    out.parent_id = doc.string(doc.object_get(node, "parent_id"));
    out.name = doc.string(doc.object_get(node, "name"));
    out.type = doc.integer(doc.object_get(node, "type"), -1);
    out.position = doc.integer(doc.object_get(node, "position"), 0);
    return out;
}

inline Message parse_message(const json::Document& doc, int node) {
    Message out;
    out.id = doc.string(doc.object_get(node, "id"));
    out.channel_id = doc.string(doc.object_get(node, "channel_id"));
    out.guild_id = doc.string(doc.object_get(node, "guild_id"));
    out.content = doc.string(doc.object_get(node, "content"));
    out.timestamp = doc.string(doc.object_get(node, "timestamp"));
    out.edited_timestamp = doc.string(doc.object_get(node, "edited_timestamp"));
    int author = doc.object_get(node, "author");
    if (author >= 0) out.author = parse_user(doc, author);

    int attachments = doc.object_get(node, "attachments");
    for (std::size_t i = 0; i < doc.array_size(attachments); ++i) {
        int a = doc.array_at(attachments, i);
        Attachment item;
        item.id = doc.string(doc.object_get(a, "id"));
        item.filename = doc.string(doc.object_get(a, "filename"));
        item.url = doc.string(doc.object_get(a, "url"));
        item.proxy_url = doc.string(doc.object_get(a, "proxy_url"));
        item.content_type = doc.string(doc.object_get(a, "content_type"));
        item.size = doc.integer(doc.object_get(a, "size"), 0);
        out.attachments.push_back(std::move(item));
    }

    int ref = doc.object_get(node, "message_reference");
    if (ref >= 0) out.referenced_message_id = doc.string(doc.object_get(ref, "message_id"));
    return out;
}

} // namespace ps5discord::discord
