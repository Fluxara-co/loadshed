#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <string>

namespace microservice {

/**
 * @brief Simple Logger class wrapping spdlog
 * Provides static convenience methods for logging
 */
class Logger {
public:
    /**
     * @brief Initialize the logger
     * @param name Logger name (default: "microservice")
     * @return true if initialization successful
     */
    static bool initialize(const std::string& name = "microservice");
    
    /**
     * @brief Shutdown the logger
     */
    static void shutdown();
    
    /**
     * @brief Check if logger is initialized
     * @return true if logger is ready
     */
    static bool isInitialized();
    
    // Convenience logging methods
    static void trace(const std::string& msg);
    static void debug(const std::string& msg);
    static void info(const std::string& msg);
    static void warn(const std::string& msg);
    static void error(const std::string& msg);
    static void critical(const std::string& msg);
    
    /**
     * @brief Set log level
     * @param level spdlog::level::level_enum
     */
    static void setLevel(spdlog::level::level_enum level);
    
    /**
     * @brief Get the logger instance
     * @return Shared pointer to the logger
     */
    static std::shared_ptr<spdlog::logger> get();

private:
    static std::shared_ptr<spdlog::logger> logger_;
    static bool initialized_;
    
    static void ensureInitialized();
};

} // namespace microservice
