#pragma once

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ps5discord::json {

enum class Type : std::uint8_t { invalid, object, array, string, number, boolean, null_value };

struct Node {
    Type type = Type::invalid;
    std::size_t begin = 0;
    std::size_t end = 0;
    int parent = -1;
    int first_child = -1;
    int next_sibling = -1;
};

class Document {
public:
    bool parse(std::string text) {
        text_ = std::move(text);
        nodes_.clear();
        error_.clear();
        std::size_t p = 0;
        skip_ws(p);
        int root = parse_value(p, -1);
        if (root < 0) return false;
        skip_ws(p);
        if (p != text_.size()) return fail("trailing characters");
        root_ = root;
        return true;
    }

    int root() const { return root_; }
    const std::string& error() const { return error_; }
    Type type(int n) const { return valid(n) ? nodes_[n].type : Type::invalid; }

    int object_get(int object, std::string_view key) const {
        if (!valid(object) || nodes_[object].type != Type::object) return -1;
        int child = nodes_[object].first_child;
        while (child >= 0) {
            int value = nodes_[child].next_sibling;
            if (value < 0) return -1;
            if (decode_string(child) == key) return value;
            child = nodes_[value].next_sibling;
        }
        return -1;
    }

    std::size_t array_size(int array) const {
        if (!valid(array) || nodes_[array].type != Type::array) return 0;
        std::size_t count = 0;
        for (int c = nodes_[array].first_child; c >= 0; c = nodes_[c].next_sibling) ++count;
        return count;
    }

    int array_at(int array, std::size_t index) const {
        if (!valid(array) || nodes_[array].type != Type::array) return -1;
        int c = nodes_[array].first_child;
        while (c >= 0 && index--) c = nodes_[c].next_sibling;
        return c;
    }

    std::string string(int n, std::string_view fallback = {}) const {
        if (!valid(n) || nodes_[n].type != Type::string) return std::string(fallback);
        return decode_string(n);
    }

    std::int64_t integer(int n, std::int64_t fallback = 0) const {
        if (!valid(n) || nodes_[n].type != Type::number) return fallback;
        const auto& v = nodes_[n];
        std::int64_t out = fallback;
        auto first = text_.data() + v.begin;
        auto last = text_.data() + v.end;
        auto r = std::from_chars(first, last, out);
        return r.ec == std::errc{} ? out : fallback;
    }

    bool boolean(int n, bool fallback = false) const {
        if (!valid(n) || nodes_[n].type != Type::boolean) return fallback;
        const auto& v = nodes_[n];
        return text_.substr(v.begin, v.end - v.begin) == "true";
    }

    std::string_view raw(int n) const {
        if (!valid(n)) return {};
        const auto& v = nodes_[n];
        return std::string_view(text_).substr(v.begin, v.end - v.begin);
    }

private:
    bool valid(int n) const { return n >= 0 && static_cast<std::size_t>(n) < nodes_.size(); }
    bool fail(std::string message) { error_ = std::move(message); return false; }

    void skip_ws(std::size_t& p) const {
        while (p < text_.size()) {
            char c = text_[p];
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') ++p; else break;
        }
    }

    int add(Type type, std::size_t begin, int parent) {
        nodes_.push_back(Node{type, begin, begin, parent, -1, -1});
        int idx = static_cast<int>(nodes_.size() - 1);
        if (parent >= 0) {
            int* slot = &nodes_[parent].first_child;
            while (*slot >= 0) slot = &nodes_[*slot].next_sibling;
            *slot = idx;
        }
        return idx;
    }

    int parse_value(std::size_t& p, int parent) {
        skip_ws(p);
        if (p >= text_.size()) { fail("unexpected end of JSON"); return -1; }
        char c = text_[p];
        if (c == '{') return parse_object(p, parent);
        if (c == '[') return parse_array(p, parent);
        if (c == '"') return parse_string(p, parent);
        if ((c >= '0' && c <= '9') || c == '-') return parse_number(p, parent);
        if (text_.compare(p, 4, "true") == 0 || text_.compare(p, 5, "false") == 0)
            return parse_literal(p, parent, Type::boolean);
        if (text_.compare(p, 4, "null") == 0) return parse_literal(p, parent, Type::null_value);
        fail("invalid JSON value");
        return -1;
    }

    int parse_object(std::size_t& p, int parent) {
        int idx = add(Type::object, p, parent);
        ++p;
        skip_ws(p);
        if (p < text_.size() && text_[p] == '}') { nodes_[idx].end = ++p; return idx; }
        while (p < text_.size()) {
            skip_ws(p);
            if (p >= text_.size() || text_[p] != '"') { fail("object key must be string"); return -1; }
            if (parse_string(p, idx) < 0) return -1;
            skip_ws(p);
            if (p >= text_.size() || text_[p++] != ':') { fail("missing object colon"); return -1; }
            if (parse_value(p, idx) < 0) return -1;
            skip_ws(p);
            if (p < text_.size() && text_[p] == ',') { ++p; continue; }
            if (p < text_.size() && text_[p] == '}') { nodes_[idx].end = ++p; return idx; }
            fail("missing object delimiter");
            return -1;
        }
        fail("unterminated object");
        return -1;
    }

    int parse_array(std::size_t& p, int parent) {
        int idx = add(Type::array, p, parent);
        ++p;
        skip_ws(p);
        if (p < text_.size() && text_[p] == ']') { nodes_[idx].end = ++p; return idx; }
        while (p < text_.size()) {
            if (parse_value(p, idx) < 0) return -1;
            skip_ws(p);
            if (p < text_.size() && text_[p] == ',') { ++p; continue; }
            if (p < text_.size() && text_[p] == ']') { nodes_[idx].end = ++p; return idx; }
            fail("missing array delimiter");
            return -1;
        }
        fail("unterminated array");
        return -1;
    }

    int parse_string(std::size_t& p, int parent) {
        int idx = add(Type::string, p + 1, parent);
        ++p;
        while (p < text_.size()) {
            char c = text_[p++];
            if (c == '"') { nodes_[idx].end = p - 1; return idx; }
            if (c == '\\') {
                if (p >= text_.size()) { fail("bad escape"); return -1; }
                char e = text_[p++];
                if (e == 'u') {
                    for (int i = 0; i < 4; ++i) {
                        if (p >= text_.size()) { fail("bad unicode escape"); return -1; }
                        char h = text_[p++];
                        bool hex = (h >= '0' && h <= '9') || (h >= 'a' && h <= 'f') || (h >= 'A' && h <= 'F');
                        if (!hex) { fail("bad unicode escape"); return -1; }
                    }
                }
            }
        }
        fail("unterminated string");
        return -1;
    }

    int parse_number(std::size_t& p, int parent) {
        std::size_t begin = p;
        if (text_[p] == '-') ++p;
        while (p < text_.size() && text_[p] >= '0' && text_[p] <= '9') ++p;
        if (p < text_.size() && text_[p] == '.') {
            ++p;
            while (p < text_.size() && text_[p] >= '0' && text_[p] <= '9') ++p;
        }
        if (p < text_.size() && (text_[p] == 'e' || text_[p] == 'E')) {
            ++p;
            if (p < text_.size() && (text_[p] == '+' || text_[p] == '-')) ++p;
            while (p < text_.size() && text_[p] >= '0' && text_[p] <= '9') ++p;
        }
        int idx = add(Type::number, begin, parent);
        nodes_[idx].end = p;
        return idx;
    }

    int parse_literal(std::size_t& p, int parent, Type type) {
        std::size_t begin = p;
        if (text_.compare(p, 4, "true") == 0 || text_.compare(p, 4, "null") == 0) p += 4;
        else if (text_.compare(p, 5, "false") == 0) p += 5;
        else { fail("invalid literal"); return -1; }
        int idx = add(type, begin, parent);
        nodes_[idx].end = p;
        return idx;
    }

    static int hex_value(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + c - 'a';
        if (c >= 'A' && c <= 'F') return 10 + c - 'A';
        return 0;
    }

    static void append_utf8(std::string& out, unsigned cp) {
        if (cp <= 0x7f) out.push_back(static_cast<char>(cp));
        else if (cp <= 0x7ff) {
            out.push_back(static_cast<char>(0xc0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
        } else {
            out.push_back(static_cast<char>(0xe0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
        }
    }

    std::string decode_string(int n) const {
        const auto& v = nodes_[n];
        std::string out;
        out.reserve(v.end - v.begin);
        for (std::size_t p = v.begin; p < v.end; ++p) {
            char c = text_[p];
            if (c != '\\') { out.push_back(c); continue; }
            if (++p >= v.end) break;
            char e = text_[p];
            switch (e) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    if (p + 4 < v.end) {
                        unsigned cp = 0;
                        for (int i = 0; i < 4; ++i) cp = (cp << 4) | static_cast<unsigned>(hex_value(text_[++p]));
                        append_utf8(out, cp);
                    }
                    break;
                }
                default: out.push_back(e); break;
            }
        }
        return out;
    }

    std::string text_;
    std::vector<Node> nodes_;
    std::string error_;
    int root_ = -1;
};

inline std::string escape(std::string_view input) {
    static const char* hex = "0123456789abcdef";
    std::string out;
    out.reserve(input.size() + 16);
    for (unsigned char c : input) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += "\\u00";
                    out.push_back(hex[(c >> 4) & 0xf]);
                    out.push_back(hex[c & 0xf]);
                } else out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

} // namespace ps5discord::json
