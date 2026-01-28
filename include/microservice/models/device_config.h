/**
 * @file device_config.h
 * @brief Device configuration for external LSP controllers
 * 
 * Configuration for MQTT-based communication with external load shedding controllers
 */

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include "mqtt_topic_config.h"

namespace microservice {
namespace models {

/**
 * @brief External LSP controller device configuration
 */
struct DeviceConfig {
    std::string device_id;           // Unique device identifier
    std::string device_name;        // Human-readable device name
    std::string device_type;         // Device type (e.g., "LSP", "RTAC", "PLC")
    
    // MQTT configuration
    MQTTTopicConfig trip_outputs;   // Load trip outputs topic (subscribe)
    MQTTTopicConfig contingency;    // Contingency detection topic (subscribe)
    MQTTTopicConfig status;         // Device status topic (subscribe)
    MQTTTopicConfig commands;       // Commands to device topic (publish)
    
    // Device properties
    bool enabled;                   // Device enabled/disabled
    bool redundant;                 // Is this a redundant controller?
    std::string redundancy_group;    // Redundancy group identifier
    
    // Communication status
    bool connected;                 // Currently connected
    std::chrono::system_clock::time_point last_seen;
    
    DeviceConfig() 
        : enabled(true)
        , redundant(false)
        , connected(false)
        , last_seen(std::chrono::system_clock::now())
    {}
};

/**
 * @brief Device configuration manager
 */
class DeviceConfigManager {
public:
    /**
     * @brief Load device configurations from JSON
     */
    bool loadFromJson(const std::string& json_path);
    
    /**
     * @brief Get device configuration by ID
     */
    DeviceConfig* getDevice(const std::string& device_id);
    
    /**
     * @brief Get all enabled devices
     */
    std::vector<DeviceConfig*> getEnabledDevices();
    
    /**
     * @brief Get devices in redundancy group
     */
    std::vector<DeviceConfig*> getRedundancyGroup(const std::string& group_id);
    
private:
    std::vector<DeviceConfig> devices_;
};

} // namespace models
} // namespace microservice
