/**
 * Generic MQTT Publisher Implementation
 */

#include "microservice/services/mqtt_publisher.h"
#include "microservice/utils/logger.h"
#include <mqtt/async_client.h>
#include <mqtt/message.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <chrono>

using json = nlohmann::json;

namespace microservice {

class MQTTPublisher::Impl {
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
    
    // Will message
    bool has_will_{false};
    std::string will_topic_;
    std::string will_message_;
    int will_qos_{1};
    
    // Statistics
    PublishStats stats_;
    
    // Callbacks
    PublishCallback publish_callback_;
    ConnectionCallback connection_callback_;
};

// Constructor/Destructor
MQTTPublisher::MQTTPublisher() : pImpl(std::make_unique<Impl>()) {}
MQTTPublisher::~MQTTPublisher() = default;

// Service lifecycle
bool MQTTPublisher::initialize(const std::string& broker_host, int broker_port, const std::string& client_id) {
    try {
        pImpl->broker_host_ = broker_host;
        pImpl->broker_port_ = broker_port;
        pImpl->client_id_ = client_id;
        
        std::string server_uri = "tcp://" + broker_host + ":" + std::to_string(broker_port);
        pImpl->client_ = std::make_unique<mqtt::async_client>(server_uri, client_id);
        
        pImpl->initialized_ = true;
        Logger::info("MQTT Publisher initialized: " + server_uri);
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("MQTT Publisher initialization failed: " + std::string(e.what()));
        return false;
    }
}

bool MQTTPublisher::connect() {
    if (!pImpl->initialized_) {
        Logger::error("MQTT Publisher not initialized");
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
        
        if (pImpl->has_will_) {
            mqtt::message will_msg(pImpl->will_topic_, pImpl->will_message_, pImpl->will_qos_, false);
            connOpts.will(will_msg);
        }
        
        auto tok = pImpl->client_->connect(connOpts.finalize());
        tok->wait();
        
        pImpl->connected_ = true;
        Logger::info("MQTT Publisher connected");
        
        if (pImpl->connection_callback_) {
            pImpl->connection_callback_(true, "Connected successfully");
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("MQTT Publisher connection failed: " + std::string(e.what()));
        pImpl->connected_ = false;
        
        if (pImpl->connection_callback_) {
            pImpl->connection_callback_(false, e.what());
        }
        
        return false;
    }
}

void MQTTPublisher::disconnect() {
    if (!pImpl->connected_) return;
    
    try {
        pImpl->client_->disconnect()->wait();
        pImpl->connected_ = false;
        Logger::info("MQTT Publisher disconnected");
        
        if (pImpl->connection_callback_) {
            pImpl->connection_callback_(false, "Disconnected");
        }
        
    } catch (const std::exception& e) {
        Logger::error("MQTT Publisher disconnect error: " + std::string(e.what()));
    }
}

bool MQTTPublisher::isConnected() const {
    return pImpl->connected_ && pImpl->client_ && pImpl->client_->is_connected();
}

// Publishing
bool MQTTPublisher::publish(const std::string& topic, const std::string& payload, int qos, bool retained) {
    if (!isConnected()) {
        Logger::error("MQTT Publisher not connected");
        pImpl->stats_.failed_publishes++;
        return false;
    }
    
    try {
        auto msg = mqtt::message::create(topic, payload, qos, retained);
        pImpl->client_->publish(msg)->wait();
        
        pImpl->stats_.total_published++;
        pImpl->stats_.successful_publishes++;
        pImpl->stats_.last_publish_time = std::chrono::system_clock::now();
        pImpl->stats_.topic_counts[topic]++;
        
        if (pImpl->publish_callback_) {
            pImpl->publish_callback_(topic, true);
        }
        
        Logger::debug("Published to " + topic);
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("MQTT Publish failed for " + topic + ": " + std::string(e.what()));
        pImpl->stats_.failed_publishes++;
        
        if (pImpl->publish_callback_) {
            pImpl->publish_callback_(topic, false);
        }
        
        return false;
    }
}

bool MQTTPublisher::publishJson(const std::string& topic, const std::string& json_payload, int qos, bool retained) {
    return publish(topic, json_payload, qos, retained);
}

bool MQTTPublisher::publishBatch(const std::map<std::string, std::string>& messages, int qos) {
    bool all_success = true;
    
    for (const auto& [topic, payload] : messages) {
        if (!publish(topic, payload, qos)) {
            all_success = false;
        }
    }
    
    return all_success;
}

// Configuration
void MQTTPublisher::setUsername(const std::string& username) {
    pImpl->username_ = username;
}

void MQTTPublisher::setPassword(const std::string& password) {
    pImpl->password_ = password;
}

void MQTTPublisher::setKeepAliveInterval(int seconds) {
    pImpl->keepalive_ = seconds;
}

void MQTTPublisher::setCleanSession(bool clean) {
    pImpl->clean_session_ = clean;
}

void MQTTPublisher::setWillMessage(const std::string& topic, const std::string& message, int qos) {
    pImpl->has_will_ = true;
    pImpl->will_topic_ = topic;
    pImpl->will_message_ = message;
    pImpl->will_qos_ = qos;
}

// Callbacks
void MQTTPublisher::setPublishCallback(PublishCallback callback) {
    pImpl->publish_callback_ = callback;
}

void MQTTPublisher::setConnectionCallback(ConnectionCallback callback) {
    pImpl->connection_callback_ = callback;
}

// Statistics
MQTTPublisher::PublishStats MQTTPublisher::getStats() const {
    return pImpl->stats_;
}

void MQTTPublisher::resetStats() {
    pImpl->stats_ = PublishStats{};
}

// Status
std::string MQTTPublisher::getBrokerHost() const {
    return pImpl->broker_host_;
}

int MQTTPublisher::getBrokerPort() const {
    return pImpl->broker_port_;
}

std::string MQTTPublisher::getClientId() const {
    return pImpl->client_id_;
}

bool MQTTPublisher::isInitialized() const {
    return pImpl->initialized_;
}

// Internal handlers
void MQTTPublisher::handleConnectionLost(const std::string& reason) {
    pImpl->connected_ = false;
    Logger::warn("MQTT Publisher connection lost: " + reason);
    
    if (pImpl->connection_callback_) {
        pImpl->connection_callback_(false, reason);
    }
}

void MQTTPublisher::handlePublishSuccess(const std::string& topic) {
    Logger::debug("MQTT Publish success: " + topic);
}

void MQTTPublisher::handlePublishFailure(const std::string& topic, const std::string& error) {
    Logger::error("MQTT Publish failure for " + topic + ": " + error);
}

} // namespace microservice

