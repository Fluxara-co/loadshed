#include "microservice/utils/config_manager.h"
#include "microservice/utils/logger.h"
#include <fstream>
#include <filesystem>

namespace microservice {

struct ConfigManager::Impl {
    std::string config_file;
    std::string state_file;
    json config;
    json state;
    mutable std::mutex config_mutex;
    mutable std::mutex state_mutex;
    bool initialized = false;
    
    bool loadJsonFile(const std::string& file_path, json& data) {
        try {
            std::ifstream file(file_path);
            if (!file.is_open()) {
                Logger::error("Failed to open file: " + file_path);
                return false;
            }
            
            file >> data;
            return true;
            
        } catch (const std::exception& e) {
            Logger::error("Error loading JSON from " + file_path + ": " + e.what());
            return false;
        }
    }
    
    bool saveJsonFile(const std::string& file_path, const json& data) {
        try {
            // Create directory if it doesn't exist
            std::filesystem::path path(file_path);
            std::filesystem::path dir = path.parent_path();
            
            if (!dir.empty() && !std::filesystem::exists(dir)) {
                std::filesystem::create_directories(dir);
            }
            
            std::ofstream file(file_path);
            if (!file.is_open()) {
                Logger::error("Failed to open file for writing: " + file_path);
                return false;
            }
            
            file << data.dump(2);  // Pretty print with 2-space indent
            return true;
            
        } catch (const std::exception& e) {
            Logger::error("Error saving JSON to " + file_path + ": " + e.what());
            return false;
        }
    }
};

ConfigManager::ConfigManager() : pImpl(std::make_unique<Impl>()) {}

ConfigManager::~ConfigManager() = default;

bool ConfigManager::initialize(const std::string& config_file) {
    Logger::info("Initializing Configuration Manager...");
    Logger::info("  Config file: " + config_file);
    
    pImpl->config_file = config_file;
    
    // Derive state file path from config file
    std::filesystem::path config_path(config_file);
    std::string state_filename = config_path.stem().string() + "_state.json";
    pImpl->state_file = config_path.parent_path() / state_filename;
    
    Logger::info("  State file: " + pImpl->state_file);
    
    // Load configuration
    if (!pImpl->loadJsonFile(config_file, pImpl->config)) {
        Logger::error("Failed to load configuration");
        return false;
    }
    
    Logger::info("Configuration loaded successfully");
    Logger::info("  Service: " + get<std::string>("service.name", "unknown"));
    Logger::info("  Version: " + get<std::string>("service.version", "unknown"));
    
    // Load state if exists
    if (std::filesystem::exists(pImpl->state_file)) {
        Logger::info("Loading previous state...");
        if (loadState()) {
            Logger::info("State loaded successfully");
        }
    } else {
        Logger::info("No previous state found, starting fresh");
        pImpl->state = json::object();
    }
    
    pImpl->initialized = true;
    return true;
}

bool ConfigManager::reload() {
    Logger::info("Reloading configuration from: " + pImpl->config_file);
    
    json new_config;
    if (!pImpl->loadJsonFile(pImpl->config_file, new_config)) {
        Logger::error("Failed to reload configuration");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(pImpl->config_mutex);
    pImpl->config = new_config;
    
    Logger::info("Configuration reloaded successfully");
    return true;
}

bool ConfigManager::save() {
    std::lock_guard<std::mutex> lock(pImpl->config_mutex);
    
    Logger::info("Saving configuration to: " + pImpl->config_file);
    
    if (!pImpl->saveJsonFile(pImpl->config_file, pImpl->config)) {
        Logger::error("Failed to save configuration");
        return false;
    }
    
    Logger::info("Configuration saved successfully");
    return true;
}

bool ConfigManager::saveAs(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(pImpl->config_mutex);
    
    Logger::info("Saving configuration to: " + file_path);
    
    if (!pImpl->saveJsonFile(file_path, pImpl->config)) {
        Logger::error("Failed to save configuration");
        return false;
    }
    
    Logger::info("Configuration saved successfully");
    return true;
}

json ConfigManager::getConfig() const {
    std::lock_guard<std::mutex> lock(pImpl->config_mutex);
    return pImpl->config;
}

json ConfigManager::getSection(const std::string& section) const {
    std::lock_guard<std::mutex> lock(pImpl->config_mutex);
    
    if (!pImpl->config.contains(section)) {
        return json::object();
    }
    
    return pImpl->config[section];
}

bool ConfigManager::setConfig(const json& config) {
    std::lock_guard<std::mutex> lock(pImpl->config_mutex);
    pImpl->config = config;
    return true;
}

bool ConfigManager::updateSection(const std::string& section, const json& data) {
    std::lock_guard<std::mutex> lock(pImpl->config_mutex);
    pImpl->config[section] = data;
    return true;
}

bool ConfigManager::loadState() {
    if (!std::filesystem::exists(pImpl->state_file)) {
        Logger::warn("State file does not exist: " + pImpl->state_file);
        return false;
    }
    
    std::lock_guard<std::mutex> lock(pImpl->state_mutex);
    
    if (!pImpl->loadJsonFile(pImpl->state_file, pImpl->state)) {
        Logger::error("Failed to load state");
        return false;
    }
    
    return true;
}

bool ConfigManager::saveState() {
    std::lock_guard<std::mutex> lock(pImpl->state_mutex);
    
    Logger::debug("Saving state to: " + pImpl->state_file);
    
    if (!pImpl->saveJsonFile(pImpl->state_file, pImpl->state)) {
        Logger::error("Failed to save state");
        return false;
    }
    
    return true;
}

json ConfigManager::getState() const {
    std::lock_guard<std::mutex> lock(pImpl->state_mutex);
    return pImpl->state;
}

bool ConfigManager::setState(const json& state) {
    std::lock_guard<std::mutex> lock(pImpl->state_mutex);
    pImpl->state = state;
    return true;
}

std::string ConfigManager::getConfigFile() const {
    return pImpl->config_file;
}

std::string ConfigManager::getStateFile() const {
    return pImpl->state_file;
}

bool ConfigManager::isInitialized() const {
    return pImpl->initialized;
}

} // namespace microservice

