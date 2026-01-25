/**
 * Generic C++ Microservice Template
 * 
 * A production-ready microservice shell with:
 * - HTTP REST API Server
 * - MQTT Publisher/Subscriber
 * - Service lifecycle management
 * - Health monitoring
 * - Logging and metrics
 * 
 * Use this template to implement any microservice logic
 */

#include "microservice/services/service_manager.h"
#include "microservice/services/api_server.h"
#include "microservice/services/mqtt_publisher.h"
#include "microservice/services/mqtt_subscriber.h"
#include "microservice/services/health_monitor.h"
#include "microservice/utils/config_manager.h"
#include "microservice/utils/logger.h"
#include "microservice/core/process_manager.h"
#include "microservice/core/process_manager_api.h"

#include <signal.h>
#include <iostream>
#include <memory>
#include <thread>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <httplib.h>  // Include AFTER our headers to avoid Logger conflict

using json = nlohmann::json;
// Explicitly use microservice:: prefix to avoid Logger conflict with httplib

// Global service manager for signal handling
std::shared_ptr<microservice::ServiceManager> g_service_manager;

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        microservice::Logger::info("Received shutdown signal");
        if (g_service_manager) {
            g_service_manager->requestShutdown();
        }
    }
}

/**
 * @brief Setup API Server with ServiceManager integration
 * 
 * This function connects the API endpoints to ServiceManager methods
 */
void setupApiEndpoints(std::shared_ptr<microservice::ApiServer> api_server, 
                       std::shared_ptr<microservice::ServiceManager> service_manager) {
    
    using namespace httplib;
    
    // === Service Lifecycle Endpoints ===
    
    // POST /api/v1/service/start - Start the service
    api_server->registerPostRoute("/service/start", 
        [service_manager](const Request& req, Response& res) {
            bool success = service_manager->handleStartRequest();
            json response = {
                {"success", success},
                {"message", success ? "Service started successfully" : "Failed to start service"},
                {"state", service_manager->getStateString()}
            };
            res.set_content(response.dump(), "application/json");
        });
    
    // POST /api/v1/service/stop - Stop the service
    api_server->registerPostRoute("/service/stop",
        [service_manager](const Request& req, Response& res) {
            bool success = service_manager->handleStopRequest();
            json response = {
                {"success", success},
                {"message", success ? "Service stopped successfully" : "Failed to stop service"},
                {"state", service_manager->getStateString()}
            };
            res.set_content(response.dump(), "application/json");
        });
    
    // POST /api/v1/service/restart - Restart the service
    api_server->registerPostRoute("/service/restart",
        [service_manager](const Request& req, Response& res) {
            bool success = service_manager->handleRestartRequest();
            json response = {
                {"success", success},
                {"message", success ? "Service restarted successfully" : "Failed to restart service"},
                {"state", service_manager->getStateString()}
            };
            res.set_content(response.dump(), "application/json");
        });
    
    // POST /api/v1/service/reset - Reset the service (clear state)
    api_server->registerPostRoute("/service/reset",
        [service_manager](const Request& req, Response& res) {
            bool success = service_manager->handleResetRequest();
            json response = {
                {"success", success},
                {"message", success ? "Service reset successfully" : "Failed to reset service"},
                {"state", service_manager->getStateString()}
            };
            res.set_content(response.dump(), "application/json");
        });
    
    // GET /api/v1/service/status - Get service status
    api_server->registerGetRoute("/service/status",
        [service_manager](const Request& req, Response& res) {
            std::string status_json = service_manager->handleStatusRequest();
            res.set_content(status_json, "application/json");
        });
    
    // GET /api/v1/service/metrics - Get service metrics
    api_server->registerGetRoute("/service/metrics",
        [service_manager](const Request& req, Response& res) {
            std::string metrics_json = service_manager->handleMetricsRequest();
            res.set_content(metrics_json, "application/json");
        });
    
    // === Custom Endpoints (Add your own here) ===
    
    // Example: Custom hello endpoint
    api_server->registerGetRoute("/custom/hello",
        [](const Request& req, Response& res) {
            json response = {
                {"message", "Hello from your microservice!"},
                {"timestamp", std::time(nullptr)}
            };
            res.set_content(response.dump(), "application/json");
        });
    
    // Example: MQTT Publish Test endpoint
    api_server->registerPostRoute("/custom/mqtt/publish",
        [service_manager](const Request& req, Response& res) {
            try {
                auto body = json::parse(req.body);
                std::string topic = body.value("topic", "test/microservice/default");
                std::string message = body.value("message", "test message");
                
                auto mqtt_pub = service_manager->getMQTTPublisher();
                if (!mqtt_pub) {
                    res.status = 503;
                    json error = {{"error", "MQTT Publisher not available"}};
                    res.set_content(error.dump(), "application/json");
                    return;
                }
                
                bool success = mqtt_pub->publish(topic, message, 1, false);
                
                json response = {
                    {"success", success},
                    {"topic", topic},
                    {"message", message},
                    {"timestamp", std::time(nullptr)}
                };
                res.set_content(response.dump(), "application/json");
                
            } catch (const std::exception& e) {
                res.status = 400;
                json error = {{"error", e.what()}};
                res.set_content(error.dump(), "application/json");
            }
        });
    
    // Configuration Management endpoints
    api_server->registerGetRoute("/custom/config",
        [service_manager](const Request& req, Response& res) {
            auto config_mgr = service_manager->getConfigManager();
            if (!config_mgr) {
                res.status = 503;
                json error = {{"error", "ConfigManager not available"}};
                res.set_content(error.dump(), "application/json");
                return;
            }
            
            json response = {
                {"success", true},
                {"config", config_mgr->getConfig()},
                {"timestamp", std::time(nullptr)}
            };
            res.set_content(response.dump(2), "application/json");
        });
    
    api_server->registerGetRoute("/custom/state",
        [service_manager](const Request& req, Response& res) {
            auto config_mgr = service_manager->getConfigManager();
            if (!config_mgr) {
                res.status = 503;
                json error = {{"error", "ConfigManager not available"}};
                res.set_content(error.dump(), "application/json");
                return;
            }
            
            json response = {
                {"success", true},
                {"state", config_mgr->getState()},
                {"state_file", config_mgr->getStateFile()},
                {"timestamp", std::time(nullptr)}
            };
            res.set_content(response.dump(2), "application/json");
        });
    
    api_server->registerPostRoute("/custom/config/save",
        [service_manager](const Request& req, Response& res) {
            auto config_mgr = service_manager->getConfigManager();
            if (!config_mgr) {
                res.status = 503;
                json error = {{"error", "ConfigManager not available"}};
                res.set_content(error.dump(), "application/json");
                return;
            }
            
            bool success = config_mgr->save();
            json response = {
                {"success", success},
                {"message", success ? "Configuration saved" : "Failed to save configuration"},
                {"timestamp", std::time(nullptr)}
            };
            res.set_content(response.dump(), "application/json");
        });
    
    api_server->registerPostRoute("/custom/state/save",
        [service_manager](const Request& req, Response& res) {
            auto config_mgr = service_manager->getConfigManager();
            if (!config_mgr) {
                res.status = 503;
                json error = {{"error", "ConfigManager not available"}};
                res.set_content(error.dump(), "application/json");
                return;
            }
            
            bool success = config_mgr->saveState();
            json response = {
                {"success", success},
                {"message", success ? "State saved" : "Failed to save state"},
                {"state_file", config_mgr->getStateFile()},
                {"timestamp", std::time(nullptr)}
            };
            res.set_content(response.dump(), "application/json");
        });
    
    microservice::Logger::info("API endpoints registered successfully");
}

/**
 * @brief Setup MQTT message handlers
 */
void setupMqttHandlers(std::shared_ptr<microservice::MQTTSubscriber> mqtt_subscriber) {
    
    // Example: Subscribe to command topics
    mqtt_subscriber->subscribe("service/command/#", 
        [](const std::string& topic, const std::string& payload) {
            microservice::Logger::info("Received command: " + topic + " -> " + payload);
            
            // TODO: Implement your MQTT command handling here
            // Example:
            // if (topic == "service/command/start") {
            //     // Handle start command
            // }
        });
    
    // Example: Subscribe to data topics
    mqtt_subscriber->subscribe("device/+/data",
        [](const std::string& topic, const std::string& payload) {
            microservice::Logger::info("Received device data: " + topic);
            
            // TODO: Implement your data processing here
            // Example:
            // auto data = json::parse(payload);
            // processDeviceData(data);
        });
    
    microservice::Logger::info("MQTT handlers registered successfully");
}

/**
 * @brief Setup custom service logic callbacks
 */
void setupServiceCallbacks(std::shared_ptr<microservice::ServiceManager> service_manager) {
    
    // Custom start logic
    service_manager->setCustomStartLogic([]() -> bool {
        microservice::Logger::info("Executing custom start logic...");
        
        // TODO: Add your service initialization logic here
        // Example:
        // - Connect to database
        // - Load configurations
        // - Initialize business logic modules
        
        return true;  // Return true if successful
    });
    
    // Custom stop logic
    service_manager->setCustomStopLogic([]() -> bool {
        microservice::Logger::info("Executing custom stop logic...");
        
        // TODO: Add your cleanup logic here
        // Example:
        // - Close database connections
        // - Save state
        // - Cleanup resources
        
        return true;
    });
    
    // Custom restart logic
    service_manager->setCustomRestartLogic([]() -> bool {
        microservice::Logger::info("Executing custom restart logic...");
        
        // TODO: Add your restart-specific logic here
        
        return true;
    });
    
    // Custom reset logic
    service_manager->setCustomResetLogic([]() -> bool {
        microservice::Logger::info("Executing custom reset logic...");
        
        // TODO: Add your reset logic here
        // Example:
        // - Clear caches
        // - Reset counters
        // - Reinitialize state
        
        return true;
    });
    
    // Custom status logic
    service_manager->setCustomStatusLogic([]() -> std::string {
        // TODO: Add your custom status information here
        json custom_status = {
            {"custom_field_1", "value"},
            {"custom_field_2", 123},
            {"custom_timestamp", std::time(nullptr)}
        };
        return custom_status.dump();
    });
    
    microservice::Logger::info("Service callbacks registered successfully");
}

int main(int argc, char* argv[]) {
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    microservice::Logger::info("========================================");
    microservice::Logger::info("  C++ Microservice Template Starting");
    microservice::Logger::info("========================================");
    
    try {
        // === 1. Create Service Components ===
        
        auto service_manager = std::make_shared<microservice::ServiceManager>();
        auto api_server = std::make_shared<microservice::ApiServer>();
        auto mqtt_publisher = std::make_shared<microservice::MQTTPublisher>();
        auto mqtt_subscriber = std::make_shared<microservice::MQTTSubscriber>();
        auto health_monitor = std::make_shared<microservice::HealthMonitor>();
        auto process_manager = std::make_shared<microservice::ProcessManager>();

        g_service_manager = service_manager;  // For signal handling
        
        // === 2. Initialize Components ===
        
        microservice::Logger::info("Initializing components...");
        
        // Initialize API Server
        if (!api_server->initialize(8080, "0.0.0.0")) {
            microservice::Logger::error("Failed to initialize API server");
            return 1;
        }
        
        // Initialize MQTT (read from environment variables or use defaults)
        const char* mqtt_host_env = std::getenv("MQTT_BROKER_HOST");
        const char* mqtt_port_env = std::getenv("MQTT_BROKER_PORT");
        
        std::string mqtt_host = mqtt_host_env ? mqtt_host_env : "localhost";
        int mqtt_port = mqtt_port_env ? std::atoi(mqtt_port_env) : 1883;
        
        microservice::Logger::info("MQTT Config: " + mqtt_host + ":" + std::to_string(mqtt_port));
        
        mqtt_publisher->initialize(mqtt_host, mqtt_port, "microservice_pub");
        mqtt_subscriber->initialize(mqtt_host, mqtt_port, "microservice_sub");
        
        // Initialize Health Monitor
        if (!health_monitor->initialize()) {
            microservice::Logger::error("Failed to initialize health monitor");
            return 1;
        }
        
        // === 3. Register Components with ServiceManager ===
        
        service_manager->setApiServer(api_server);
        service_manager->setMQTTPublisher(mqtt_publisher);
        service_manager->setMQTTSubscriber(mqtt_subscriber);
        service_manager->setHealthMonitor(health_monitor);
        
        // === 4. Setup Integrations ===

        setupApiEndpoints(api_server, service_manager);
        setupProcessManagerEndpoints(api_server, process_manager);
        setupServiceCallbacks(service_manager);
        
        // === 5. Initialize Service Manager ===
        
        if (!service_manager->initialize("configs/service_config.json")) {
            microservice::Logger::error("Failed to initialize service manager");
            return 1;
        }
        
        // === 6. Start Services ===
        
        microservice::Logger::info("Starting services...");
        
        if (!service_manager->start()) {
            microservice::Logger::error("Failed to start service manager");
            return 1;
        }
        
        // === 7. Setup MQTT Handlers (AFTER MQTT is connected) ===
        setupMqttHandlers(mqtt_subscriber);
        
        microservice::Logger::info("=== Microservice Running ===");
        microservice::Logger::info("API Server: http://localhost:8080");
        microservice::Logger::info("Press Ctrl+C to stop");
        
        // === 8. Main Loop ===

        while (!service_manager->isShutdownRequested()) {
            // Check health of managed processes
            process_manager->checkAllProcessHealth();

            // TODO: Implement your main business logic here
            // Example:
            // - Process queued tasks
            // - Run periodic operations

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // === 9. Shutdown ===

        microservice::Logger::info("Shutting down microservice...");

        // Stop all managed processes
        microservice::Logger::info("Stopping all managed processes...");
        auto process_ids = process_manager->listProcesses();
        for (const auto& process_id : process_ids) {
            auto proc_info = process_manager->getProcess(process_id);
            if (proc_info && proc_info->is_running) {
                process_manager->stopProcess(process_id, false);
            }
        }

        service_manager->shutdown();
        
        microservice::Logger::info("Microservice exited successfully");
        return 0;
        
    } catch (const std::exception& e) {
        microservice::Logger::error("Fatal error: " + std::string(e.what()));
        return 1;
    }
}
