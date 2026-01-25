#pragma once

#include <mqtt/async_client.h>
#include <memory>
#include <string>
#include <functional>
#include <map>
#include <chrono>

namespace microservice {

/**
 * @brief Generic MQTT Publisher Service
 * 
 * Handles publishing messages to MQTT topics with:
 * - QoS support
 * - Retained messages
 * - Connection management
 * - Publish statistics
 * - Callbacks for success/failure
 */
class MQTTPublisher {
public:
    MQTTPublisher();
    ~MQTTPublisher();
    
    // Copy operations disabled
    MQTTPublisher(const MQTTPublisher&) = delete;
    MQTTPublisher& operator=(const MQTTPublisher&) = delete;
    
    // Move operations disabled
    MQTTPublisher(MQTTPublisher&&) = delete;
    MQTTPublisher& operator=(MQTTPublisher&&) = delete;
    
    // Service lifecycle
    bool initialize(const std::string& broker_host, 
                   int broker_port, 
                   const std::string& client_id);
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    // Basic message publishing
    bool publish(const std::string& topic, 
                const std::string& payload, 
                int qos = 1,
                bool retained = false);
    
    // Convenience methods for JSON publishing
    bool publishJson(const std::string& topic,
                    const std::string& json_payload,
                    int qos = 1,
                    bool retained = false);
    
    // Batch publishing
    bool publishBatch(const std::map<std::string, std::string>& messages,
                     int qos = 1);
    
    // Configuration
    void setUsername(const std::string& username);
    void setPassword(const std::string& password);
    void setKeepAliveInterval(int seconds);
    void setCleanSession(bool clean);
    void setWillMessage(const std::string& topic, 
                       const std::string& message,
                       int qos = 1);
    
    // Callbacks
    using PublishCallback = std::function<void(const std::string& topic, bool success)>;
    using ConnectionCallback = std::function<void(bool connected, const std::string& reason)>;
    
    void setPublishCallback(PublishCallback callback);
    void setConnectionCallback(ConnectionCallback callback);
    
    // Statistics
    struct PublishStats {
        uint64_t total_published{0};
        uint64_t successful_publishes{0};
        uint64_t failed_publishes{0};
        uint64_t retries{0};
        std::chrono::system_clock::time_point last_publish_time;
        std::map<std::string, uint64_t> topic_counts;
    };
    
    PublishStats getStats() const;
    void resetStats();
    
    // Status
    std::string getBrokerHost() const;
    int getBrokerPort() const;
    std::string getClientId() const;
    bool isInitialized() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    
    // Internal methods
    void handleConnectionLost(const std::string& reason);
    void handlePublishSuccess(const std::string& topic);
    void handlePublishFailure(const std::string& topic, const std::string& error);
};

} // namespace microservice
