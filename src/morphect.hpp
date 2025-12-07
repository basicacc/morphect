/*
 * morphect.hpp - main include file
 *
 * just include this and you get everything
 */

#ifndef MORPHECT_HPP
#define MORPHECT_HPP

// Version information
#define MORPHECT_VERSION_MAJOR 1
#define MORPHECT_VERSION_MINOR 0
#define MORPHECT_VERSION_PATCH 1
#define MORPHECT_VERSION_STRING "1.0.1"

// Core components
#include "core/transformation_base.hpp"
#include "core/pass_manager.hpp"
#include "core/statistics.hpp"

// Common utilities
#include "common/logging.hpp"
#include "common/random.hpp"
#include "common/json_parser.hpp"

// Transformation passes
#include "passes/mba/mba.hpp"
#include "passes/data/data.hpp"
#include "passes/cff/cff.hpp"

namespace morphect {

// version string
inline const char* getVersion() {
    return MORPHECT_VERSION_STRING;
}

// ascii art banner
inline const char* getBanner() {
    return R"(
  __  __                  _               _
 |  \/  | ___  _ __ _ __ | |__   ___  ___| |_
 | |\/| |/ _ \| '__| '_ \| '_ \ / _ \/ __| __|
 | |  | | (_) | |  | |_) | | | |  __/ (__| |_
 |_|  |_|\___/|_|  | .__/|_| |_|\___|\___|\__|
                   |_|
  Multi-Language Code Obfuscator v)" MORPHECT_VERSION_STRING R"(
)";
}

inline void printBanner() {
    std::cout << getBanner() << std::endl;
}

// config options - most of these have sane defaults
struct MorphectConfig {
    double global_probability = 0.85;
    int verbosity = 1;  // 0=silent, 1=normal, 2=verbose, 3=debug
    bool preserve_functionality = true;

    // what to enable
    bool enable_mba = true;
    bool enable_cff = false;
    bool enable_bogus_cf = false;
    bool enable_string_encoding = true;
    bool enable_constant_obf = true;

    // mba config
    int mba_nesting_depth = 1;
    std::string mba_complexity = "high";

    // string encoding
    std::string string_encoding_method = "xor";
    uint8_t string_xor_key = 0x7B;
    int string_min_length = 3;

    // for reproducible builds
    bool use_fixed_seed = false;
    uint64_t random_seed = 0;

    // output options
    bool print_statistics = true;
    std::string stats_output_file;

    bool loadFromFile(const std::string& path) {
        try {
            auto json = JsonParser::parseFile(path);
            loadFromJson(json);
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to load config: {}", e.what());
            return false;
        }
    }

    void loadFromJson(const JsonValue& json) {
        if (json.has("global_probability")) {
            global_probability = json["global_probability"].asDouble(0.85);
        }
        if (json.has("verbosity")) {
            verbosity = json["verbosity"].asInt(1);
        }

        // handle both flat config and nested "obfuscation_settings"
        const JsonValue& settings = json.has("obfuscation_settings")
            ? json["obfuscation_settings"]
            : json;

        if (settings.has("global_probability")) {
            global_probability = settings["global_probability"].asDouble(0.85);
        }
        if (settings.has("preserve_functionality")) {
            preserve_functionality = settings["preserve_functionality"].asBool(true);
        }
        if (settings.has("enable_advanced_mba")) {
            enable_mba = settings["enable_advanced_mba"].asBool(true);
        }
        if (settings.has("mba_complexity_level")) {
            mba_complexity = settings["mba_complexity_level"].asString("high");
        }
        if (settings.has("enable_string_encoding")) {
            enable_string_encoding = settings["enable_string_encoding"].asBool(true);
        }

        if (json.has("string_encoding")) {
            const auto& se = json["string_encoding"];
            if (se.has("enabled")) {
                enable_string_encoding = se["enabled"].asBool(true);
            }
            if (se.has("encoding_method")) {
                string_encoding_method = se["encoding_method"].asString("xor");
            }
            if (se.has("xor_key")) {
                string_xor_key = static_cast<uint8_t>(se["xor_key"].asInt(0x7B));
            }
            if (se.has("min_string_length")) {
                string_min_length = se["min_string_length"].asInt(3);
            }
        }

        if (json.has("random_seed")) {
            use_fixed_seed = true;
            random_seed = static_cast<uint64_t>(json["random_seed"].asDouble(0));
        }
    }

    PassConfig toPassConfig() const {
        PassConfig pc;
        pc.enabled = true;
        pc.probability = global_probability;
        pc.verbosity = verbosity;
        return pc;
    }
};

inline void initialize(const MorphectConfig& config) {
    LogConfig::get().setLevel(config.verbosity);

    if (config.use_fixed_seed) {
        GlobalRandom::setSeed(config.random_seed);
    }

    LOG_DEBUG("initialized with prob={}, verbosity={}",
             config.global_probability, config.verbosity);
}

} // namespace morphect

#endif // MORPHECT_HPP
