#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <functional>

namespace microservice {

// Forward declarations
class ApiServer;
class MQTTPublisher;
class MQTTSubscriber;
class HealthMonitor;
class ConfigManager;

/**
 * @brief Generic Service Manager
 * 
 * Provides unified service lifecycle management with:
 * - Service start/stop/restart/reset
 * - Health monitoring integration
 * - API server integration
 * - MQTT service integration
 * - Status and metrics
 */
class ServiceManager {
public:
    ServiceManager();
    ~ServiceManager();
    
    // Copy operations disabled
    ServiceManager(const ServiceManager&) = delete;
    ServiceManager& operator=(const ServiceManager&) = delete;
    
    // Move operations
    ServiceManager(ServiceManager&&) noexcept;
    ServiceManager& operator=(ServiceManager&&) noexcept;
    
    // === Service Lifecycle ===
    
    bool initialize(const std::string& config_file = "config.json");
    bool start();
    bool stop();
    bool restart();
    bool reset();  // Clears state and reinitializes
    bool shutdown();
    
    bool isInitialized() const;
    bool isRunning() const;
    
    // === Component Registration ===
    
    void setApiServer(std::shared_ptr<ApiServer> api_server);
    void setMQTTPublisher(std::shared_ptr<MQTTPublisher> mqtt_publisher);
    void setMQTTSubscriber(std::shared_ptr<MQTTSubscriber> mqtt_subscriber);
    void setHealthMonitor(std::shared_ptr<HealthMonitor> health_monitor);
    
    // Component access
    std::shared_ptr<ApiServer> getApiServer() const;
    std::shared_ptr<MQTTPublisher> getMQTTPublisher() const;
    std::shared_ptr<MQTTSubscriber> getMQTTSubscriber() const;
    std::shared_ptr<HealthMonitor> getHealthMonitor() const;
    std::shared_ptr<ConfigManager> getConfigManager() const;
    
    // === Service Status ===
    
    enum class ServiceState {
        UNINITIALIZED,
        INITIALIZED,
        STARTING,
        RUNNING,
        STOPPING,
        STOPPED,
        ERROR
    };
    
    ServiceState getState() const;
    std::string getStateString() const;
    
    struct ServiceStatus {
        ServiceState state{ServiceState::UNINITIALIZED};
        bool is_healthy{false};
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point last_status_update;
        uint32_t restart_count{0};
        std::string last_error;
        
        // Component status
        bool api_server_running{false};
        bool mqtt_publisher_connected{false};
        bool mqtt_subscriber_connected{false};
        bool health_monitor_running{false};
    };
    
    ServiceStatus getStatus() const;
    bool isHealthy() const;
    std::vector<std::string> getHealthIssues() const;
    
    // === Metrics ===
    
    struct ServiceMetrics {
        uint64_t uptime_seconds{0};
        uint64_t total_restarts{0};
        uint64_t total_errors{0};
        uint32_t api_requests_total{0};
        uint32_t mqtt_messages_published{0};
        uint32_t mqtt_messages_received{0};
        double cpu_usage_percent{0.0};
        double memory_usage_mb{0.0};
    };
    
    ServiceMetrics getMetrics() const;
    std::map<std::string, std::string> getMetricsMap() const;
    
    // === Configuration ===
    
    bool loadConfiguration(const std::string& config_file);
    bool reloadConfiguration();
    bool saveConfiguration(const std::string& config_file = "");
    
    // === Auto-restart Management ===
    
    void enableAutoRestart(bool enable = true);
    bool isAutoRestartEnabled() const;
    void setMaxRestartAttempts(uint32_t max_attempts);
    uint32_t getMaxRestartAttempts() const;
    
    // === Event Callbacks ===
    
    using StateChangeCallback = std::function<void(ServiceState old_state, ServiceState new_state)>;
    using ErrorCallback = std::function<void(const std::string& error_message)>;
    using RestartCallback = std::function<void(uint32_t restart_count)>;
    
    void setStateChangeCallback(StateChangeCallback callback);
    void setErrorCallback(ErrorCallback callback);
    void setRestartCallback(RestartCallback callback);
    
    // === API Integration ===
    // These methods are called by the API endpoints
    
    bool handleStartRequest();
    bool handleStopRequest();
    bool handleRestartRequest();
    bool handleResetRequest();
    std::string handleStatusRequest();  // Returns JSON status
    std::string handleMetricsRequest();  // Returns JSON metrics
    
    // === Custom Service Logic Callbacks ===
    // Implement your business logic by setting these callbacks
    
    using ServiceLogicCallback = std::function<bool()>;
    using ServiceStatusCallback = std::function<std::string()>;  // Returns custom status JSON
    
    void setCustomStartLogic(ServiceLogicCallback callback);
    void setCustomStopLogic(ServiceLogicCallback callback);
    void setCustomRestartLogic(ServiceLogicCallback callback);
    void setCustomResetLogic(ServiceLogicCallback callback);
    void setCustomStatusLogic(ServiceStatusCallback callback);
    
    // === Graceful Shutdown ===
    
    void requestShutdown();
    bool isShutdownRequested() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    
    // Internal methods
    void changeState(ServiceState new_state);
    void recordError(const std::string& error);
    void updateMetrics();
    void logEvent(const std::string& event, const std::string& details = "");
    
    // Component management
    bool startComponents();
    bool stopComponents();
    bool checkComponentsHealth();
};

} // namespace microservice
