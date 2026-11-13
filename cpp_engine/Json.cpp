#include "Json.h"

#include <cctype>
#include <cstdio>
#include <stdexcept>

namespace {
    class Parser {
    public:
        explicit Parser(const std::string& text) : s_(text) {}

        void skipSpaces() {
            while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
        }
        bool atEnd() const { return pos_ >= s_.size(); }
        char peek() const { return atEnd() ? '\0' : s_[pos_]; }

        void expect(char ch) {
            skipSpaces();
            if (peek() != ch) fail(std::string("expected '") + ch + "'");
            ++pos_;
        }

        std::string readString() {
            expect('"');
            std::string out;
            while (!atEnd()) {
                char ch = s_[pos_++];
                if (ch == '"') return out;
                if (ch != '\\') {
                    out += ch;
                    continue;
                }
                if (atEnd()) break;
                ch = s_[pos_++];
                switch (ch) {
                    case '"': case '\\': case '/': out += ch; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': out += readUnicodeEscape(); break;
                    default: fail("bad escape sequence");
                }
            }
            fail("unterminated string");
            return out;
        }

        std::string readLiteral() {
            const size_t start = pos_;
            while (!atEnd() && (std::isalnum(static_cast<unsigned char>(s_[pos_])) || s_[pos_] == '-' ||
                                s_[pos_] == '+' || s_[pos_] == '.')) {
                ++pos_;
            }
            if (start == pos_) fail("expected a value");
            return s_.substr(start, pos_ - start);
        }

        [[noreturn]] void fail(const std::string& what) const {
            throw std::runtime_error("Bad JSON at position " + std::to_string(pos_) + ": " + what);
        }

    private:
        std::string readUnicodeEscape() {
            if (pos_ + 4 > s_.size()) fail("bad \\u escape");
            const unsigned code = static_cast<unsigned>(std::stoul(s_.substr(pos_, 4), nullptr, 16));
            pos_ += 4;
            std::string out;  // encode as UTF-8 (BMP only, enough for this protocol)
            if (code < 0x80) {
                out += static_cast<char>(code);
            } else if (code < 0x800) {
                out += static_cast<char>(0xC0 | (code >> 6));
                out += static_cast<char>(0x80 | (code & 0x3F));
            } else {
                out += static_cast<char>(0xE0 | (code >> 12));
                out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (code & 0x3F));
            }
            return out;
        }

        const std::string& s_;
        size_t pos_ = 0;
    };
}

JsonObject JsonObject::parse(const std::string& text) {
    JsonObject result;
    Parser p(text);
    p.expect('{');
    p.skipSpaces();
    if (p.peek() == '}') {
        p.expect('}');
        return result;
    }
    while (true) {
        p.skipSpaces();
        const std::string key = p.readString();
        p.expect(':');
        p.skipSpaces();

        Value value;
        const char next = p.peek();
        if (next == '"') {
            value.isString = true;
            value.text = p.readString();
        } else if (next == '{' || next == '[') {
            p.fail("nested objects and arrays are not supported");
        } else {
            value.text = p.readLiteral();
        }
        result.values_[key] = value;

        p.skipSpaces();
        if (p.peek() == ',') {
            p.expect(',');
            continue;
        }
        p.expect('}');
        break;
    }
    p.skipSpaces();
    if (!p.atEnd()) p.fail("unexpected data after the object");
    return result;
}

std::string JsonObject::getString(const std::string& key, const std::string& fallback) const {
    const auto it = values_.find(key);
    return it != values_.end() && it->second.isString ? it->second.text : fallback;
}

long long JsonObject::getInt(const std::string& key, long long fallback) const {
    const auto it = values_.find(key);
    if (it == values_.end() || it->second.isString) return fallback;
    try {
        size_t used = 0;
        const long long value = std::stoll(it->second.text, &used);
        return used == it->second.text.size() ? value : fallback;
    } catch (const std::exception&) {
        return fallback;
    }
}

std::string jsonQuote(const std::string& text) {
    std::string out = "\"";
    for (unsigned char ch : text) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
                    out += buf;
                } else {
                    out += static_cast<char>(ch);
                }
        }
    }
    out += '"';
    return out;
}
