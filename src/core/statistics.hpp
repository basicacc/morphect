/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * statistics.hpp - Statistics tracking and reporting
 *
 * Tracks all obfuscation metrics:
 *   - Transformations applied per type
 *   - Code size before/after
 *   - Functions processed
 *   - Time spent per pass
 */

#ifndef MORPHECT_STATISTICS_HPP
#define MORPHECT_STATISTICS_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace morphect {

/**
 * Timer utility for measuring pass execution time
 */
class Timer {
public:
    void start() {
        start_time_ = std::chrono::high_resolution_clock::now();
        running_ = true;
    }

    void stop() {
        if (running_) {
            end_time_ = std::chrono::high_resolution_clock::now();
            running_ = false;
        }
    }

    /**
     * Get elapsed time in milliseconds
     */
    double elapsedMs() const {
        auto end = running_ ? std::chrono::high_resolution_clock::now() : end_time_;
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_time_);
        return duration.count() / 1000.0;
    }

    /**
     * Get elapsed time in seconds
     */
    double elapsedSec() const {
        return elapsedMs() / 1000.0;
    }

private:
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;
    bool running_ = false;
};

/**
 * RAII timer that automatically records duration
 */
class ScopedTimer {
public:
    ScopedTimer(double& target) : target_(target) {
        timer_.start();
    }

    ~ScopedTimer() {
        timer_.stop();
        target_ += timer_.elapsedMs();
    }

private:
    Timer timer_;
    double& target_;
};

/**
 * Statistics container for tracking obfuscation metrics
 */
class Statistics {
public:
    /**
     * Set an integer statistic
     */
    void set(const std::string& name, int value) {
        int_stats_[name] = value;
    }

    /**
     * Set a double statistic
     */
    void set(const std::string& name, double value) {
        double_stats_[name] = value;
    }

    /**
     * Set a string statistic
     */
    void set(const std::string& name, const std::string& value) {
        string_stats_[name] = value;
    }

    /**
     * Increment an integer statistic
     */
    void increment(const std::string& name, int amount = 1) {
        int_stats_[name] += amount;
    }

    /**
     * Add to a double statistic
     */
    void add(const std::string& name, double amount) {
        double_stats_[name] += amount;
    }

    /**
     * Get an integer statistic (0 if not found)
     */
    int getInt(const std::string& name) const {
        auto it = int_stats_.find(name);
        return it != int_stats_.end() ? it->second : 0;
    }

    /**
     * Get a double statistic (0.0 if not found)
     */
    double getDouble(const std::string& name) const {
        auto it = double_stats_.find(name);
        return it != double_stats_.end() ? it->second : 0.0;
    }

    /**
     * Get a string statistic ("" if not found)
     */
    std::string getString(const std::string& name) const {
        auto it = string_stats_.find(name);
        return it != string_stats_.end() ? it->second : "";
    }

    /**
     * Check if a statistic exists
     */
    bool has(const std::string& name) const {
        return int_stats_.find(name) != int_stats_.end() ||
               double_stats_.find(name) != double_stats_.end() ||
               string_stats_.find(name) != string_stats_.end();
    }

    /**
     * Get all integer statistics
     */
    const std::unordered_map<std::string, int>& getIntStats() const {
        return int_stats_;
    }

    /**
     * Get all double statistics
     */
    const std::unordered_map<std::string, double>& getDoubleStats() const {
        return double_stats_;
    }

    /**
     * Get all string statistics
     */
    const std::unordered_map<std::string, std::string>& getStringStats() const {
        return string_stats_;
    }

    /**
     * Merge another Statistics object into this one
     */
    void merge(const Statistics& other) {
        for (const auto& [name, value] : other.int_stats_) {
            int_stats_[name] += value;
        }
        for (const auto& [name, value] : other.double_stats_) {
            double_stats_[name] += value;
        }
        for (const auto& [name, value] : other.string_stats_) {
            string_stats_[name] = value;  // Overwrite strings
        }
    }

    /**
     * Clear all statistics
     */
    void clear() {
        int_stats_.clear();
        double_stats_.clear();
        string_stats_.clear();
    }

    /**
     * Get total transformations (sum of all int stats containing "transform")
     */
    int getTotalTransformations() const {
        int total = 0;
        for (const auto& [name, value] : int_stats_) {
            if (name.find("transform") != std::string::npos ||
                name.find("applied") != std::string::npos) {
                total += value;
            }
        }
        return total;
    }

    /**
     * Format statistics as a string report
     */
    std::string format() const {
        std::ostringstream oss;

        oss << "=== Morphect Statistics Report ===" << std::endl;

        // Group statistics by prefix (pass name)
        std::unordered_map<std::string, std::vector<std::string>> grouped;

        for (const auto& [name, value] : int_stats_) {
            size_t dot = name.find('.');
            std::string prefix = (dot != std::string::npos) ? name.substr(0, dot) : "general";
            grouped[prefix].push_back(name);
        }

        // Sort prefixes
        std::vector<std::string> prefixes;
        for (const auto& [prefix, names] : grouped) {
            prefixes.push_back(prefix);
        }
        std::sort(prefixes.begin(), prefixes.end());

        // Print general stats first
        if (grouped.find("general") != grouped.end() || int_stats_.find("functions_processed") != int_stats_.end()) {
            oss << std::endl << "[General]" << std::endl;

            for (const auto& [name, value] : int_stats_) {
                if (name.find('.') == std::string::npos) {
                    oss << "  " << std::setw(30) << std::left << name
                        << ": " << value << std::endl;
                }
            }
        }

        // Print per-pass stats
        for (const auto& prefix : prefixes) {
            if (prefix == "general") continue;

            oss << std::endl << "[" << prefix << "]" << std::endl;

            for (const auto& name : grouped[prefix]) {
                std::string short_name = name.substr(prefix.length() + 1);
                oss << "  " << std::setw(30) << std::left << short_name
                    << ": " << int_stats_.at(name) << std::endl;
            }
        }

        // Print timing stats
        bool has_timing = false;
        for (const auto& [name, value] : double_stats_) {
            if (name.find("time") != std::string::npos) {
                has_timing = true;
                break;
            }
        }

        if (has_timing) {
            oss << std::endl << "[Timing]" << std::endl;
            for (const auto& [name, value] : double_stats_) {
                if (name.find("time") != std::string::npos) {
                    oss << "  " << std::setw(30) << std::left << name
                        << ": " << std::fixed << std::setprecision(2)
                        << value << " ms" << std::endl;
                }
            }
        }

        oss << std::endl << "Total transformations: " << getTotalTransformations() << std::endl;
        oss << "=====================================" << std::endl;

        return oss.str();
    }

    /**
     * Format as JSON string
     */
    std::string toJson() const {
        std::ostringstream oss;
        oss << "{" << std::endl;

        bool first = true;

        for (const auto& [name, value] : int_stats_) {
            if (!first) oss << "," << std::endl;
            oss << "  \"" << name << "\": " << value;
            first = false;
        }

        for (const auto& [name, value] : double_stats_) {
            if (!first) oss << "," << std::endl;
            oss << "  \"" << name << "\": " << std::fixed << std::setprecision(4) << value;
            first = false;
        }

        for (const auto& [name, value] : string_stats_) {
            if (!first) oss << "," << std::endl;
            oss << "  \"" << name << "\": \"" << value << "\"";
            first = false;
        }

        oss << std::endl << "}" << std::endl;

        return oss.str();
    }

private:
    std::unordered_map<std::string, int> int_stats_;
    std::unordered_map<std::string, double> double_stats_;
    std::unordered_map<std::string, std::string> string_stats_;
};

/**
 * Global statistics singleton for convenience
 */
class GlobalStats {
public:
    static Statistics& get() {
        static Statistics instance;
        return instance;
    }

    // Prevent copying
    GlobalStats(const GlobalStats&) = delete;
    GlobalStats& operator=(const GlobalStats&) = delete;

private:
    GlobalStats() = default;
};

} // namespace morphect

#endif // MORPHECT_STATISTICS_HPP
