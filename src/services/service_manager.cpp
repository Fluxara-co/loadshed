/**
 * Generic Service Manager Implementation
 */

#include "microservice/services/service_manager.h"
#include "microservice/services/api_server.h"
#include "microservice/services/mqtt_publisher.h"
#include "microservice/services/mqtt_subscriber.h"
#include "microservice/services/health_monitor.h"
#include "microservice/utils/config_manager.h"
#include "microservice/utils/logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <chrono>
#include <thread>

using json = nlohmann::json;

namespace microservice {

class ServiceManager::Impl {
public:
    Impl() = default;
    ~Impl() = default;
    
    // Service components
    std::shared_ptr<ApiServer> api_server_;
    std::shared_ptr<MQTTPublisher> mqtt_publisher_;
    std::shared_ptr<MQTTSubscriber> mqtt_subscriber_;
    std::shared_ptr<HealthMonitor> health_monitor_;
    std::shared_ptr<ConfigManager> config_manager_;
    
    // Service state
    ServiceState state_{ServiceState::UNINITIALIZED};
    bool initialized_{false};
    bool running_{false};
    bool shutdown_requested_{false};
    
    // Configuration
    std::string config_file_;
    json config_;
    
    // Status
    ServiceStatus status_;
    ServiceMetrics metrics_;
    
    // Auto-restart
    bool auto_restart_enabled_{false};
    uint32_t max_restart_attempts_{3};
    
    // Callbacks
    StateChangeCallback state_change_callback_;
    ErrorCallback error_callback_;
    RestartCallback restart_callback_;
    
    // Custom logic callbacks
    std::function<bool()> custom_start_logic_;
    std::function<bool()> custom_stop_logic_;
    std::function<bool()> custom_restart_logic_;
    std::function<bool()> custom_reset_logic_;
    std::function<std::string()> custom_status_logic_;
};

// Constructor/Destructor
ServiceManager::ServiceManager() : pImpl(std::make_unique<Impl>()) {}
ServiceManager::~ServiceManager() = default;

// Move operations
ServiceManager::ServiceManager(ServiceManager&&) noexcept = default;
ServiceManager& ServiceManager::operator=(ServiceManager&&) noexcept = default;

// Service Lifecycle
bool ServiceManager::initialize(const std::string& config_file) {
    if (pImpl->initialized_) {
        Logger::warn("Service Manager already initialized");
        return true;
    }
    
    try {
        pImpl->config_file_ = config_file;
        
        // Initialize ConfigManager
        pImpl->config_manager_ = std::make_shared<ConfigManager>();
        if (!pImpl->config_manager_->initialize(config_file)) {
            Logger::error("Failed to initialize Configuration Manager");
            return false;
        }
        
        // Load configuration from ConfigManager
        pImpl->config_ = pImpl->config_manager_->getConfig();
        
        // Load previous state if available
        json prev_state = pImpl->config_manager_->getState();
        if (!prev_state.empty()) {
            Logger::info("Loading previous service state...");
            if (prev_state.contains("metrics")) {
                auto& m = prev_state["metrics"];
                pImpl->metrics_.total_restarts = m.value("total_restarts", 0);
                pImpl->metrics_.total_errors = m.value("total_errors", 0);
                Logger::info("  Previous restarts: " + std::to_string(pImpl->metrics_.total_restarts));
            }
        }
        
        pImpl->status_.start_time = std::chrono::system_clock::now();
        pImpl->initialized_ = true;
        changeState(ServiceState::INITIALIZED);
        
        Logger::info("Service Manager initialized");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Service Manager initialization failed: " + std::string(e.what()));
        changeState(ServiceState::ERROR);
        return false;
    }
}

bool ServiceManager::start() {
    if (!pImpl->initialized_) {
        Logger::error("Service Manager not initialized");
        return false;
    }
    
    if (pImpl->running_) {
        Logger::warn("Service Manager already running");
        return true;
    }
    
    try {
        changeState(ServiceState::STARTING);
        Logger::info("Starting Service Manager...");
        
        // Execute custom start logic
        if (pImpl->custom_start_logic_) {
            if (!pImpl->custom_start_logic_()) {
                Logger::error("Custom start logic failed");
                changeState(ServiceState::ERROR);
                return false;
            }
        }
        
        // Start components
        if (!startComponents()) {
            Logger::error("Failed to start components");
            changeState(ServiceState::ERROR);
            return false;
        }
        
        pImpl->running_ = true;
        changeState(ServiceState::RUNNING);
        
        Logger::info("Service Manager started successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Service Manager start failed: " + std::string(e.what()));
        changeState(ServiceState::ERROR);
        recordError(e.what());
        return false;
    }
}

bool ServiceManager::stop() {
    if (!pImpl->running_) {
    return true;
}

    try {
        changeState(ServiceState::STOPPING);
        Logger::info("Stopping Service Manager...");
        
        // Save state before stopping
        if (pImpl->config_manager_) {
            Logger::info("Saving service state...");
            json state;
            state["last_stop_time"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            state["metrics"] = {
                {"total_restarts", pImpl->metrics_.total_restarts},
                {"total_errors", pImpl->metrics_.total_errors},
                {"api_requests_total", pImpl->metrics_.api_requests_total},
                {"mqtt_messages_published", pImpl->metrics_.mqtt_messages_published},
                {"mqtt_messages_received", pImpl->metrics_.mqtt_messages_received}
            };
            pImpl->config_manager_->setState(state);
            pImpl->config_manager_->saveState();
            Logger::info("Service state saved");
        }
        
        // Execute custom stop logic
        if (pImpl->custom_stop_logic_) {
            pImpl->custom_stop_logic_();
        }
        
        // Stop components
        stopComponents();
        
        pImpl->running_ = false;
        changeState(ServiceState::STOPPED);
        
        Logger::info("Service Manager stopped");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Service Manager stop failed: " + std::string(e.what()));
        recordError(e.what());
        return false;
    }
}

bool ServiceManager::restart() {
    Logger::info("Restarting Service Manager...");
    
    // Increment restart counter
    pImpl->metrics_.total_restarts++;
    Logger::info("  Total restarts: " + std::to_string(pImpl->metrics_.total_restarts));
    
    // Execute custom restart logic
    if (pImpl->custom_restart_logic_) {
        if (!pImpl->custom_restart_logic_()) {
            Logger::error("Custom restart logic failed");
            return false;
        }
    }
    
    if (!stop()) {
        Logger::error("Failed to stop service during restart");
        return false;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    if (!start()) {
        Logger::error("Failed to start service during restart");
        return false;
    }
    
    pImpl->status_.restart_count++;
    pImpl->metrics_.total_restarts++;
    
    if (pImpl->restart_callback_) {
        pImpl->restart_callback_(pImpl->status_.restart_count);
    }
    
    Logger::info("Service Manager restarted successfully");
    return true;
}

bool ServiceManager::reset() {
    Logger::info("Resetting Service Manager...");
    
    // Stop first
    if (pImpl->running_) {
        stop();
    }
    
    // Execute custom reset logic
    if (pImpl->custom_reset_logic_) {
        if (!pImpl->custom_reset_logic_()) {
            Logger::error("Custom reset logic failed");
            return false;
        }
    }
    
    // Reset state
    pImpl->status_ = ServiceStatus{};
    pImpl->metrics_ = ServiceMetrics{};
    pImpl->status_.start_time = std::chrono::system_clock::now();
    
    Logger::info("Service Manager reset successfully");
    return true;
}

bool ServiceManager::shutdown() {
    Logger::info("Shutting down Service Manager...");
    pImpl->shutdown_requested_ = true;
    return stop();
}

bool ServiceManager::isInitialized() const {
    return pImpl->initialized_;
}

bool ServiceManager::isRunning() const {
    return pImpl->running_;
}

// Component registration
void ServiceManager::setApiServer(std::shared_ptr<ApiServer> api_server) {
    pImpl->api_server_ = api_server;
}

void ServiceManager::setMQTTPublisher(std::shared_ptr<MQTTPublisher> mqtt_publisher) {
    pImpl->mqtt_publisher_ = mqtt_publisher;
}

void ServiceManager::setMQTTSubscriber(std::shared_ptr<MQTTSubscriber> mqtt_subscriber) {
    pImpl->mqtt_subscriber_ = mqtt_subscriber;
}

void ServiceManager::setHealthMonitor(std::shared_ptr<HealthMonitor> health_monitor) {
    pImpl->health_monitor_ = health_monitor;
}

// Component access
std::shared_ptr<ApiServer> ServiceManager::getApiServer() const {
    return pImpl->api_server_;
}

std::shared_ptr<MQTTPublisher> ServiceManager::getMQTTPublisher() const {
    return pImpl->mqtt_publisher_;
}

std::shared_ptr<MQTTSubscriber> ServiceManager::getMQTTSubscriber() const {
    return pImpl->mqtt_subscriber_;
}

std::shared_ptr<HealthMonitor> ServiceManager::getHealthMonitor() const {
    return pImpl->health_monitor_;
}

std::shared_ptr<ConfigManager> ServiceManager::getConfigManager() const {
    return pImpl->config_manager_;
}

// Service Status
ServiceManager::ServiceState ServiceManager::getState() const {
    return pImpl->state_;
}

std::string ServiceManager::getStateString() const {
    switch (pImpl->state_) {
        case ServiceState::UNINITIALIZED: return "uninitialized";
        case ServiceState::INITIALIZED: return "initialized";
        case ServiceState::STARTING: return "starting";
        case ServiceState::RUNNING: return "running";
        case ServiceState::STOPPING: return "stopping";
        case ServiceState::STOPPED: return "stopped";
        case ServiceState::ERROR: return "error";
        default: return "unknown";
    }
}

ServiceManager::ServiceStatus ServiceManager::getStatus() const {
    auto status = pImpl->status_;
    status.state = pImpl->state_;
    status.is_healthy = isHealthy();
    status.last_status_update = std::chrono::system_clock::now();
    
    // Update component status
    status.api_server_running = pImpl->api_server_ && pImpl->api_server_->isRunning();
    status.mqtt_publisher_connected = pImpl->mqtt_publisher_ && pImpl->mqtt_publisher_->isConnected();
    status.mqtt_subscriber_connected = pImpl->mqtt_subscriber_ && pImpl->mqtt_subscriber_->isConnected();
    status.health_monitor_running = pImpl->health_monitor_ && pImpl->health_monitor_->isRunning();
    
    return status;
}

bool ServiceManager::isHealthy() const {
    return pImpl->state_ == ServiceState::RUNNING && 
           pImpl->status_.last_error.empty();
}

std::vector<std::string> ServiceManager::getHealthIssues() const {
    std::vector<std::string> issues;
    
    if (pImpl->state_ == ServiceState::ERROR) {
        issues.push_back("Service in error state");
    }
    
    if (!pImpl->status_.last_error.empty()) {
        issues.push_back("Last error: " + pImpl->status_.last_error);
    }
    
    if (pImpl->api_server_ && !pImpl->api_server_->isRunning()) {
        issues.push_back("API Server not running");
    }
    
    return issues;
}

// Metrics
ServiceManager::ServiceMetrics ServiceManager::getMetrics() const {
    auto metrics = pImpl->metrics_;
    
    if (pImpl->status_.start_time.time_since_epoch().count() > 0) {
        metrics.uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now() - pImpl->status_.start_time).count();
    }
    
    if (pImpl->api_server_) {
        auto api_stats = pImpl->api_server_->getStats();
        metrics.api_requests_total = api_stats.total_requests;
    }
    
    if (pImpl->mqtt_publisher_) {
        auto pub_stats = pImpl->mqtt_publisher_->getStats();
        metrics.mqtt_messages_published = pub_stats.total_published;
    }
    
    if (pImpl->mqtt_subscriber_) {
        auto sub_stats = pImpl->mqtt_subscriber_->getStats();
        metrics.mqtt_messages_received = sub_stats.total_messages_received;
    }
    
    return metrics;
}

std::map<std::string, std::string> ServiceManager::getMetricsMap() const {
    auto metrics = getMetrics();
    
    return {
        {"uptime_seconds", std::to_string(metrics.uptime_seconds)},
        {"total_restarts", std::to_string(metrics.total_restarts)},
        {"total_errors", std::to_string(metrics.total_errors)},
        {"api_requests_total", std::to_string(metrics.api_requests_total)},
        {"mqtt_messages_published", std::to_string(metrics.mqtt_messages_published)},
        {"mqtt_messages_received", std::to_string(metrics.mqtt_messages_received)},
        {"cpu_usage_percent", std::to_string(metrics.cpu_usage_percent)},
        {"memory_usage_mb", std::to_string(metrics.memory_usage_mb)}
    };
}

// Configuration
bool ServiceManager::loadConfiguration(const std::string& config_file) {
    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            Logger::warn("Config file not found: " + config_file + " - using defaults");
            pImpl->config_ = json::object();
            return true;
        }
        
        file >> pImpl->config_;
        Logger::info("Configuration loaded from: " + config_file);
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Failed to load configuration: " + std::string(e.what()));
        return false;
    }
}

bool ServiceManager::reloadConfiguration() {
    Logger::info("Reloading configuration...");
    return loadConfiguration(pImpl->config_file_);
}

bool ServiceManager::saveConfiguration(const std::string& config_file) {
    try {
        std::string file_path = config_file.empty() ? pImpl->config_file_ : config_file;
        
        std::ofstream file(file_path);
        if (!file.is_open()) {
            Logger::error("Failed to open config file for writing: " + file_path);
            return false;
        }
        
        file << pImpl->config_.dump(2);
        Logger::info("Configuration saved to: " + file_path);
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Failed to save configuration: " + std::string(e.what()));
        return false;
    }
}

// Auto-restart
void ServiceManager::enableAutoRestart(bool enable) {
    pImpl->auto_restart_enabled_ = enable;
    Logger::info(enable ? "Auto-restart enabled" : "Auto-restart disabled");
}

bool ServiceManager::isAutoRestartEnabled() const {
    return pImpl->auto_restart_enabled_;
}

void ServiceManager::setMaxRestartAttempts(uint32_t max_attempts) {
    pImpl->max_restart_attempts_ = max_attempts;
}

uint32_t ServiceManager::getMaxRestartAttempts() const {
    return pImpl->max_restart_attempts_;
}

// Event callbacks
void ServiceManager::setStateChangeCallback(StateChangeCallback callback) {
    pImpl->state_change_callback_ = callback;
}

void ServiceManager::setErrorCallback(ErrorCallback callback) {
    pImpl->error_callback_ = callback;
}

void ServiceManager::setRestartCallback(RestartCallback callback) {
    pImpl->restart_callback_ = callback;
}

// API Integration
bool ServiceManager::handleStartRequest() {
    Logger::info("API Request: Start service");
    return start();
}

bool ServiceManager::handleStopRequest() {
    Logger::info("API Request: Stop service");
    return stop();
}

bool ServiceManager::handleRestartRequest() {
    Logger::info("API Request: Restart service");
    return restart();
}

bool ServiceManager::handleResetRequest() {
    Logger::info("API Request: Reset service");
    return reset();
}

std::string ServiceManager::handleStatusRequest() {
    auto status = getStatus();
    
    json status_json = {
        {"state", getStateString()},
        {"is_healthy", status.is_healthy},
        {"start_time", std::chrono::duration_cast<std::chrono::milliseconds>(
            status.start_time.time_since_epoch()).count()},
        {"restart_count", status.restart_count},
        {"last_error", status.last_error},
        {"components", {
            {"api_server_running", status.api_server_running},
            {"mqtt_publisher_connected", status.mqtt_publisher_connected},
            {"mqtt_subscriber_connected", status.mqtt_subscriber_connected},
            {"health_monitor_running", status.health_monitor_running}
        }}
    };
    
    // Add custom status
    if (pImpl->custom_status_logic_) {
        try {
            status_json["custom"] = json::parse(pImpl->custom_status_logic_());
        } catch (...) {
            status_json["custom"] = "error_getting_custom_status";
        }
    }
    
    return status_json.dump(2);
}

std::string ServiceManager::handleMetricsRequest() {
    auto metrics = getMetrics();
    
    json metrics_json = {
        {"uptime_seconds", metrics.uptime_seconds},
        {"total_restarts", metrics.total_restarts},
        {"total_errors", metrics.total_errors},
        {"api_requests_total", metrics.api_requests_total},
        {"mqtt_messages_published", metrics.mqtt_messages_published},
        {"mqtt_messages_received", metrics.mqtt_messages_received},
        {"cpu_usage_percent", metrics.cpu_usage_percent},
        {"memory_usage_mb", metrics.memory_usage_mb}
    };
    
    return metrics_json.dump(2);
}

// Custom service logic callbacks
void ServiceManager::setCustomStartLogic(ServiceLogicCallback callback) {
    pImpl->custom_start_logic_ = callback;
}

void ServiceManager::setCustomStopLogic(ServiceLogicCallback callback) {
    pImpl->custom_stop_logic_ = callback;
}

void ServiceManager::setCustomRestartLogic(ServiceLogicCallback callback) {
    pImpl->custom_restart_logic_ = callback;
}

void ServiceManager::setCustomResetLogic(ServiceLogicCallback callback) {
    pImpl->custom_reset_logic_ = callback;
}

void ServiceManager::setCustomStatusLogic(ServiceStatusCallback callback) {
    pImpl->custom_status_logic_ = callback;
}

// Graceful shutdown
void ServiceManager::requestShutdown() {
    Logger::info("Shutdown requested");
    pImpl->shutdown_requested_ = true;
}

bool ServiceManager::isShutdownRequested() const {
    return pImpl->shutdown_requested_;
}

// Internal methods
void ServiceManager::changeState(ServiceState new_state) {
    auto old_state = pImpl->state_;
    pImpl->state_ = new_state;
    
    Logger::info("State change: " + getStateString());
    
    if (pImpl->state_change_callback_) {
        pImpl->state_change_callback_(old_state, new_state);
    }
}

void ServiceManager::recordError(const std::string& error) {
    pImpl->status_.last_error = error;
    pImpl->metrics_.total_errors++;
    
    Logger::error("Service error: " + error);
    
    if (pImpl->error_callback_) {
        pImpl->error_callback_(error);
    }
}

void ServiceManager::updateMetrics() {
    // Metrics are updated in getMetrics()
}

void ServiceManager::logEvent(const std::string& event, const std::string& details) {
    std::string log_message = event;
    if (!details.empty()) {
        log_message += ": " + details;
    }
    Logger::info(log_message);
}

// Component management
bool ServiceManager::startComponents() {
    Logger::info("Starting components...");
    
    // Start API Server
    if (pImpl->api_server_) {
        if (!pImpl->api_server_->start()) {
            Logger::error("Failed to start API Server");
            return false;
        }
        pImpl->status_.api_server_running = true;
    }
    
    // Start MQTT Publisher
    if (pImpl->mqtt_publisher_) {
        if (pImpl->mqtt_publisher_->connect()) {
            pImpl->status_.mqtt_publisher_connected = true;
        } else {
            Logger::warn("MQTT Publisher connection failed (non-fatal)");
        }
    }
    
    // Start MQTT Subscriber
    if (pImpl->mqtt_subscriber_) {
        if (pImpl->mqtt_subscriber_->connect()) {
            pImpl->status_.mqtt_subscriber_connected = true;
        } else {
            Logger::warn("MQTT Subscriber connection failed (non-fatal)");
        }
    }
    
    // Start Health Monitor
    if (pImpl->health_monitor_) {
        if (pImpl->health_monitor_->start()) {
            pImpl->status_.health_monitor_running = true;
        } else {
            Logger::warn("Health Monitor start failed (non-fatal)");
        }
    }
    
    Logger::info("Components started");
    return true;
}

bool ServiceManager::stopComponents() {
    Logger::info("Stopping components...");
    
    // Stop Health Monitor
    if (pImpl->health_monitor_) {
        pImpl->health_monitor_->stop();
        pImpl->status_.health_monitor_running = false;
    }
    
    // Stop MQTT Subscriber
    if (pImpl->mqtt_subscriber_) {
        pImpl->mqtt_subscriber_->disconnect();
        pImpl->status_.mqtt_subscriber_connected = false;
    }
    
    // Stop MQTT Publisher
    if (pImpl->mqtt_publisher_) {
        pImpl->mqtt_publisher_->disconnect();
        pImpl->status_.mqtt_publisher_connected = false;
    }
    
    // Stop API Server
    if (pImpl->api_server_) {
        pImpl->api_server_->stop();
        pImpl->status_.api_server_running = false;
    }
    
    Logger::info("Components stopped");
    return true;
}

bool ServiceManager::checkComponentsHealth() {
    bool all_healthy = true;
    
    if (pImpl->api_server_ && !pImpl->api_server_->isRunning()) {
        all_healthy = false;
    }
    
    // MQTT components are optional - don't fail if they're not connected
    
    return all_healthy;
}

} // namespace microservice
