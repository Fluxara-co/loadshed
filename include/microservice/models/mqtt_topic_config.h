/**
 * @file mqtt_topic_config.h
 * @brief MQTT topic configuration structure
 */

#pragma once

#include <string>
#include <cstdint>

namespace microservice {
namespace models {

/**
 * @brief MQTT topic configuration
 */
struct MQTTTopicConfig {
    std::string subscribe_topic;     // Topic to subscribe to (receive data from LSP)
    std::string publish_topic;      // Topic to publish to (send commands to LSP)
    uint8_t qos;                    // Quality of Service (0, 1, or 2)
    bool retain;                    // Retain flag
    
    MQTTTopicConfig() 
        : qos(1)
        , retain(false)
    {}
};

} // namespace models
} // namespace microservice
