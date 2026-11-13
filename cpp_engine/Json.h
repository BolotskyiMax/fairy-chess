#pragma once
// Minimal JSON support for the engine protocol.
// Requests are flat objects ({"cmd":"move","from":52,"to":36}), so JsonObject parses exactly
// that: string / number / bool / null values. Nested values are rejected with an error.

#include <map>
#include <string>

class JsonObject {
public:
    static JsonObject parse(const std::string& text);  // throws std::runtime_error

    bool has(const std::string& key) const { return values_.count(key) != 0; }
    std::string getString(const std::string& key, const std::string& fallback = "") const;
    long long getInt(const std::string& key, long long fallback = 0) const;

private:
    struct Value {
        bool isString = false;
        std::string text;
    };
    std::map<std::string, Value> values_;
};

// Escapes a string and wraps it in quotes: abc"d -> "abc\"d".
std::string jsonQuote(const std::string& text);
