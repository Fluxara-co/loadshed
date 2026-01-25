#pragma once

#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <map>

namespace microservice {

/**
 * @brief Generic Service Health Monitor
 * 
 * Monitors service health including:
 * - Component health checks
 * - Performance metrics
 * - Error tracking
 * - Health history
 */
class HealthMonitor {
public:
    HealthMonitor();
    ~HealthMonitor();
    
    // Copy operations disabled
    HealthMonitor(const HealthMonitor&) = delete;
    HealthMonitor& operator=(const HealthMonitor&) = delete;
    
    // Move operations disabled
    HealthMonitor(HealthMonitor&&) = delete;
    HealthMonitor& operator=(HealthMonitor&&) = delete;
    
    // Service lifecycle
    bool initialize();
    bool start();
    void stop();
    bool isRunning() const;
    
    // Overall health
    bool isHealthy() const;
    std::string getHealthStatus() const;  // "healthy", "degraded", "unhealthy"
    std::vector<std::string> getHealthIssues() const;
    
    // Component registration and health checks
    using HealthCheckFunction = std::function<bool()>;
    void registerComponent(const std::string& component_name, HealthCheckFunction check_func);
    void unregisterComponent(const std::string& component_name);
    bool isComponentHealthy(const std::string& component_name) const;
    std::string getComponentStatus(const std::string& component_name) const;
    
    // Metrics
    struct HealthMetrics {
        uint64_t uptime_seconds{0};
        uint64_t total_errors{0};
        uint64_t errors_last_minute{0};
        uint64_t errors_last_hour{0};
        double cpu_usage_percent{0.0};
        double memory_usage_percent{0.0};
        uint32_t active_connections{0};
        uint32_t total_components{0};
        uint32_t healthy_components{0};
        std::chrono::system_clock::time_point last_health_check;
    };
    
    HealthMetrics getMetrics() const;
    
    // Error tracking
    void recordError(const std::string& error_type, const std::string& error_message = "");
    uint64_t getTotalErrors() const;
    uint64_t getErrorCount(const std::string& error_type) const;
    std::map<std::string, uint64_t> getErrorCounts() const;
    void clearErrors();
    
    // Performance tracking
    void recordResponseTime(const std::string& operation, std::chrono::milliseconds duration);
    std::chrono::milliseconds getAverageResponseTime(const std::string& operation) const;
    std::chrono::milliseconds getMaxResponseTime(const std::string& operation) const;
    
    // Configuration
    std::chrono::seconds getHealthCheckInterval() const;
    void setHealthCheckInterval(std::chrono::seconds interval);
    void setErrorThreshold(uint32_t max_errors_per_minute);
    uint32_t getErrorThreshold() const;
    
    // Callbacks
    using HealthChangedCallback = std::function<void(bool is_healthy, const std::string& reason)>;
    using IssueDetectedCallback = std::function<void(const std::string& issue)>;
    
    void setHealthChangedCallback(HealthChangedCallback callback);
    void setIssueDetectedCallback(IssueDetectedCallback callback);
    
    // Manual health check
    void performHealthCheck();
    
    // Health reporting
    std::string getHealthReport() const;  // JSON string
    std::string getDetailedHealthReport() const;  // JSON string with full details
    std::string getHealthSummary() const;  // Human-readable summary
    
    // Statistics
    uint64_t getHealthChecksPerformed() const;
    std::chrono::system_clock::time_point getLastHealthCheck() const;
    std::chrono::system_clock::time_point getServiceStartTime() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    
    // Internal methods
    void runHealthCheckLoop();
    void updateMetrics();
};

} // namespace microservice

