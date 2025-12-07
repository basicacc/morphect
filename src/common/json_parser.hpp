/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * json_parser.hpp - Lightweight JSON parser (no external dependencies)
 *
 * Features:
 *   - Parse JSON config files
 *   - Extract strings, numbers, arrays, objects
 *   - No external library dependencies
 *   - Header-only implementation
 */

#ifndef MORPHECT_JSON_PARSER_HPP
#define MORPHECT_JSON_PARSER_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <optional>

namespace morphect {

/**
 * JSON Value types
 */
enum class JsonType {
    Null,
    Bool,
    Number,
    String,
    Array,
    Object
};

/**
 * Forward declaration
 */
class JsonValue;

/**
 * JSON Value class - represents any JSON value
 */
class JsonValue {
public:
    JsonType type = JsonType::Null;

    // Value storage
    bool bool_value = false;
    double number_value = 0.0;
    std::string string_value;
    std::vector<JsonValue> array_value;
    std::unordered_map<std::string, JsonValue> object_value;

    // Constructors
    JsonValue() : type(JsonType::Null) {}
    JsonValue(bool v) : type(JsonType::Bool), bool_value(v) {}
    JsonValue(int v) : type(JsonType::Number), number_value(v) {}
    JsonValue(double v) : type(JsonType::Number), number_value(v) {}
    JsonValue(const std::string& v) : type(JsonType::String), string_value(v) {}
    JsonValue(const char* v) : type(JsonType::String), string_value(v) {}

    // Type checks
    bool isNull() const { return type == JsonType::Null; }
    bool isBool() const { return type == JsonType::Bool; }
    bool isNumber() const { return type == JsonType::Number; }
    bool isString() const { return type == JsonType::String; }
    bool isArray() const { return type == JsonType::Array; }
    bool isObject() const { return type == JsonType::Object; }

    // Accessors with defaults
    bool asBool(bool def = false) const {
        return isBool() ? bool_value : def;
    }

    double asDouble(double def = 0.0) const {
        return isNumber() ? number_value : def;
    }

    int asInt(int def = 0) const {
        return isNumber() ? static_cast<int>(number_value) : def;
    }

    const std::string& asString(const std::string& def = "") const {
        static std::string empty;
        return isString() ? string_value : (def.empty() ? empty : def);
    }

    // Array access
    size_t size() const {
        if (isArray()) return array_value.size();
        if (isObject()) return object_value.size();
        return 0;
    }

    const JsonValue& operator[](size_t index) const {
        static JsonValue null_value;
        if (!isArray() || index >= array_value.size()) {
            return null_value;
        }
        return array_value[index];
    }

    // Object access
    const JsonValue& operator[](const std::string& key) const {
        static JsonValue null_value;
        if (!isObject()) return null_value;
        auto it = object_value.find(key);
        return it != object_value.end() ? it->second : null_value;
    }

    bool has(const std::string& key) const {
        return isObject() && object_value.find(key) != object_value.end();
    }

    // Get nested value with dot notation (e.g., "obfuscation.mba.enabled")
    const JsonValue& get(const std::string& path) const {
        static JsonValue null_value;

        size_t dot = path.find('.');
        if (dot == std::string::npos) {
            return (*this)[path];
        }

        std::string key = path.substr(0, dot);
        std::string rest = path.substr(dot + 1);

        const JsonValue& child = (*this)[key];
        if (child.isNull()) return null_value;

        return child.get(rest);
    }

    // Convenience methods for common patterns
    std::vector<std::string> asStringArray() const {
        std::vector<std::string> result;
        if (isArray()) {
            for (const auto& item : array_value) {
                if (item.isString()) {
                    result.push_back(item.string_value);
                }
            }
        }
        return result;
    }

    std::vector<double> asDoubleArray() const {
        std::vector<double> result;
        if (isArray()) {
            for (const auto& item : array_value) {
                if (item.isNumber()) {
                    result.push_back(item.number_value);
                }
            }
        }
        return result;
    }
};

/**
 * JSON Parser - parses JSON text into JsonValue
 */
class JsonParser {
public:
    /**
     * Parse JSON string
     *
     * @param json JSON text
     * @return Parsed JsonValue
     * @throws std::runtime_error on parse error
     */
    static JsonValue parse(const std::string& json) {
        JsonParser parser(json);
        return parser.parseValue();
    }

    /**
     * Parse JSON file
     *
     * @param path File path
     * @return Parsed JsonValue
     * @throws std::runtime_error on file or parse error
     */
    static JsonValue parseFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + path);
        }

        std::string content((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());
        return parse(content);
    }

    /**
     * Try to parse JSON file, return empty object on failure
     */
    static JsonValue tryParseFile(const std::string& path) {
        try {
            return parseFile(path);
        } catch (...) {
            JsonValue empty;
            empty.type = JsonType::Object;
            return empty;
        }
    }

private:
    std::string json_;
    size_t pos_ = 0;

    explicit JsonParser(const std::string& json) : json_(json) {}

    char peek() const {
        return pos_ < json_.size() ? json_[pos_] : '\0';
    }

    char get() {
        return pos_ < json_.size() ? json_[pos_++] : '\0';
    }

    void skipWhitespace() {
        while (pos_ < json_.size() && std::isspace(json_[pos_])) {
            pos_++;
        }
    }

    void expect(char c) {
        skipWhitespace();
        if (get() != c) {
            throw std::runtime_error(std::string("Expected '") + c + "' at position " +
                                    std::to_string(pos_));
        }
    }

    JsonValue parseValue() {
        skipWhitespace();
        char c = peek();

        if (c == '"') return parseString();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == 't' || c == 'f') return parseBool();
        if (c == 'n') return parseNull();
        if (c == '-' || std::isdigit(c)) return parseNumber();

        throw std::runtime_error("Unexpected character at position " + std::to_string(pos_));
    }

    JsonValue parseString() {
        expect('"');
        std::string result;

        while (pos_ < json_.size()) {
            char c = get();

            if (c == '"') {
                JsonValue v;
                v.type = JsonType::String;
                v.string_value = result;
                return v;
            }

            if (c == '\\') {
                char escaped = get();
                switch (escaped) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        // Parse unicode escape (simplified - just skip)
                        pos_ += 4;
                        result += '?';
                        break;
                    }
                    default: result += escaped;
                }
            } else {
                result += c;
            }
        }

        throw std::runtime_error("Unterminated string");
    }

    JsonValue parseNumber() {
        skipWhitespace();
        size_t start = pos_;

        if (peek() == '-') pos_++;

        // Handle hex numbers (0x...)
        if (peek() == '0' && pos_ + 1 < json_.size() &&
            (json_[pos_ + 1] == 'x' || json_[pos_ + 1] == 'X')) {
            pos_ += 2;
            while (pos_ < json_.size() && std::isxdigit(json_[pos_])) {
                pos_++;
            }
            std::string hex_str = json_.substr(start, pos_ - start);
            JsonValue v;
            v.type = JsonType::Number;
            v.number_value = static_cast<double>(std::stoll(hex_str, nullptr, 0));
            return v;
        }

        while (pos_ < json_.size() && std::isdigit(json_[pos_])) pos_++;

        if (peek() == '.') {
            pos_++;
            while (pos_ < json_.size() && std::isdigit(json_[pos_])) pos_++;
        }

        if (peek() == 'e' || peek() == 'E') {
            pos_++;
            if (peek() == '+' || peek() == '-') pos_++;
            while (pos_ < json_.size() && std::isdigit(json_[pos_])) pos_++;
        }

        std::string num_str = json_.substr(start, pos_ - start);
        JsonValue v;
        v.type = JsonType::Number;
        v.number_value = std::stod(num_str);
        return v;
    }

    JsonValue parseBool() {
        skipWhitespace();
        JsonValue v;
        v.type = JsonType::Bool;

        if (json_.substr(pos_, 4) == "true") {
            pos_ += 4;
            v.bool_value = true;
        } else if (json_.substr(pos_, 5) == "false") {
            pos_ += 5;
            v.bool_value = false;
        } else {
            throw std::runtime_error("Invalid boolean at position " + std::to_string(pos_));
        }

        return v;
    }

    JsonValue parseNull() {
        skipWhitespace();
        if (json_.substr(pos_, 4) == "null") {
            pos_ += 4;
            return JsonValue();
        }
        throw std::runtime_error("Invalid null at position " + std::to_string(pos_));
    }

    JsonValue parseArray() {
        expect('[');
        JsonValue v;
        v.type = JsonType::Array;

        skipWhitespace();
        if (peek() == ']') {
            get();
            return v;
        }

        while (true) {
            v.array_value.push_back(parseValue());
            skipWhitespace();

            if (peek() == ']') {
                get();
                return v;
            }

            expect(',');
        }
    }

    JsonValue parseObject() {
        expect('{');
        JsonValue v;
        v.type = JsonType::Object;

        skipWhitespace();
        if (peek() == '}') {
            get();
            return v;
        }

        while (true) {
            skipWhitespace();

            // Parse key
            JsonValue key = parseString();
            if (!key.isString()) {
                throw std::runtime_error("Object key must be a string");
            }

            expect(':');

            // Parse value
            v.object_value[key.string_value] = parseValue();

            skipWhitespace();

            if (peek() == '}') {
                get();
                return v;
            }

            expect(',');
        }
    }
};

/**
 * JSON Serializer - convert JsonValue back to string
 */
class JsonSerializer {
public:
    static std::string serialize(const JsonValue& value, bool pretty = true, int indent = 0) {
        std::ostringstream oss;
        serializeValue(oss, value, pretty, indent);
        return oss.str();
    }

private:
    static void serializeValue(std::ostringstream& oss, const JsonValue& v,
                               bool pretty, int indent) {
        switch (v.type) {
            case JsonType::Null:
                oss << "null";
                break;

            case JsonType::Bool:
                oss << (v.bool_value ? "true" : "false");
                break;

            case JsonType::Number:
                oss << v.number_value;
                break;

            case JsonType::String:
                oss << '"' << escapeString(v.string_value) << '"';
                break;

            case JsonType::Array:
                serializeArray(oss, v, pretty, indent);
                break;

            case JsonType::Object:
                serializeObject(oss, v, pretty, indent);
                break;
        }
    }

    static std::string escapeString(const std::string& s) {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c;
            }
        }
        return result;
    }

    static void serializeArray(std::ostringstream& oss, const JsonValue& v,
                               bool pretty, int indent) {
        oss << '[';

        bool first = true;
        for (const auto& item : v.array_value) {
            if (!first) oss << ',';
            if (pretty) oss << '\n' << std::string(indent + 2, ' ');
            serializeValue(oss, item, pretty, indent + 2);
            first = false;
        }

        if (pretty && !v.array_value.empty()) {
            oss << '\n' << std::string(indent, ' ');
        }
        oss << ']';
    }

    static void serializeObject(std::ostringstream& oss, const JsonValue& v,
                                bool pretty, int indent) {
        oss << '{';

        bool first = true;
        for (const auto& [key, value] : v.object_value) {
            if (!first) oss << ',';
            if (pretty) oss << '\n' << std::string(indent + 2, ' ');
            oss << '"' << escapeString(key) << '"';
            oss << ':';
            if (pretty) oss << ' ';
            serializeValue(oss, value, pretty, indent + 2);
            first = false;
        }

        if (pretty && !v.object_value.empty()) {
            oss << '\n' << std::string(indent, ' ');
        }
        oss << '}';
    }
};

} // namespace morphect

#endif // MORPHECT_JSON_PARSER_HPP
