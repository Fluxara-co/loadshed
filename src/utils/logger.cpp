/**
 * Simple Logger Implementation
 */

#include "microservice/utils/logger.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>

namespace microservice {

// Static member initialization
std::shared_ptr<spdlog::logger> Logger::logger_ = nullptr;
bool Logger::initialized_ = false;

bool Logger::initialize(const std::string& name) {
    if (initialized_) {
        return true;
    }
    
    try {
        // Create console sink with colors
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(spdlog::level::debug);
        
        // Create logger
        logger_ = std::make_shared<spdlog::logger>(name, console_sink);
        logger_->set_level(spdlog::level::debug);
        logger_->flush_on(spdlog::level::info);
        
        // Set as default logger
        spdlog::set_default_logger(logger_);
        
        initialized_ = true;
        logger_->info("Logger initialized: {}", name);
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

