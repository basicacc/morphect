/**
 * Morphect - Multi-Language Code Obfuscator
 *
 * logging.hpp - Logging utilities with verbosity levels
 *
 * Features:
 *   - Multiple verbosity levels (ERROR, WARN, INFO, DEBUG, TRACE)
 *   - Configurable output (stderr, file, callback)
 *   - Colored output for terminals
 *   - Format string support (printf-style and {})
 */

#ifndef MORPHECT_LOGGING_HPP
#define MORPHECT_LOGGING_HPP

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <functional>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <memory>
#include <chrono>
#include <iomanip>

namespace morphect {

/**
 * Log levels in order of severity
 */
enum class LogLevel {
    Trace = 0,    // Most verbose - function entry/exit, detailed state
    Debug = 1,    // Debugging info - variable values, decisions made
    Info = 2,     // Normal operation - pass started/completed
    Warn = 3,     // Potential issues - fallback behavior, deprecated usage
    Error = 4,    // Errors - transformation failed, config error
    Silent = 5    // No output at all
};

/**
 * Convert LogLevel to string
 */
inline const char* logLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Silent: return "SILENT";
        default: return "UNKNOWN";
    }
}

/**
 * ANSI color codes for terminal output
 */
namespace colors {
    constexpr const char* Reset   = "\033[0m";
    constexpr const char* Red     = "\033[31m";
    constexpr const char* Green   = "\033[32m";
    constexpr const char* Yellow  = "\033[33m";
    constexpr const char* Blue    = "\033[34m";
    constexpr const char* Magenta = "\033[35m";
    constexpr const char* Cyan    = "\033[36m";
    constexpr const char* White   = "\033[37m";
    constexpr const char* Bold    = "\033[1m";
    constexpr const char* Dim     = "\033[2m";
}

/**
 * Get color for log level
 */
inline const char* logLevelColor(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return colors::Dim;
        case LogLevel::Debug: return colors::Cyan;
        case LogLevel::Info:  return colors::Green;
        case LogLevel::Warn:  return colors::Yellow;
        case LogLevel::Error: return colors::Red;
        default: return colors::Reset;
    }
}

/**
 * Global logging configuration
 */
class LogConfig {
public:
    static LogConfig& get() {
        static LogConfig instance;
        return instance;
    }

    LogLevel minLevel = LogLevel::Info;
    bool useColors = true;
    bool showTimestamp = false;
    bool showLevel = true;
    bool showSource = true;
    std::ostream* output = &std::cerr;
    std::unique_ptr<std::ofstream> fileOutput;
    std::function<void(LogLevel, const std::string&, const std::string&)> callback;

    /**
     * Set minimum log level from LogLevel enum
     */
    void setLevel(LogLevel level) {
        minLevel = level;
    }

    /**
     * Set minimum log level from integer (for CLI parsing)
     * 0=Trace, 1=Debug, 2=Info, 3=Warn, 4=Error, 5=Silent
     */
    void setLevel(int level) {
        if (level < 0) level = 0;
        if (level > 5) level = 5;
        minLevel = static_cast<LogLevel>(level);
    }

    /**
     * Set minimum log level from string
     */
    void setLevel(const std::string& level) {
        if (level == "trace" || level == "TRACE") minLevel = LogLevel::Trace;
        else if (level == "debug" || level == "DEBUG") minLevel = LogLevel::Debug;
        else if (level == "info" || level == "INFO") minLevel = LogLevel::Info;
        else if (level == "warn" || level == "WARN") minLevel = LogLevel::Warn;
        else if (level == "error" || level == "ERROR") minLevel = LogLevel::Error;
        else if (level == "silent" || level == "SILENT") minLevel = LogLevel::Silent;
    }

    /**
     * Set output to file
     */
    bool setOutputFile(const std::string& path) {
        fileOutput = std::make_unique<std::ofstream>(path);
        if (fileOutput->is_open()) {
            output = fileOutput.get();
            useColors = false;  // No colors in file
            return true;
        }
        fileOutput.reset();
        return false;
    }

private:
    LogConfig() = default;
};

/**
 * Logger class - use one per component/pass
 */
class Logger {
public:
    explicit Logger(const std::string& source = "Morphect")
        : source_(source) {}

    /**
     * Log a message with format string (using {} placeholders)
     */
    template<typename... Args>
    void log(LogLevel level, const std::string& format, Args&&... args) {
        auto& config = LogConfig::get();

        if (level < config.minLevel) return;

        std::string message = formatString(format, std::forward<Args>(args)...);
        writeLog(level, message);
    }

    template<typename... Args>
    void trace(const std::string& format, Args&&... args) {
        log(LogLevel::Trace, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void debug(const std::string& format, Args&&... args) {
        log(LogLevel::Debug, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void info(const std::string& format, Args&&... args) {
        log(LogLevel::Info, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void warn(const std::string& format, Args&&... args) {
        log(LogLevel::Warn, format, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(const std::string& format, Args&&... args) {
        log(LogLevel::Error, format, std::forward<Args>(args)...);
    }

    /**
     * Set the source name for this logger
     */
    void setSource(const std::string& source) {
        source_ = source;
    }

private:
    std::string source_;
    static std::mutex mutex_;

    /**
     * Format a string with {} placeholders
     */
    template<typename T>
    static std::string toString(const T& value) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }

    static std::string formatString(const std::string& format) {
        return format;
    }

    template<typename T, typename... Rest>
    static std::string formatString(const std::string& format, T&& value, Rest&&... rest) {
        std::string result;
        size_t pos = 0;
        size_t placeholder = format.find("{}", pos);

        if (placeholder != std::string::npos) {
            result = format.substr(0, placeholder);
            result += toString(std::forward<T>(value));
            result += formatString(format.substr(placeholder + 2), std::forward<Rest>(rest)...);
        } else {
            result = format;
        }

        return result;
    }

    /**
     * Write the formatted log message
     */
    void writeLog(LogLevel level, const std::string& message) {
        auto& config = LogConfig::get();

        std::ostringstream oss;

        // Timestamp
        if (config.showTimestamp) {
            auto now = std::chrono::system_clock::now();
            auto time = std::chrono::system_clock::to_time_t(now);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) % 1000;

            oss << std::put_time(std::localtime(&time), "%H:%M:%S");
            oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << " ";
        }

        // Level with color
        if (config.showLevel) {
            if (config.useColors) {
                oss << logLevelColor(level);
            }
            oss << "[" << std::setw(5) << logLevelToString(level) << "]";
            if (config.useColors) {
                oss << colors::Reset;
            }
            oss << " ";
        }

        // Source
        if (config.showSource && !source_.empty()) {
            if (config.useColors) {
                oss << colors::Bold;
            }
            oss << "[" << source_ << "]";
            if (config.useColors) {
                oss << colors::Reset;
            }
            oss << " ";
        }

        // Message
        oss << message;

        // Thread-safe output
        {
            std::lock_guard<std::mutex> lock(mutex_);
            *config.output << oss.str() << std::endl;

            // Also call callback if set
            if (config.callback) {
                config.callback(level, source_, message);
            }
        }
    }
};

// Static mutex definition (in header for simplicity)
inline std::mutex Logger::mutex_;

/**
 * Convenience macros for logging with source file info
 */
#define MORPHECT_LOG(logger, level, ...) \
    logger.log(level, __VA_ARGS__)

#define MORPHECT_TRACE(logger, ...) logger.trace(__VA_ARGS__)
#define MORPHECT_DEBUG(logger, ...) logger.debug(__VA_ARGS__)
#define MORPHECT_INFO(logger, ...)  logger.info(__VA_ARGS__)
#define MORPHECT_WARN(logger, ...)  logger.warn(__VA_ARGS__)
#define MORPHECT_ERROR(logger, ...) logger.error(__VA_ARGS__)

/**
 * Global logger instance for quick access
 */
inline Logger& globalLogger() {
    static Logger instance("Morphect");
    return instance;
}

// Shorthand macros using global logger
#define LOG_TRACE(...) morphect::globalLogger().trace(__VA_ARGS__)
#define LOG_DEBUG(...) morphect::globalLogger().debug(__VA_ARGS__)
#define LOG_INFO(...)  morphect::globalLogger().info(__VA_ARGS__)
#define LOG_WARN(...)  morphect::globalLogger().warn(__VA_ARGS__)
#define LOG_ERROR(...) morphect::globalLogger().error(__VA_ARGS__)

} // namespace morphect

#endif // MORPHECT_LOGGING_HPP
