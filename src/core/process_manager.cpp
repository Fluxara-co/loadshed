/**
 * Process Manager Implementation
 *
 * Adapted from device-modbus-service SimpleDeviceManager
 * Generic process lifecycle management with health monitoring
 */

#include "microservice/core/process_manager.h"
#include "microservice/utils/logger.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <optional>

namespace microservice {

// ============================================================================
// ProcessStatus JSON Conversion
// ============================================================================

json ProcessStatus::to_json() const {
    return json{
        {"process_id", process_id},
        {"name", name},
        {"description", description},
        {"type", static_cast<int>(type)},
        {"pid", pid},
        {"is_running", is_running},
        {"is_healthy", is_healthy},
        {"restart_count", restart_count},
        {"last_error", last_error},
        {"exit_code", exit_code},
        {"uptime_seconds", uptime.count() / 1000}
    };
}

// ============================================================================
// ProcessManager Implementation
// ============================================================================

ProcessManager::ProcessManager() {
    Logger::info("ProcessManager initialized");
}

ProcessManager::~ProcessManager() {
    Logger::info("ProcessManager shutting down - stopping all processes");
    stopAllProcesses();
}

// ============================================================================
// CRUD Operations
// ============================================================================

bool ProcessManager::addProcess(const std::string& process_id, const ProcessInfo& process_info) {
    std::lock_guard<std::mutex> lock(processes_mutex_);

    if (processes_.find(process_id) != processes_.end()) {
        Logger::warn("Process already exists: " + process_id);
        return false;
    }

    ProcessInfo info = process_info;
    info.process_id = process_id;

    processes_[process_id] = info;
    stats_.total_processes++;

    Logger::info("Process added: " + process_id + " (" + info.name + ")");
    emitEvent(process_id, "added", {{"name", info.name}});

    return true;
}

bool ProcessManager::addProcessFromJson(const std::string& process_id, const json& config) {
    try {
        ProcessInfo info;
        info.process_id = process_id;
        info.name = config.value("name", process_id);
        info.description = config.value("description", "");
        info.command = config.value("command", "");
        info.working_dir = config.value("working_dir", "");

        // Parse type
        std::string type_str = config.value("type", "executable");
        if (type_str == "executable") info.type = ProcessType::EXECUTABLE;
        else if (type_str == "script") info.type = ProcessType::SCRIPT;
        else if (type_str == "docker") info.type = ProcessType::DOCKER;
        else if (type_str == "internal") info.type = ProcessType::INTERNAL;

        // Parse args
        if (config.contains("args") && config["args"].is_array()) {
            for (const auto& arg : config["args"]) {
                info.args.push_back(arg.get<std::string>());
            }
        }

        // Parse environment
        if (config.contains("environment") && config["environment"].is_object()) {
            for (auto& [key, value] : config["environment"].items()) {
                info.environment[key] = value.get<std::string>();
            }
        }

        // Parse restart policy
        if (config.contains("restart_policy")) {
            auto& rp = config["restart_policy"];
            info.restart_policy.auto_restart = rp.value("auto_restart", true);
            info.restart_policy.max_restart_attempts = rp.value("max_retries", 3);
            info.restart_policy.restart_delay_ms = rp.value("backoff_seconds", 1) * 1000;
        }

        // Parse health check
        if (config.contains("health_check")) {
            auto& hc = config["health_check"];
            info.health_check.enabled = hc.value("enabled", true);
            info.health_check.interval_seconds = hc.value("interval_seconds", 30);
            info.health_check.timeout_seconds = hc.value("timeout_seconds", 5);
        }

        // Parse resource limits
        if (config.contains("resource_limits")) {
            auto& rl = config["resource_limits"];
            info.limits.max_memory_mb = rl.value("max_memory_mb", 0);
            info.limits.max_cpu_percent = rl.value("max_cpu_percent", 0);
        }

        // Parse dependencies
        if (config.contains("dependencies") && config["dependencies"].is_array()) {
            for (const auto& dep : config["dependencies"]) {
                info.depends_on.push_back(dep.get<std::string>());
            }
        }

        // Parse tags
        if (config.contains("metadata") && config["metadata"].contains("tags")) {
            for (auto& [key, value] : config["metadata"]["tags"].items()) {
                info.tags[key] = value.get<std::string>();
            }
        }

        info.config_data = config;

        return addProcess(process_id, info);

    } catch (const std::exception& e) {
        Logger::error("Failed to parse process config: " + std::string(e.what()));
        return false;
    }
}

bool ProcessManager::removeProcess(const std::string& process_id) {
    // Check if exists and stop if running
    {
        std::lock_guard<std::mutex> lock(processes_mutex_);
        auto it = processes_.find(process_id);
        if (it == processes_.end()) {
            Logger::warn("Process not found: " + process_id);
            return false;
        }

        if (it->second.is_running) {
            // Release lock before stopping to avoid deadlock
            processes_mutex_.unlock();
            stopProcess(process_id);
            processes_mutex_.lock();
        }
    }

    // Remove from registry
    {
        std::lock_guard<std::mutex> lock(processes_mutex_);
        processes_.erase(process_id);
        stats_.total_processes--;
    }

    Logger::info("Process removed: " + process_id);
    emitEvent(process_id, "removed");

    return true;
}

std::optional<ProcessInfo> ProcessManager::getProcess(const std::string& process_id) const {
    std::lock_guard<std::mutex> lock(processes_mutex_);

    auto it = processes_.find(process_id);
    if (it == processes_.end()) {
        return std::nullopt;
    }

    return it->second;
}

bool ProcessManager::updateProcess(const std::string& process_id, const ProcessInfo& process_info) {
    std::lock_guard<std::mutex> lock(processes_mutex_);

    auto it = processes_.find(process_id);
    if (it == processes_.end()) {
        Logger::warn("Process not found: " + process_id);
        return false;
    }

    if (it->second.is_running) {
        Logger::warn("Cannot update running process: " + process_id);
        return false;
    }

    // Preserve runtime state
    ProcessInfo updated = process_info;
    updated.process_id = process_id;
    updated.pid = it->second.pid;
    updated.is_running = it->second.is_running;
    updated.is_healthy = it->second.is_healthy;
    updated.start_time = it->second.start_time;
    updated.restart_count = it->second.restart_count;

    processes_[process_id] = updated;

    Logger::info("Process updated: " + process_id);
    return true;
}

std::vector<std::string> ProcessManager::listProcesses() const {
    std::lock_guard<std::mutex> lock(processes_mutex_);

    std::vector<std::string> process_ids;
    process_ids.reserve(processes_.size());

    for (const auto& [pid, info] : processes_) {
        process_ids.push_back(pid);
    }

    return process_ids;
}

std::vector<ProcessStatus> ProcessManager::listProcessStatuses() const {
    std::lock_guard<std::mutex> lock(processes_mutex_);

    std::vector<ProcessStatus> statuses;
    statuses.reserve(processes_.size());

    auto now = std::chrono::system_clock::now();

    for (const auto& [pid, info] : processes_) {
        ProcessStatus status;
        status.process_id = info.process_id;
        status.name = info.name;
        status.description = info.description;
        status.type = info.type;
        status.pid = info.pid;
        status.is_running = info.is_running;
        status.is_healthy = info.is_healthy;
        status.restart_count = info.restart_count;
        status.last_error = info.last_error;
        status.exit_code = info.exit_code;
        status.start_time = info.start_time;
        status.last_health_check = info.last_health_check;

        if (info.is_running) {
            status.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - info.start_time);
        }

        statuses.push_back(status);
    }

    return statuses;
}

// ============================================================================
// Lifecycle Operations (Adapted from SimpleDeviceManager)
// ============================================================================

bool ProcessManager::startProcess(const std::string& process_id) {
    std::lock_guard<std::mutex> lock(processes_mutex_);

    auto it = processes_.find(process_id);
    if (it == processes_.end()) {
        Logger::error("Process not found: " + process_id);
        return false;
    }

    if (it->second.is_running) {
        Logger::warn("Process already running: " + process_id);
        return true;
    }

    ProcessInfo& info = it->second;

    // Check dependencies
    for (const auto& dep : info.depends_on) {
        auto dep_it = processes_.find(dep);
        if (dep_it == processes_.end() || !dep_it->second.is_running) {
            Logger::error("Dependency not running: " + dep + " required by " + process_id);
            info.last_error = "Dependency " + dep + " not running";
            return false;
        }
    }

    // Fork and exec
    pid_t pid;
    if (!forkAndExec(info, pid)) {
        Logger::error("Failed to start process: " + process_id);
        return false;
    }

    // Update state
    info.pid = pid;
    info.is_running = true;
    info.is_healthy = true;
    info.start_time = std::chrono::system_clock::now();
    info.last_health_check = std::chrono::system_clock::now();
    info.last_error = "";
    process_pids_[process_id] = pid;

    stats_.total_starts++;
    stats_.running_processes++;

    Logger::info("Process started: " + process_id + " (PID: " + std::to_string(pid) + ")");
    emitEvent(process_id, "started", {{"pid", pid}});

    return true;
}

bool ProcessManager::stopProcess(const std::string& process_id, bool force) {
    std::lock_guard<std::mutex> lock(processes_mutex_);

    auto it = processes_.find(process_id);
    if (it == processes_.end()) {
        Logger::error("Process not found: " + process_id);
        return false;
    }

    if (!it->second.is_running) {
        Logger::warn("Process not running: " + process_id);
        return true;
    }

    ProcessInfo& info = it->second;

    // Terminate the process
    if (!terminateProcess(info.pid, force)) {
        Logger::error("Failed to terminate process: " + process_id);
        return false;
    }

    // Wait for process to exit
    int exit_code = waitForProcess(info.pid, true);
    info.exit_code = exit_code;

    // Update state
    info.is_running = false;
    info.is_healthy = false;
    process_pids_.erase(process_id);

    stats_.total_stops++;
    stats_.running_processes--;
    stats_.stopped_processes++;

    Logger::info("Process stopped: " + process_id + " (exit code: " + std::to_string(exit_code) + ")");
    emitEvent(process_id, "stopped", {{"exit_code", exit_code}});

    return true;
}

bool ProcessManager::restartProcess(const std::string& process_id) {
    Logger::info("Restarting process: " + process_id);

    if (!stopProcess(process_id)) {
        return false;
    }

    // Wait a moment before restarting
    {
        std::lock_guard<std::mutex> lock(processes_mutex_);
        auto it = processes_.find(process_id);
        if (it != processes_.end()) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(it->second.restart_policy.restart_delay_ms)
            );
        }
    }

    if (!startProcess(process_id)) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(processes_mutex_);
        auto it = processes_.find(process_id);
        if (it != processes_.end()) {
            it->second.restart_count++;
            stats_.total_restarts++;
        }
    }

    emitEvent(process_id, "restarted");
    return true;
}

bool ProcessManager::killProcess(const std::string& process_id) {
    return stopProcess(process_id, true);
}

void ProcessManager::stopAllProcesses() {
    std::lock_guard<std::mutex> lock(processes_mutex_);

    Logger::info("Stopping all processes (" + std::to_string(processes_.size()) + ")");

    for (auto& [pid, info] : processes_) {
        if (info.is_running) {
            Logger::info("Stopping: " + pid);
            terminateProcess(info.pid, false);
        }
    }

    // Wait for all to exit
    for (auto& [pid, info] : processes_) {
        if (info.pid > 0) {
            waitForProcess(info.pid, true);
            info.is_running = false;
            info.is_healthy = false;
        }
    }

    process_pids_.clear();
    stats_.running_processes = 0;

    Logger::info("All processes stopped");
}

// ============================================================================
// Health Monitoring (Adapted from SimpleDeviceManager::checkDeviceHealth)
// ============================================================================

bool ProcessManager::checkProcessHealth(const std::string& process_id) {
    std::lock_guard<std::mutex> lock(processes_mutex_);

    auto it = processes_.find(process_id);
    if (it == processes_.end()) {
        return false;
    }

    ProcessInfo& info = it->second;

    if (!info.is_running) {
        return false;
    }

    // Check if process is still alive using kill(pid, 0)
    bool is_alive = isProcessAlive(info.pid);

    if (!is_alive) {
        // Process died
        Logger::warn("Process terminated unexpectedly: " + process_id);

        int exit_code = waitForProcess(info.pid, false);
        info.exit_code = exit_code;
        info.is_running = false;
        info.is_healthy = false;
        info.last_error = "Process terminated with exit code " + std::to_string(exit_code);
        process_pids_.erase(process_id);

        stats_.running_processes--;
        stats_.failed_processes++;

        emitEvent(process_id, "crashed", {{"exit_code", exit_code}});

        // Auto-restart if enabled
        if (info.restart_policy.auto_restart &&
            info.restart_count < info.restart_policy.max_restart_attempts) {

            Logger::warn("Auto-restarting process: " + process_id + " (attempt " +
                        std::to_string(info.restart_count + 1) + "/" +
                        std::to_string(info.restart_policy.max_restart_attempts) + ")");

            // Sleep before restart
            std::this_thread::sleep_for(
                std::chrono::milliseconds(info.restart_policy.restart_delay_ms)
            );

            // Unlock mutex before calling startProcess to avoid deadlock
            processes_mutex_.unlock();
            bool restarted = startProcess(process_id);
            processes_mutex_.lock();

            if (restarted) {
                it->second.restart_count++;
                stats_.total_restarts++;
            }

            return false;
        } else {
            Logger::error("Process exceeded max restart attempts: " + process_id);
        }

        return false;
    }

    // Process is alive and healthy
    info.is_healthy = true;
    info.last_health_check = std::chrono::system_clock::now();

    return true;
}

void ProcessManager::checkAllProcessHealth() {
    auto process_ids = listProcesses();

    for (const auto& pid : process_ids) {
        checkProcessHealth(pid);
    }
}

std::optional<ProcessStatus> ProcessManager::getProcessStatus(const std::string& process_id) const {
    std::lock_guard<std::mutex> lock(processes_mutex_);

    auto it = processes_.find(process_id);
    if (it == processes_.end()) {
        return std::nullopt;
    }

    const ProcessInfo& info = it->second;

    ProcessStatus status;
    status.process_id = info.process_id;
    status.name = info.name;
    status.description = info.description;
    status.type = info.type;
    status.pid = info.pid;
    status.is_running = info.is_running;
    status.is_healthy = info.is_healthy;
    status.restart_count = info.restart_count;
    status.last_error = info.last_error;
    status.exit_code = info.exit_code;
    status.start_time = info.start_time;
    status.last_health_check = info.last_health_check;

    if (info.is_running) {
        auto now = std::chrono::system_clock::now();
        status.uptime = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - info.start_time);
    }

    return status;
}

// ============================================================================
// Bulk Operations
// ============================================================================

std::map<std::string, bool> ProcessManager::startProcesses(const std::vector<std::string>& process_ids) {
    std::map<std::string, bool> results;

    for (const auto& pid : process_ids) {
        results[pid] = startProcess(pid);
    }

    return results;
}

std::map<std::string, bool> ProcessManager::stopProcesses(const std::vector<std::string>& process_ids, bool force) {
    std::map<std::string, bool> results;

    for (const auto& pid : process_ids) {
        results[pid] = stopProcess(pid, force);
    }

    return results;
}

// ============================================================================
// Query Operations
// ============================================================================

std::vector<std::string> ProcessManager::findProcessesByTag(const std::string& tag_key, const std::string& tag_value) const {
    std::lock_guard<std::mutex> lock(processes_mutex_);

    std::vector<std::string> matching;

    for (const auto& [pid, info] : processes_) {
        auto tag_it = info.tags.find(tag_key);
        if (tag_it != info.tags.end()) {
            if (tag_value.empty() || tag_it->second == tag_value) {
                matching.push_back(pid);
            }
        }
    }

    return matching;
}

std::vector<std::string> ProcessManager::getRunningProcesses() const {
    std::lock_guard<std::mutex> lock(processes_mutex_);

    std::vector<std::string> running;

    for (const auto& [pid, info] : processes_) {
        if (info.is_running) {
            running.push_back(pid);
        }
    }

    return running;
}

std::vector<std::string> ProcessManager::getStoppedProcesses() const {
    std::lock_guard<std::mutex> lock(processes_mutex_);

    std::vector<std::string> stopped;

    for (const auto& [pid, info] : processes_) {
        if (!info.is_running) {
            stopped.push_back(pid);
        }
    }

    return stopped;
}

// ============================================================================
// Configuration
// ============================================================================

void ProcessManager::setAutoRestart(bool enabled) {
    auto_restart_enabled_ = enabled;
    Logger::info("Auto-restart " + std::string(enabled ? "enabled" : "disabled"));
}

void ProcessManager::setMaxRestartAttempts(uint32_t max_attempts) {
    max_restart_attempts_ = max_attempts;
    Logger::info("Max restart attempts set to: " + std::to_string(max_attempts));
}

void ProcessManager::setHealthCheckInterval(uint32_t interval_seconds) {
    health_check_interval_ = interval_seconds;
    Logger::info("Health check interval set to: " + std::to_string(interval_seconds) + " seconds");
}

void ProcessManager::setEventCallback(ProcessEventCallback callback) {
    event_callback_ = callback;
}

// ============================================================================
// Statistics
// ============================================================================

ProcessManager::ManagerStats ProcessManager::getStats() const {
    std::lock_guard<std::mutex> lock(processes_mutex_);
    return stats_;
}

// ============================================================================
// Internal Helper Methods
// ============================================================================

bool ProcessManager::forkAndExec(const ProcessInfo& process_info, pid_t& out_pid) {
    pid_t pid = fork();

    if (pid == 0) {
        // Child process

        // Change working directory if specified
        if (!process_info.working_dir.empty()) {
            if (chdir(process_info.working_dir.c_str()) != 0) {
                Logger::error("Failed to change directory to: " + process_info.working_dir);
                exit(1);
            }
        }

        // Set environment variables
        for (const auto& [key, value] : process_info.environment) {
            setenv(key.c_str(), value.c_str(), 1);
        }

        // Prepare arguments for execve
        std::vector<char*> args;
        args.push_back(const_cast<char*>(process_info.command.c_str()));

        for (const auto& arg : process_info.args) {
            args.push_back(const_cast<char*>(arg.c_str()));
        }

        args.push_back(nullptr);

        // Execute the command
        execve(process_info.command.c_str(), args.data(), environ);

        // If execve returns, it failed
        Logger::error("execve failed for: " + process_info.command);
        exit(1);

    } else if (pid > 0) {
        // Parent process
        out_pid = pid;
        return true;

    } else {
        // Fork failed
        Logger::error("fork() failed");
        return false;
    }
}

bool ProcessManager::terminateProcess(pid_t pid, bool force) {
    int signal = force ? SIGKILL : SIGTERM;

    if (kill(pid, signal) != 0) {
        Logger::error("Failed to send signal " + std::to_string(signal) + " to PID " + std::to_string(pid));
        return false;
    }

    // If using SIGTERM, wait a bit and then SIGKILL if still alive
    if (!force) {
        std::this_thread::sleep_for(std::chrono::seconds(5));

        if (isProcessAlive(pid)) {
            Logger::warn("Process " + std::to_string(pid) + " did not respond to SIGTERM, sending SIGKILL");
            kill(pid, SIGKILL);
        }
    }

    return true;
}

bool ProcessManager::isProcessAlive(pid_t pid) const {
    return kill(pid, 0) == 0;
}

int ProcessManager::waitForProcess(pid_t pid, bool blocking) {
    int status;
    int options = blocking ? 0 : WNOHANG;

    pid_t result = waitpid(pid, &status, options);

    if (result > 0 && WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }

    return -1;
}

void ProcessManager::emitEvent(const std::string& process_id, const std::string& event, const json& data) {
    if (event_callback_) {
        try {
            event_callback_(process_id, event, data);
        } catch (const std::exception& e) {
            Logger::error("Event callback error: " + std::string(e.what()));
        }
    }
}

} // namespace microservice
