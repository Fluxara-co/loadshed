#pragma once

#include <string>
#include <mutex>
#include <memory>
#include <nlohmann/json.hpp>

namespace microservice {

using json = nlohmann::json;

/**
 * @brief Configuration Manager for loading, saving, and managing service configuration
 * 
 * Features:
 * - Load configuration from JSON file
 * - Save configuration changes to disk
 * - Hot-reload configuration
 * - State persistence (metrics, runtime state)
 * - Thread-safe operations
 */
class ConfigManager {
public:
    ConfigManager();
    ~ConfigManager();
    
    // Lifecycle
    bool initialize(const std::string& config_file);
    bool reload();
    bool save();
    bool saveAs(const std::string& file_path);
    
    // Configuration access
    json getConfig() const;
    json getSection(const std::string& section) const;
    bool setConfig(const json& config);
    bool updateSection(const std::string& section, const json& data);
    
    // Get specific values
    template<typename T>
    T get(const std::string& key, const T& default_value = T()) const;
    
    // Set specific values
    template<typename T>
    bool set(const std::string& key, const T& value);
    
    // State persistence
    bool loadState();
    bool saveState();
    json getState() const;
    bool setState(const json& state);
    
    // Getters
    std::string getConfigFile() const;
    std::string getStateFile() const;
    bool isInitialized() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

// Template implementations
template<typename T>
T ConfigManager::get(const std::string& key, const T& default_value) const {
    try {
        json config = getConfig();
        
        // Support nested keys with dot notation (e.g., "mqtt.broker_host")
        std::string key_copy = key;
        size_t pos = 0;
        json current = config;
        
        while ((pos = key_copy.find('.')) != std::string::npos) {
            std::string section = key_copy.substr(0, pos);
            if (!current.contains(section)) {
                return default_value;
            }
            current = current[section];
            key_copy.erase(0, pos + 1);
        }
        
        if (!current.contains(key_copy)) {
            return default_value;
        }
        
        return current[key_copy].get<T>();
        
    } catch (...) {
        return default_value;
    }
}

template<typename T>
bool ConfigManager::set(const std::string& key, const T& value) {
    try {
        json config = getConfig();
        
        // Support nested keys with dot notation
        std::string key_copy = key;
        size_t pos = 0;
        json* current = &config;
        
        while ((pos = key_copy.find('.')) != std::string::npos) {
            std::string section = key_copy.substr(0, pos);
            if (!current->contains(section)) {
                (*current)[section] = json::object();
            }
            current = &(*current)[section];
            key_copy.erase(0, pos + 1);
        }
        
        (*current)[key_copy] = value;
        
        return setConfig(config);
        
    } catch (...) {
        return false;
    }
}

} // namespace microservice

