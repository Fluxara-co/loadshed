/**
 * Simple Logger Implementation with File Rotation
 */

#include "microservice/utils/logger.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <algorithm>
#include <cctype>

namespace microservice {

// Static member initialization
std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;
bool Logger::initialized_ = false;

// Helper function to parse log level from environment variable
static spdlog::level::level_enum getLogLevelFromEnv() {
    const char* log_level_env = std::getenv("LOG_LEVEL");
    if (!log_level_env) {
        return spdlog::level::info;  // Default to INFO
    }
    
    std::string level_str = log_level_env;
    // Convert to lowercase for case-insensitive comparison
    std::transform(level_str.begin(), level_str.end(), level_str.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    // Map string to spdlog level
    if (level_str == "trace") return spdlog::level::trace;
    if (level_str == "debug") return spdlog::level::debug;
    if (level_str == "info") return spdlog::level::info;
    if (level_str == "warn" || level_str == "warning") return spdlog::level::warn;
    if (level_str == "error") return spdlog::level::err;
    if (level_str == "critical") return spdlog::level::critical;
    if (level_str == "off") return spdlog::level::off;
    
    // Default to INFO if invalid value
    return spdlog::level::info;
}

bool Logger::initialize(const std::string& name) {
    if (initialized_) {
        return true;
    }
    
    try {
        // Get log level from environment variable
        spdlog::level::level_enum log_level = getLogLevelFromEnv();
        
        // Get log directory from environment or use default
        std::string log_dir = std::getenv("LOG_DIR") ? std::getenv("LOG_DIR") : "/app/logs";
        
        // Create log directory if it doesn't exist
        std::filesystem::path log_path(log_dir);
        if (!std::filesystem::exists(log_path)) {
            std::filesystem::create_directories(log_path);
        }
        
        // Create console sink with colors
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(log_level);
        
        // Get rotation settings from environment or use defaults
        int max_file_size_mb = 10;  // Default 10 MB per file
        int max_files = 5;          // Default 5 files (50 MB total)
        
        const char* max_size_env = std::getenv("LOG_MAX_SIZE_MB");
        if (max_size_env) {
            max_file_size_mb = std::atoi(max_size_env);
            if (max_file_size_mb <= 0) max_file_size_mb = 10;
        }
        
        const char* max_files_env = std::getenv("LOG_MAX_FILES");
        if (max_files_env) {
            max_files = std::atoi(max_files_env);
            if (max_files <= 0) max_files = 5;
        }
        
        // Create rotating file sink
        std::string log_file = log_dir + "/" + name + ".log";
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            log_file,
            1024 * 1024 * max_file_size_mb,  // Max file size in bytes
            max_files                         // Number of files to keep
        );
        file_sink->set_level(log_level);
        
        // Combine sinks
        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        
        // Create logger with both sinks
        logger_ = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
        logger_->set_level(log_level);
        logger_->flush_on(spdlog::level::info);
        
        // Set as default logger
        spdlog::set_default_logger(logger_);
        
        initialized_ = true;
        logger_->info("Logger initialized: {} (log file: {}, max_size: {}MB, max_files: {})", 
                     name, log_file, max_file_size_mb, max_files);
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Logger initialization failed: " << e.what() << std::endl;
        return false;
    }
}

void Logger::shutdown() {
    if (logger_) {
        logger_->flush();
        spdlog::shutdown();
    }
    initialized_ = false;
}

bool Logger::isInitialized() {
    return initialized_;
}

void Logger::ensureInitialized() {
    if (!initialized_) {
        initialize();
    }
}

// Convenience logging methods
void Logger::trace(const std::string& msg) {
    ensureInitialized();
    if (logger_) logger_->trace(msg);
}

void Logger::debug(const std::string& msg) {
    ensureInitialized();
    if (logger_) logger_->debug(msg);
}

void Logger::info(const std::string& msg) {
    ensureInitialized();
    if (logger_) logger_->info(msg);
}

void Logger::warn(const std::string& msg) {
    ensureInitialized();
    if (logger_) logger_->warn(msg);
}

void Logger::error(const std::string& msg) {
    ensureInitialized();
    if (logger_) logger_->error(msg);
}

void Logger::critical(const std::string& msg) {
    ensureInitialized();
    if (logger_) logger_->critical(msg);
}

void Logger::setLevel(spdlog::level::level_enum level) {
    if (logger_) {
        logger_->set_level(level);
    }
}

std::shared_ptr<spdlog::logger> Logger::get() {
    ensureInitialized();
    return logger_;
}

} // namespace microservice

