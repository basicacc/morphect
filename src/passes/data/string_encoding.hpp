/*
 * string_encoding.hpp
 *
 * encode strings so they don't appear in plaintext in the binary
 * supports xor, rolling xor, rc4, etc.
 */

#ifndef MORPHECT_STRING_ENCODING_HPP
#define MORPHECT_STRING_ENCODING_HPP

#include "data_base.hpp"
#include "../../core/transformation_base.hpp"

#include <regex>
#include <unordered_set>

namespace morphect {
namespace data {

class StringEncoder {
public:
    StringEncoder() : logger_("StringEncoder") {}

    void configure(const StringEncodingConfig& config) {
        config_ = config;
    }

    EncodedString encode(const std::string& str) const {
        StringEncodingMethod method = config_.method;

        if (config_.randomize_method) {
            std::vector<StringEncodingMethod> methods = {
                StringEncodingMethod::XOR,
                StringEncodingMethod::RollingXOR,
                StringEncodingMethod::ChainedXOR,
                StringEncodingMethod::ByteSwapXOR
            };
            method = methods[GlobalRandom::nextSize(methods.size())];
        }

        return encodeWithMethod(str, method);
    }

    EncodedString encodeWithMethod(const std::string& str, StringEncodingMethod method) const {
        EncodedString result;

        switch (method) {
            case StringEncodingMethod::XOR:
                result = encodeXOR(str, config_.xor_key);
                break;
            case StringEncodingMethod::RollingXOR:
                result = encodeRollingXOR(str, config_.xor_key);
                break;
            case StringEncodingMethod::MultiByteXOR:
                result = encodeMultiByteXOR(str, config_.multi_byte_key);
                break;
            case StringEncodingMethod::Base64XOR:
                result = encodeBase64XOR(str, config_.xor_key);
                break;
            case StringEncodingMethod::ChainedXOR:
                result = encodeChainedXOR(str, config_.xor_key);
                break;
            case StringEncodingMethod::ByteSwapXOR:
                result = encodeByteSwapXOR(str, config_.xor_key);
                break;
            case StringEncodingMethod::RC4:
                result = encodeRC4(str, config_.rc4_key);
                break;
            default:
                result = encodeXOR(str, config_.xor_key);
                break;
        }

        result.method = method;
        return result;
    }

    SplitString splitAndEncode(const std::string& str) const {
        SplitString result;
        result.original = str;

        int num_parts = GlobalRandom::nextInt(2, config_.max_split_parts);
        if (str.length() < static_cast<size_t>(num_parts * 2)) {
            num_parts = 2;
        }

        size_t part_size = str.length() / num_parts;
        size_t pos = 0;

        for (int i = 0; i < num_parts; i++) {
            size_t end;
            if (i == num_parts - 1) {
                end = str.length();
            } else {
                int variance = GlobalRandom::nextInt(-2, 3);
                end = pos + part_size + variance;
                if (end <= pos) end = pos + 1;
                if (end > str.length()) end = str.length();
            }

            std::string part = str.substr(pos, end - pos);
            result.parts.push_back(part);
            result.split_points.push_back(pos);

            uint8_t part_key = config_.xor_key ^ static_cast<uint8_t>(i * 31);
            result.encoded_parts.push_back(encodeXOR(part, part_key));

            pos = end;
        }

        return result;
    }

    bool shouldEncode(const std::string& str) const {
        if (str.length() < static_cast<size_t>(config_.min_string_length)) {
            return false;
        }

        for (const auto& pattern : config_.exclude_patterns) {
            if (str.find(pattern) != std::string::npos) {
                return false;
            }
        }

        // skip printf-style format strings
        if (!config_.encode_format_strings) {
            if (str.find('%') != std::string::npos) {
                std::regex fmt_re(R"(%[-+0 #]*\d*\.?\d*[diouxXeEfFgGaAcspn%])");
                if (std::regex_search(str, fmt_re)) {
                    return false;
                }
            }
        }

        return true;
    }

    std::string generateDecoderCall(const EncodedString& encoded,
                                    const std::string& var_name) const {
        std::ostringstream oss;

        switch (config_.method) {
            case StringEncodingMethod::XOR:
                oss << config_.decoder_function << "("
                    << var_name << ", "
                    << encoded.length << ", "
                    << static_cast<int>(encoded.key) << ")";
                break;
            case StringEncodingMethod::RollingXOR:
                oss << config_.decoder_function << "_rolling("
                    << var_name << ", "
                    << encoded.length << ", "
                    << static_cast<int>(encoded.key) << ")";
                break;
            default:
                oss << config_.decoder_function << "("
                    << var_name << ", "
                    << encoded.length << ", "
                    << static_cast<int>(encoded.key) << ")";
        }

        return oss.str();
    }

private:
    StringEncodingConfig config_;
    Logger logger_;

    EncodedString encodeXOR(const std::string& str, uint8_t key) const {
        EncodedString result;
        result.original = str;
        result.key = key;
        result.length = str.length();

        result.encoded_bytes.reserve(str.length());
        for (char c : str) {
            result.encoded_bytes.push_back(static_cast<uint8_t>(c) ^ key);
        }

        return result;
    }

    // each byte xored with previous decoded byte
    EncodedString encodeRollingXOR(const std::string& str, uint8_t init_key) const {
        EncodedString result;
        result.original = str;
        result.key = init_key;
        result.length = str.length();

        uint8_t key = init_key;
        result.encoded_bytes.reserve(str.length());
        for (char c : str) {
            uint8_t plain = static_cast<uint8_t>(c);
            result.encoded_bytes.push_back(plain ^ key);
            key = plain;
        }

        return result;
    }

    EncodedString encodeMultiByteXOR(const std::string& str,
                                     const std::vector<uint8_t>& key) const {
        EncodedString result;
        result.original = str;
        result.key = key.empty() ? 0 : key[0];
        result.length = str.length();

        if (key.empty()) {
            return encodeXOR(str, config_.xor_key);
        }

        result.encoded_bytes.reserve(str.length());
        for (size_t i = 0; i < str.length(); i++) {
            uint8_t k = key[i % key.size()];
            result.encoded_bytes.push_back(static_cast<uint8_t>(str[i]) ^ k);
        }

        return result;
    }

    // base64 then xor
    EncodedString encodeBase64XOR(const std::string& str, uint8_t key) const {
        EncodedString result;
        result.original = str;
        result.key = key;

        std::string base64 = base64Encode(str);
        result.length = base64.length();
        result.encoded_bytes.reserve(base64.length());
        for (char c : base64) {
            result.encoded_bytes.push_back(static_cast<uint8_t>(c) ^ key);
        }

        return result;
    }

    // position-dependent key: k[i] = (init_key * (i + 1) + i) % 256
    EncodedString encodeChainedXOR(const std::string& str, uint8_t init_key) const {
        EncodedString result;
        result.original = str;
        result.key = init_key;
        result.length = str.length();

        result.encoded_bytes.reserve(str.length());
        for (size_t i = 0; i < str.length(); i++) {
            uint8_t k = static_cast<uint8_t>((init_key * (i + 1) + i) & 0xFF);
            result.encoded_bytes.push_back(static_cast<uint8_t>(str[i]) ^ k);
        }

        return result;
    }

    // swap adjacent bytes then xor
    EncodedString encodeByteSwapXOR(const std::string& str, uint8_t key) const {
        EncodedString result;
        result.original = str;
        result.key = key;
        result.length = str.length();

        std::string swapped = str;
        for (size_t i = 0; i + 1 < swapped.length(); i += 2) {
            std::swap(swapped[i], swapped[i + 1]);
        }

        result.encoded_bytes.reserve(swapped.length());
        for (char c : swapped) {
            result.encoded_bytes.push_back(static_cast<uint8_t>(c) ^ key);
        }

        return result;
    }

    EncodedString encodeRC4(const std::string& str, const std::vector<uint8_t>& key) const {
        EncodedString result;
        result.original = str;
        result.key = key.empty() ? config_.xor_key : key[0];
        result.length = str.length();

        std::vector<uint8_t> rc4_key = key;
        if (rc4_key.empty()) {
            rc4_key = {config_.xor_key, 0x5A, 0xA5, 0x3C};
        }

        // rc4 state init
        std::vector<uint8_t> S(256);
        for (int i = 0; i < 256; i++) S[i] = static_cast<uint8_t>(i);

        // ksa
        int j = 0;
        for (int i = 0; i < 256; i++) {
            j = (j + S[i] + rc4_key[i % rc4_key.size()]) & 0xFF;
            std::swap(S[i], S[j]);
        }

        // prga
        result.encoded_bytes.reserve(str.length());
        int i = 0;
        j = 0;
        for (char c : str) {
            i = (i + 1) & 0xFF;
            j = (j + S[i]) & 0xFF;
            std::swap(S[i], S[j]);
            uint8_t k = S[(S[i] + S[j]) & 0xFF];
            result.encoded_bytes.push_back(static_cast<uint8_t>(c) ^ k);
        }

        return result;
    }

    std::string base64Encode(const std::string& input) const {
        static const char* alphabet =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string result;
        size_t len = input.length();
        result.reserve(((len + 2) / 3) * 4);

        for (size_t i = 0; i < len; i += 3) {
            uint32_t octet_a = static_cast<uint8_t>(input[i]);
            uint32_t octet_b = (i + 1 < len) ? static_cast<uint8_t>(input[i + 1]) : 0;
            uint32_t octet_c = (i + 2 < len) ? static_cast<uint8_t>(input[i + 2]) : 0;

            uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

            result += alphabet[(triple >> 18) & 0x3F];
            result += alphabet[(triple >> 12) & 0x3F];
            result += (i + 1 < len) ? alphabet[(triple >> 6) & 0x3F] : '=';
            result += (i + 2 < len) ? alphabet[triple & 0x3F] : '=';
        }

        return result;
    }

    std::string base64Decode(const std::string& input) const {
        static const uint8_t decode_table[256] = {
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
            52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
            64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
            15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
            64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
            41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
        };

        std::string result;
        result.reserve((input.length() / 4) * 3);

        uint32_t buffer = 0;
        int bits_collected = 0;

        for (char c : input) {
            if (c == '=') break;
            uint8_t val = decode_table[static_cast<uint8_t>(c)];
            if (val == 64) continue;

            buffer = (buffer << 6) | val;
            bits_collected += 6;

            if (bits_collected >= 8) {
                bits_collected -= 8;
                result += static_cast<char>((buffer >> bits_collected) & 0xFF);
            }
        }

        return result;
    }
};

class LLVMStringEncodingPass : public LLVMTransformationPass {
public:
    LLVMStringEncodingPass() : logger_("StringEncodingPass") {}

    std::string getName() const override { return "StringEncoding"; }
    std::string getDescription() const override {
        return "Encodes string literals using XOR";
    }

    PassPriority getPriority() const override { return PassPriority::Data; }

    bool initialize(const PassConfig& config) override {
        TransformationPass::initialize(config);
        return true;
    }

    void configure(const StringEncodingConfig& config) {
        str_config_ = config;
        encoder_.configure(config);
    }

    TransformResult transformIR(std::vector<std::string>& lines) override {
        if (!str_config_.enabled) {
            return TransformResult::Skipped;
        }

        int transformations = 0;

        // match global string constants
        std::regex string_const_re(
            R"((@[\w.]+)\s*=\s*(private\s+)?unnamed_addr\s+constant\s+\[(\d+)\s+x\s+i8\]\s+c\"([^\"]*)(\\00)?\")"
        );

        for (auto& line : lines) {
            std::smatch match;
            if (std::regex_search(line, match, string_const_re)) {
                std::string var_name = match[1];
                std::string str_content = match[4];

                // Unescape the string
                std::string unescaped = unescapeString(str_content);

                if (encoder_.shouldEncode(unescaped)) {
                    EncodedString encoded = encoder_.encode(unescaped);

                    std::ostringstream new_line;
                    new_line << var_name << " = private unnamed_addr constant ["
                             << (encoded.length + 1) << " x i8] c\""
                             << encoded.toHexString() << "\\00\"";
                    new_line << " ; MORPHECT_ENCODED key=" << static_cast<int>(encoded.key);

                    line = new_line.str();
                    transformations++;
                    incrementStat("strings_encoded");
                    encoded_strings_[var_name] = encoded;

                    logger_.debug("Encoded string {}: \"{}\"", var_name,
                                 unescaped.substr(0, 20) + (unescaped.length() > 20 ? "..." : ""));
                }
            }
        }

        incrementStat("total_transformations", transformations);
        return transformations > 0 ? TransformResult::Success : TransformResult::NotApplicable;
    }

    const std::unordered_map<std::string, EncodedString>& getEncodedStrings() const {
        return encoded_strings_;
    }

private:
    Logger logger_;
    StringEncodingConfig str_config_;
    StringEncoder encoder_;
    std::unordered_map<std::string, EncodedString> encoded_strings_;

    std::string unescapeString(const std::string& str) const {
        std::string result;
        result.reserve(str.length());

        for (size_t i = 0; i < str.length(); i++) {
            if (str[i] == '\\' && i + 2 < str.length()) {
                char hex[3] = {str[i+1], str[i+2], 0};
                char* end;
                long val = strtol(hex, &end, 16);
                if (end == hex + 2) {
                    result += static_cast<char>(val);
                    i += 2;
                    continue;
                }
            }
            result += str[i];
        }

        return result;
    }
};

class GimpleStringEncodingPass : public GimpleTransformationPass {
public:
    GimpleStringEncodingPass() : logger_("GimpleStringEncoding") {}

    std::string getName() const override { return "StringEncoding"; }
    std::string getDescription() const override {
        return "Encodes string literals using XOR";
    }

    PassPriority getPriority() const override { return PassPriority::Data; }

    void configure(const StringEncodingConfig& config) {
        config_ = config;
        encoder_.configure(config);
    }

    TransformResult transformGimple(void* func) override {
        (void)func;  // impl in plugin
        return TransformResult::NotApplicable;
    }

private:
    Logger logger_;
    StringEncodingConfig config_;
    StringEncoder encoder_;
};

} // namespace data
} // namespace morphect

#endif // MORPHECT_STRING_ENCODING_HPP
