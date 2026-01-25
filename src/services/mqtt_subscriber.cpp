/**
 * Generic MQTT Subscriber Implementation
 */

#include "microservice/services/mqtt_subscriber.h"
#include "microservice/utils/logger.h"
#include <mqtt/async_client.h>
#include <mqtt/message.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <map>
#include <vector>
#include <algorithm>

using json = nlohmann::json;

namespace microservice {

class MQTTSubscriber::Impl {
public:
    Impl() = default;
    ~Impl() {
        if (client_) {
            try {
                client_->disconnect()->wait();
            } catch (...) {}
        }
    }
    
    // MQTT Client
    std::unique_ptr<mqtt::async_client> client_;
    
    // Configuration
    std::string broker_host_;
    int broker_port_{1883};
    std::string client_id_;
    std::string username_;
    std::string password_;
    int keepalive_{60};
    bool clean_session_{true};
    
    // State
    bool initialized_{false};
    bool connected_{false};
    
    // Subscriptions
    std::vector<std::string> subscribed_topics_;
    std::map<std::string, MessageCallback> topic_callbacks_;
    MessageCallback default_callback_;
    
    // Connection callback
    ConnectionCallback connection_callback_;
    
    // Statistics
    SubscriberStats stats_;
};

// Constructor/Destructor
MQTTSubscriber::MQTTSubscriber() : pImpl(std::make_unique<Impl>()) {}
MQTTSubscriber::~MQTTSubscriber() = default;

// Service lifecycle
bool MQTTSubscriber::initialize(const std::string& broker_host, int broker_port, const std::string& client_id) {
    try {
        pImpl->broker_host_ = broker_host;
        pImpl->broker_port_ = broker_port;
        pImpl->client_id_ = client_id;
        
        std::string server_uri = "tcp://" + broker_host + ":" + std::to_string(broker_port);
        pImpl->client_ = std::make_unique<mqtt::async_client>(server_uri, client_id);
        
        // Set message callback
        pImpl->client_->set_message_callback([this](mqtt::const_message_ptr msg) {
            handleMessageArrived(msg->get_topic(), msg->to_string());
        });
        
        // Set connection lost callback
        pImpl->client_->set_connection_lost_handler([this](const std::string& cause) {
            handleConnectionLost(cause);
        });
        
        pImpl->initialized_ = true;
        Logger::info("MQTT Subscriber initialized: " + server_uri);
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("MQTT Subscriber initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool MQTTSubscriber::connect() {
    if (!pImpl->initialized_) {
        Logger::error("MQTT Subscriber not initialized");
        return false;
    }
    
    try {
        auto connOpts = mqtt::connect_options_builder()
            .keep_alive_interval(std::chrono::seconds(pImpl->keepalive_))
            .clean_session(pImpl->clean_session_)
            .automatic_reconnect(true);
        
        if (!pImpl->username_.empty()) {
            connOpts.user_name(pImpl->username_);
            connOpts.password(pImpl->password_);
        }
        
        auto tok = pImpl->client_->connect(connOpts.finalize());
        tok->wait();
        
        pImpl->connected_ = true;
        Logger::info("MQTT Subscriber connected");
        
        // Resubscribe to all topics
        for (const auto& topic : pImpl->subscribed_topics_) {
            pImpl->client_->subscribe(topic, 1)->wait();
        }
        
        if (pImpl->connection_callback_) {
            pImpl->connection_callback_(true, "Connected successfully");
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("MQTT Subscriber connection failed: " + std::string(e.what()));
        pImpl->connected_ = false;
        
        if (pImpl->connection_callback_) {
            pImpl->connection_callback_(false, e.what());
        }
        
        return false;
    }
}

void MQTTSubscriber::disconnect() {
    if (!pImpl->connected_) return;
    
    try {
        pImpl->client_->disconnect()->wait();
        pImpl->connected_ = false;
        Logger::info("MQTT Subscriber disconnected");
        
        if (pImpl->connection_callback_) {
            pImpl->connection_callback_(false, "Disconnected");
        }
        
    } catch (const std::exception& e) {
        Logger::error("MQTT Subscriber disconnect error: " + std::string(e.what()));
    }
}

bool MQTTSubscriber::isConnected() const {
    return pImpl->connected_ && pImpl->client_ && pImpl->client_->is_connected();
}

// Subscription management
bool MQTTSubscriber::subscribe(const std::string& topic, int qos) {
    if (!isConnected()) {
        Logger::error("MQTT Subscriber not connected");
        return false;
    }
    
    try {
        pImpl->client_->subscribe(topic, qos)->wait();
        
        if (std::find(pImpl->subscribed_topics_.begin(), pImpl->subscribed_topics_.end(), topic) == pImpl->subscribed_topics_.end()) {
            pImpl->subscribed_topics_.push_back(topic);
        }
        
        pImpl->stats_.active_subscriptions = pImpl->subscribed_topics_.size();
        
        Logger::info("Subscribed to: " + topic);
        handleSubscriptionSuccess(topic);
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("MQTT Subscribe failed for " + topic + ": " + std::string(e.what()));
        handleSubscriptionFailure(topic, e.what());
        return false;
    }
}

bool MQTTSubscriber::subscribe(const std::string& topic, MessageCallback callback, int qos) {
    pImpl->topic_callbacks_[topic] = callback;
    return subscribe(topic, qos);
}

bool MQTTSubscriber::subscribe(const std::vector<std::string>& topics, int qos) {
    bool all_success = true;
    
    for (const auto& topic : topics) {
        if (!subscribe(topic, qos)) {
            all_success = false;
        }
    }
    
    return all_success;
}

bool MQTTSubscriber::unsubscribe(const std::string& topic) {
    if (!isConnected()) {
        return false;
    }
    
    try {
        pImpl->client_->unsubscribe(topic)->wait();
        
        auto it = std::find(pImpl->subscribed_topics_.begin(), pImpl->subscribed_topics_.end(), topic);
        if (it != pImpl->subscribed_topics_.end()) {
            pImpl->subscribed_topics_.erase(it);
        }
        
        pImpl->topic_callbacks_.erase(topic);
        pImpl->stats_.active_subscriptions = pImpl->subscribed_topics_.size();
        
        Logger::info("Unsubscribed from: " + topic);
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("MQTT Unsubscribe failed for " + topic + ": " + std::string(e.what()));
        return false;
    }
}

bool MQTTSubscriber::unsubscribe(const std::vector<std::string>& topics) {
    bool all_success = true;
    
    for (const auto& topic : topics) {
        if (!unsubscribe(topic)) {
            all_success = false;
        }
    }
    
    return all_success;
}

void MQTTSubscriber::unsubscribeAll() {
    auto topics = pImpl->subscribed_topics_;  // Copy to avoid iterator invalidation
    for (const auto& topic : topics) {
        unsubscribe(topic);
    }
}

// Callback registration
void MQTTSubscriber::setDefaultMessageCallback(MessageCallback callback) {
    pImpl->default_callback_ = callback;
}

void MQTTSubscriber::setTopicMessageCallback(const std::string& topic, MessageCallback callback) {
    pImpl->topic_callbacks_[topic] = callback;
}

void MQTTSubscriber::removeTopicMessageCallback(const std::string& topic) {
    pImpl->topic_callbacks_.erase(topic);
}

void MQTTSubscriber::setConnectionCallback(ConnectionCallback callback) {
    pImpl->connection_callback_ = callback;
}

// Configuration
void MQTTSubscriber::setUsername(const std::string& username) {
    pImpl->username_ = username;
}

void MQTTSubscriber::setPassword(const std::string& password) {
    pImpl->password_ = password;
}

void MQTTSubscriber::setKeepAliveInterval(int seconds) {
    pImpl->keepalive_ = seconds;
}

void MQTTSubscriber::setCleanSession(bool clean) {
    pImpl->clean_session_ = clean;
}

// Statistics
MQTTSubscriber::SubscriberStats MQTTSubscriber::getStats() const {
    return pImpl->stats_;
}

void MQTTSubscriber::resetStats() {
    pImpl->stats_ = SubscriberStats{};
    pImpl->stats_.active_subscriptions = pImpl->subscribed_topics_.size();
}

// Status
std::string MQTTSubscriber::getBrokerHost() const {
    return pImpl->broker_host_;
}

int MQTTSubscriber::getBrokerPort() const {
    return pImpl->broker_port_;
}

std::string MQTTSubscriber::getClientId() const {
    return pImpl->client_id_;
}

bool MQTTSubscriber::isInitialized() const {
    return pImpl->initialized_;
}

std::vector<std::string> MQTTSubscriber::getSubscribedTopics() const {
    return pImpl->subscribed_topics_;
}

bool MQTTSubscriber::isSubscribed(const std::string& topic) const {
    return std::find(pImpl->subscribed_topics_.begin(), pImpl->subscribed_topics_.end(), topic) 
           != pImpl->subscribed_topics_.end();
}

// Internal handlers
void MQTTSubscriber::handleMessageArrived(const std::string& topic, const std::string& payload) {
    pImpl->stats_.total_messages_received++;
    pImpl->stats_.total_bytes_received += payload.size();
    pImpl->stats_.last_message_time = std::chrono::system_clock::now();
    pImpl->stats_.topic_message_counts[topic]++;
    
    Logger::debug("MQTT Message received on " + topic);
    
    // Call topic-specific callback if registered
    auto it = pImpl->topic_callbacks_.find(topic);
    if (it != pImpl->topic_callbacks_.end()) {
        it->second(topic, payload);
        return;
    }
    
    // Call default callback if set
    if (pImpl->default_callback_) {
        pImpl->default_callback_(topic, payload);
    }
}

void MQTTSubscriber::handleConnectionLost(const std::string& reason) {
    pImpl->connected_ = false;
    Logger::warn("MQTT Subscriber connection lost: " + reason);
    
    if (pImpl->connection_callback_) {
        pImpl->connection_callback_(false, reason);
    }
}

void MQTTSubscriber::handleSubscriptionSuccess(const std::string& topic) {
    Logger::debug("MQTT Subscription success: " + topic);
}

void MQTTSubscriber::handleSubscriptionFailure(const std::string& topic, const std::string& error) {
    Logger::error("MQTT Subscription failure for " + topic + ": " + error);
}

} // namespace microservice

