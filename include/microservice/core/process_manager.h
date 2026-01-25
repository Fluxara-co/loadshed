#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>
#include <mutex>
#include <functional>
#include <optional>
#include <nlohmann/json.hpp>
#include <sys/types.h>

namespace microservice {

using json = nlohmann::json;

/**
 * @brief Process execution type
 */
enum class ProcessType {
    EXECUTABLE,      // Native binary executable
    SCRIPT,          // Shell script or interpreted script
    DOCKER,          // Docker container (future)
    INTERNAL         // Internal thread-based process
};

/**
 * @brief Process restart policy
 */
struct RestartPolicy {
    bool auto_restart{true};
    uint32_t max_restart_attempts{3};
    uint32_t restart_delay_ms{1000};
    std::vector<int> restart_on_exit_codes{1, 2};  // Which exit codes trigger restart
};

/**
 * @brief Process health check configuration
 */
struct HealthCheckConfig {
    bool enabled{true};
    uint32_t interval_seconds{30};
    uint32_t timeout_seconds{5};
    std::string health_check_command;  // Optional: command to run for health check
};

/**
 * @brief Process resource limits
 */
struct ResourceLimits {
    uint64_t max_memory_mb{0};     // 0 = unlimited
    uint32_t max_cpu_percent{0};   // 0 = unlimited
};

/**
 * @brief Complete process information structure
 */
struct ProcessInfo {
    // Identity
    std::string process_id;
    std::string name;
    std::string description;
    ProcessType type{ProcessType::EXECUTABLE};

    // Execution configuration
    std::string command;                              // Executable path
    std::vector<std::string> args;                    // Command arguments
    std::map<std::string, std::string> environment;   // Environment variables
    std::string working_dir;                          // Working directory

    // Runtime state
    pid_t pid{0};
    bool is_running{false};
    bool is_healthy{false};
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_health_check;
    uint32_t restart_count{0};
    std::string last_error;
    int exit_code{0};

    // Configuration
    RestartPolicy restart_policy;
    HealthCheckConfig health_check;
    ResourceLimits limits;

    // Dependencies
    std::vector<std::string> depends_on;

    // Metadata
    std::map<std::string, std::string> tags;
    std::string owner;
    json config_data;  // Additional configuration as JSON
};

/**
 * @brief Process status structure for API responses
 */
struct ProcessStatus {
    std::string process_id;
    std::string name;
    std::string description;
    ProcessType type;
    pid_t pid{0};
    bool is_running{false};
    bool is_healthy{false};
    uint32_t restart_count{0};
    std::string last_error;
    int exit_code{0};
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_health_check;
    std::chrono::milliseconds uptime{0};

    // Convert to JSON
    json to_json() const;
};

/**
 * @brief Process Manager - Generic process lifecycle management
 *
 * Adapted from device-modbus-service SimpleDeviceManager
 *
 * Features:
 * - CRUD operations for process definitions
 * - Process lifecycle (start, stop, restart)
 * - Health monitoring with auto-restart
 * - Thread-safe operations
 * - Process dependency management
 * - Resource tracking
 */
class ProcessManager {
public:
    ProcessManager();
    ~ProcessManager();

    // Disable copy
    ProcessManager(const ProcessManager&) = delete;
    ProcessManager& operator=(const ProcessManager&) = delete;

    // === CRUD Operations ===

    /**
     * @brief Add a new process definition
     * @param process_id Unique process identifier
     * @param process_info Process configuration
     * @return true if added successfully
     */
    bool addProcess(const std::string& process_id, const ProcessInfo& process_info);

    /**
     * @brief Add process from JSON configuration
     * @param process_id Unique process identifier
     * @param config JSON configuration
     * @return true if added successfully
     */
    bool addProcessFromJson(const std::string& process_id, const json& config);

    /**
     * @brief Remove a process (stops it first if running)
     * @param process_id Process identifier
     * @return true if removed successfully
     */
    bool removeProcess(const std::string& process_id);

    /**
     * @brief Get process information
     * @param process_id Process identifier
     * @return ProcessInfo if found, nullopt otherwise
     */
    std::optional<ProcessInfo> getProcess(const std::string& process_id) const;

    /**
     * @brief Update process configuration (must be stopped)
     * @param process_id Process identifier
     * @param process_info Updated configuration
     * @return true if updated successfully
     */
    bool updateProcess(const std::string& process_id, const ProcessInfo& process_info);

    /**
     * @brief Get list of all process IDs
     * @return Vector of process IDs
     */
    std::vector<std::string> listProcesses() const;

    /**
     * @brief Get list of all process statuses
     * @return Vector of ProcessStatus objects
     */
    std::vector<ProcessStatus> listProcessStatuses() const;

    // === Lifecycle Operations ===

    /**
     * @brief Start a process (fork and exec)
     * @param process_id Process identifier
     * @return true if started successfully
     */
    bool startProcess(const std::string& process_id);

    /**
     * @brief Stop a process (SIGTERM, then SIGKILL if needed)
     * @param process_id Process identifier
     * @param force If true, use SIGKILL immediately
     * @return true if stopped successfully
     */
    bool stopProcess(const std::string& process_id, bool force = false);

    /**
     * @brief Restart a process (stop + start)
     * @param process_id Process identifier
     * @return true if restarted successfully
     */
    bool restartProcess(const std::string& process_id);

    /**
     * @brief Kill a process forcefully (SIGKILL)
     * @param process_id Process identifier
     * @return true if killed successfully
     */
    bool killProcess(const std::string& process_id);

    /**
     * @brief Stop all processes
     */
    void stopAllProcesses();

    // === Health Monitoring ===

    /**
     * @brief Check health of a specific process
     * @param process_id Process identifier
     * @return true if healthy
     */
    bool checkProcessHealth(const std::string& process_id);

    /**
     * @brief Check health of all processes
     */
    void checkAllProcessHealth();

    /**
     * @brief Get process status
     * @param process_id Process identifier
     * @return ProcessStatus if found
     */
    std::optional<ProcessStatus> getProcessStatus(const std::string& process_id) const;

    // === Bulk Operations ===

    /**
     * @brief Start multiple processes
     * @param process_ids List of process IDs to start
     * @return Map of process_id -> success
     */
    std::map<std::string, bool> startProcesses(const std::vector<std::string>& process_ids);

    /**
     * @brief Stop multiple processes
     * @param process_ids List of process IDs to stop
     * @param force Force kill
     * @return Map of process_id -> success
     */
    std::map<std::string, bool> stopProcesses(const std::vector<std::string>& process_ids, bool force = false);

    // === Query Operations ===

    /**
     * @brief Search processes by tag
     * @param tag_key Tag key
     * @param tag_value Tag value (optional, empty = any value)
     * @return List of matching process IDs
     */
    std::vector<std::string> findProcessesByTag(const std::string& tag_key, const std::string& tag_value = "") const;

    /**
     * @brief Get all running processes
     * @return List of running process IDs
     */
    std::vector<std::string> getRunningProcesses() const;

    /**
     * @brief Get all stopped processes
     * @return List of stopped process IDs
     */
    std::vector<std::string> getStoppedProcesses() const;

    // === Configuration ===

    /**
     * @brief Enable/disable auto-restart globally
     */
    void setAutoRestart(bool enabled);

    /**
     * @brief Set default max restart attempts
     */
    void setMaxRestartAttempts(uint32_t max_attempts);

    /**
     * @brief Set health check interval
     */
    void setHealthCheckInterval(uint32_t interval_seconds);

    // === Callbacks ===

    using ProcessEventCallback = std::function<void(const std::string& process_id, const std::string& event, const json& data)>;

    /**
     * @brief Set callback for process events
     * Events: "started", "stopped", "crashed", "restarted", "health_check_failed"
     */
    void setEventCallback(ProcessEventCallback callback);

    // === Statistics ===

    struct ManagerStats {
        uint32_t total_processes{0};
        uint32_t running_processes{0};
        uint32_t stopped_processes{0};
        uint32_t failed_processes{0};
        uint32_t total_restarts{0};
        uint32_t total_starts{0};
        uint32_t total_stops{0};
    };

    ManagerStats getStats() const;

private:
    // Process registry
    std::map<std::string, ProcessInfo> processes_;
    std::map<std::string, pid_t> process_pids_;

    // Configuration
    bool auto_restart_enabled_{true};
    uint32_t max_restart_attempts_{3};
    uint32_t health_check_interval_{30};

    // Thread safety
    mutable std::mutex processes_mutex_;

    // Statistics
    mutable ManagerStats stats_;

    // Callbacks
    ProcessEventCallback event_callback_;

    // Internal helpers
    bool forkAndExec(const ProcessInfo& process_info, pid_t& out_pid);
    bool terminateProcess(pid_t pid, bool force);
    bool isProcessAlive(pid_t pid) const;
    int waitForProcess(pid_t pid, bool blocking = true);
    void emitEvent(const std::string& process_id, const std::string& event, const json& data = {});
};

} // namespace microservice
