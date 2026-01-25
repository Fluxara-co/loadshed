/**
 * Process Manager Usage Example
 *
 * Demonstrates how to use the Process Manager for lifecycle management
 */

#include "microservice/core/process_manager.h"
#include "microservice/core/process_manager_api.h"
#include "microservice/services/api_server.h"
#include "microservice/utils/logger.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>

using namespace microservice;

volatile bool g_shutdown = false;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        Logger::info("Shutdown signal received");
        g_shutdown = true;
    }
}

int main(int argc, char* argv[]) {
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Logger::info("========================================");
    Logger::info("Process Manager Example");
    Logger::info("========================================");

    // Create Process Manager
    auto process_manager = std::make_shared<ProcessManager>();

    // Create API Server
    auto api_server = std::make_shared<ApiServer>();
    if (!api_server->initialize(8080, "0.0.0.0")) {
        Logger::error("Failed to initialize API server");
        return 1;
    }

    // Setup Process Manager API endpoints
    setupProcessManagerEndpoints(api_server, process_manager);

    // Start API server
    if (!api_server->start()) {
        Logger::error("Failed to start API server");
        return 1;
    }

    Logger::info("API Server running on http://localhost:8080");
    Logger::info("========================================");

    // ========================================================================
    // Example 1: Add a simple process (sleep command)
    // ========================================================================

    Logger::info("\n=== Example 1: Add simple process ===");

    ProcessInfo sleep_process;
    sleep_process.name = "Sleep Process";
    sleep_process.description = "Simple sleep command for testing";
    sleep_process.type = ProcessType::EXECUTABLE;
    sleep_process.command = "/bin/sleep";
    sleep_process.args = {"30"};
    sleep_process.restart_policy.auto_restart = true;
    sleep_process.restart_policy.max_restart_attempts = 3;
    sleep_process.tags["environment"] = "test";
    sleep_process.tags["type"] = "demo";

    if (process_manager->addProcess("sleep-001", sleep_process)) {
        Logger::info("Process added: sleep-001");
    }

    // ========================================================================
    // Example 2: Add a process from JSON
    // ========================================================================

    Logger::info("\n=== Example 2: Add process from JSON ===");

    json echo_config = {
        {"name", "Echo Process"},
        {"description", "Echo command for testing"},
        {"type", "executable"},
        {"command", "/bin/echo"},
        {"args", {"Hello", "from", "Process", "Manager"}},
        {"restart_policy", {
            {"auto_restart", false},
            {"max_retries", 0}
        }},
        {"metadata", {
            {"tags", {
                {"environment", "test"},
                {"type", "demo"}
            }}
        }}
    };

    if (process_manager->addProcessFromJson("echo-001", echo_config)) {
        Logger::info("Process added from JSON: echo-001");
    }

    // ========================================================================
    // Example 3: Start processes
    // ========================================================================

    Logger::info("\n=== Example 3: Start processes ===");

    if (process_manager->startProcess("sleep-001")) {
        Logger::info("Started: sleep-001");
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (process_manager->startProcess("echo-001")) {
        Logger::info("Started: echo-001");
    }

    // ========================================================================
    // Example 4: List all processes
    // ========================================================================

    Logger::info("\n=== Example 4: List all processes ===");

    auto processes = process_manager->listProcessStatuses();
    Logger::info("Total processes: " + std::to_string(processes.size()));

    for (const auto& proc : processes) {
        Logger::info("  - " + proc.process_id + " (" + proc.name + "): " +
                    (proc.is_running ? "RUNNING" : "STOPPED") +
                    " [PID: " + std::to_string(proc.pid) + "]");
    }

    // ========================================================================
    // Example 5: Check process health
    // ========================================================================

    Logger::info("\n=== Example 5: Health monitoring ===");

    // Wait a bit for echo to complete
    std::this_thread::sleep_for(std::chrono::seconds(2));

    for (const auto& proc_id : process_manager->listProcesses()) {
        bool healthy = process_manager->checkProcessHealth(proc_id);
        Logger::info("  - " + proc_id + ": " + (healthy ? "HEALTHY" : "UNHEALTHY"));
    }

    // ========================================================================
    // Example 6: Query operations
    // ========================================================================

    Logger::info("\n=== Example 6: Query operations ===");

    auto running = process_manager->getRunningProcesses();
    Logger::info("Running processes: " + std::to_string(running.size()));
    for (const auto& pid : running) {
        Logger::info("  - " + pid);
    }

    auto stopped = process_manager->getStoppedProcesses();
    Logger::info("Stopped processes: " + std::to_string(stopped.size()));
    for (const auto& pid : stopped) {
        Logger::info("  - " + pid);
    }

    auto demo_tagged = process_manager->findProcessesByTag("type", "demo");
    Logger::info("Processes tagged 'demo': " + std::to_string(demo_tagged.size()));

    // ========================================================================
    // Example 7: Statistics
    // ========================================================================

    Logger::info("\n=== Example 7: Statistics ===");

    auto stats = process_manager->getStats();
    Logger::info("Process Manager Stats:");
    Logger::info("  Total processes: " + std::to_string(stats.total_processes));
    Logger::info("  Running: " + std::to_string(stats.running_processes));
    Logger::info("  Stopped: " + std::to_string(stats.stopped_processes));
    Logger::info("  Failed: " + std::to_string(stats.failed_processes));
    Logger::info("  Total starts: " + std::to_string(stats.total_starts));
    Logger::info("  Total stops: " + std::to_string(stats.total_stops));
    Logger::info("  Total restarts: " + std::to_string(stats.total_restarts));

    // ========================================================================
    // Example 8: Event callbacks
    // ========================================================================

    Logger::info("\n=== Example 8: Event callbacks ===");

    process_manager->setEventCallback(
        [](const std::string& process_id, const std::string& event, const json& data) {
            Logger::info("EVENT: " + process_id + " - " + event +
                        (data.empty() ? "" : " - " + data.dump()));
        }
    );

    // ========================================================================
    // API Endpoint Examples
    // ========================================================================

    Logger::info("\n========================================");
    Logger::info("API Endpoints Available:");
    Logger::info("========================================");
    Logger::info("CRUD:");
    Logger::info("  POST   /api/v1/processes           - Create process");
    Logger::info("  GET    /api/v1/processes           - List all processes");
    Logger::info("  GET    /api/v1/processes/:id       - Get process");
    Logger::info("  PUT    /api/v1/processes/:id       - Update process");
    Logger::info("  DELETE /api/v1/processes/:id       - Delete process");
    Logger::info("");
    Logger::info("Lifecycle:");
    Logger::info("  POST   /api/v1/processes/:id/start - Start process");
    Logger::info("  POST   /api/v1/processes/:id/stop  - Stop process");
    Logger::info("  POST   /api/v1/processes/:id/restart - Restart process");
    Logger::info("  POST   /api/v1/processes/:id/kill  - Kill process");
    Logger::info("");
    Logger::info("Health & Status:");
    Logger::info("  GET    /api/v1/processes/:id/status - Get status");
    Logger::info("  POST   /api/v1/processes/:id/health - Health check");
    Logger::info("");
    Logger::info("Bulk Operations:");
    Logger::info("  POST   /api/v1/processes/bulk/start - Start multiple");
    Logger::info("  POST   /api/v1/processes/bulk/stop  - Stop multiple");
    Logger::info("");
    Logger::info("Query:");
    Logger::info("  GET    /api/v1/processes/search    - Search by tag");
    Logger::info("  GET    /api/v1/processes/running   - Get running");
    Logger::info("  GET    /api/v1/processes/stopped   - Get stopped");
    Logger::info("  GET    /api/v1/processes/stats     - Get statistics");
    Logger::info("========================================");

    Logger::info("\nTry these curl commands:");
    Logger::info("  curl http://localhost:8080/api/v1/processes");
    Logger::info("  curl http://localhost:8080/api/v1/processes/sleep-001");
    Logger::info("  curl http://localhost:8080/api/v1/processes/stats");
    Logger::info("  curl http://localhost:8080/api/v1/processes/running");
    Logger::info("");

    // ========================================================================
    // Main loop with health monitoring
    // ========================================================================

    Logger::info("Process Manager running. Press Ctrl+C to stop.");

    while (!g_shutdown) {
        // Check health periodically
        process_manager->checkAllProcessHealth();

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    // ========================================================================
    // Cleanup
    // ========================================================================

    Logger::info("\nShutting down...");

    // Stop API server
    api_server->stop();

    // Stop all processes
    process_manager->stopAllProcesses();

    Logger::info("Process Manager Example completed");

    return 0;
}
