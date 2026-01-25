#pragma once

#include <mqtt/async_client.h>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <map>

namespace microservice {

/**
 * @brief Generic MQTT Subscriber Service
 * 
 * Handles subscribing to MQTT topics with:
 * - Multiple topic subscriptions
 * - Wildcard support
 * - Message callbacks
 * - QoS support
 * - Subscription statistics
 */
class MQTTSubscriber {
public:
    MQTTSubscriber();
    ~MQTTSubscriber();
    
    // Copy operations disabled
    MQTTSubscriber(const MQTTSubscriber&) = delete;
    MQTTSubscriber& operator=(const MQTTSubscriber&) = delete;
    
    // Move operations disabled
    MQTTSubscriber(MQTTSubscriber&&) = delete;
    MQTTSubscriber& operator=(MQTTSubscriber&&) = delete;
    
    // Service lifecycle
    bool initialize(const std::string& broker_host, 
                   int broker_port, 
                   const std::string& client_id);
    bool connect();
    void disconnect();
    bool isConnected() const;
    
    // Message callback type
    using MessageCallback = std::function<void(const std::string& topic, const std::string& payload)>;
    
    // Subscription management
    bool subscribe(const std::string& topic, int qos = 1);
    bool subscribe(const std::string& topic, MessageCallback callback, int qos = 1);
    bool subscribe(const std::vector<std::string>& topics, int qos = 1);
    bool unsubscribe(const std::string& topic);
    bool unsubscribe(const std::vector<std::string>& topics);
    void unsubscribeAll();
    
    // Callback registration
    void setDefaultMessageCallback(MessageCallback callback);
    void setTopicMessageCallback(const std::string& topic, MessageCallback callback);
    void removeTopicMessageCallback(const std::string& topic);
    
    // Connection callbacks
    using ConnectionCallback = std::function<void(bool connected, const std::string& reason)>;
    void setConnectionCallback(ConnectionCallback callback);
    
    // Configuration
    void setUsername(const std::string& username);
    void setPassword(const std::string& password);
    void setKeepAliveInterval(int seconds);
    void setCleanSession(bool clean);
    
    // Statistics
    struct SubscriberStats {
        uint64_t total_messages_received{0};
        uint64_t total_bytes_received{0};
        uint32_t active_subscriptions{0};
        std::chrono::system_clock::time_point last_message_time;
        std::map<std::string, uint64_t> topic_message_counts;
    };
    
    SubscriberStats getStats() const;
    void resetStats();
    
    // Status
    std::string getBrokerHost() const;
    int getBrokerPort() const;
    std::string getClientId() const;
    bool isInitialized() const;
    std::vector<std::string> getSubscribedTopics() const;
    bool isSubscribed(const std::string& topic) const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    
    // Internal methods
    void handleMessageArrived(const std::string& topic, const std::string& payload);
    void handleConnectionLost(const std::string& reason);
    void handleSubscriptionSuccess(const std::string& topic);
    void handleSubscriptionFailure(const std::string& topic, const std::string& error);
};

} // namespace microservice
