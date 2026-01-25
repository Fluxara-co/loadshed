/**
 * Process Manager API Endpoints
 *
 * RESTful API for process lifecycle management
 * Adapted from device-modbus-service DeviceApiServer
 */

#include "microservice/core/process_manager.h"
#include "microservice/services/api_server.h"
#include "microservice/utils/logger.h"
#include <httplib.h>
#include <nlohmann/json.hpp>

namespace microservice {

using json = nlohmann::json;
using namespace httplib;

/**
 * @brief Setup Process Manager API endpoints
 *
 * Registers all CRUD and lifecycle endpoints for process management
 */
void setupProcessManagerEndpoints(std::shared_ptr<ApiServer> api_server,
                                   std::shared_ptr<ProcessManager> process_manager) {

    if (!api_server || !process_manager) {
        Logger::error("Invalid API server or Process Manager");
        return;
    }

    // ==================================================================
    // CRUD Endpoints
    // ==================================================================

    // POST /api/v1/processes - Create a new process
    api_server->registerPostRoute("/processes",
        [process_manager](const Request& req, Response& res) {
            try {
                auto body = json::parse(req.body);

                std::string process_id = body.value("process_id", "");
                if (process_id.empty()) {
                    res.status = 400;
                    json error = {{"error", "process_id is required"}};
                    res.set_content(error.dump(), "application/json");
                    return;
                }

                bool success = process_manager->addProcessFromJson(process_id, body);

                json response = {
                    {"success", success},
                    {"message", success ? "Process created" : "Failed to create process"},
                    {"process_id", process_id}
                };

                res.status = success ? 201 : 400;
                res.set_content(response.dump(), "application/json");

            } catch (const std::exception& e) {
                res.status = 400;
                json error = {{"error", e.what()}};
                res.set_content(error.dump(), "application/json");
            }
        });

    // GET /api/v1/processes - List all processes
    api_server->registerGetRoute("/processes",
        [process_manager](const Request& req, Response& res) {
            auto statuses = process_manager->listProcessStatuses();

            json process_list = json::array();
            for (const auto& status : statuses) {
                process_list.push_back(status.to_json());
            }

            json response = {
                {"success", true},
                {"count", process_list.size()},
                {"processes", process_list}
            };

            res.set_content(response.dump(2), "application/json");
        });

    // ==================================================================
    // Bulk Operations (MUST be before /processes/:id)
    // ==================================================================

    // POST /api/v1/processes/bulk/start - Start multiple processes
    api_server->registerPostRoute("/processes/bulk/start",
        [process_manager](const Request& req, Response& res) {
            try {
                auto body = json::parse(req.body);

                if (!body.contains("process_ids") || !body["process_ids"].is_array()) {
                    res.status = 400;
                    json error = {{"error", "process_ids array required"}};
                    res.set_content(error.dump(), "application/json");
                    return;
                }

                std::vector<std::string> process_ids = body["process_ids"].get<std::vector<std::string>>();

                auto results = process_manager->startProcesses(process_ids);

                json response = {
                    {"success", true},
                    {"results", results}
                };

                res.set_content(response.dump(), "application/json");

            } catch (const std::exception& e) {
                res.status = 400;
                json error = {{"error", e.what()}};
                res.set_content(error.dump(), "application/json");
            }
        });

    // POST /api/v1/processes/bulk/stop - Stop multiple processes
    api_server->registerPostRoute("/processes/bulk/stop",
        [process_manager](const Request& req, Response& res) {
            try {
                auto body = json::parse(req.body);

                if (!body.contains("process_ids") || !body["process_ids"].is_array()) {
                    res.status = 400;
                    json error = {{"error", "process_ids array required"}};
                    res.set_content(error.dump(), "application/json");
                    return;
                }

                std::vector<std::string> process_ids = body["process_ids"].get<std::vector<std::string>>();
                bool force = body.value("force", false);

                auto results = process_manager->stopProcesses(process_ids, force);

                json response = {
                    {"success", true},
                    {"results", results},
                    {"force", force}
                };

                res.set_content(response.dump(), "application/json");

            } catch (const std::exception& e) {
                res.status = 400;
                json error = {{"error", e.what()}};
                res.set_content(error.dump(), "application/json");
            }
        });

    // ==================================================================
    // Query Endpoints (MUST be before /processes/:id)
    // ==================================================================

    // GET /api/v1/processes/search - Search processes by tag
    api_server->registerGetRoute("/processes/search",
        [process_manager](const Request& req, Response& res) {
            std::string tag_key = req.has_param("tag_key") ? req.get_param_value("tag_key") : "";
            std::string tag_value = req.has_param("tag_value") ? req.get_param_value("tag_value") : "";

            if (tag_key.empty()) {
                res.status = 400;
                json error = {{"error", "tag_key parameter required"}};
                res.set_content(error.dump(), "application/json");
                return;
            }

            auto process_ids = process_manager->findProcessesByTag(tag_key, tag_value);

            json response = {
                {"success", true},
                {"tag_key", tag_key},
                {"tag_value", tag_value},
                {"count", process_ids.size()},
                {"process_ids", process_ids}
            };

            res.set_content(response.dump(), "application/json");
        });

    // GET /api/v1/processes/running - Get all running processes
    api_server->registerGetRoute("/processes/running",
        [process_manager](const Request& req, Response& res) {
            auto process_ids = process_manager->getRunningProcesses();

            json response = {
                {"success", true},
                {"count", process_ids.size()},
                {"running_processes", process_ids}
            };

            res.set_content(response.dump(), "application/json");
        });

    // GET /api/v1/processes/stopped - Get all stopped processes
    api_server->registerGetRoute("/processes/stopped",
        [process_manager](const Request& req, Response& res) {
            auto process_ids = process_manager->getStoppedProcesses();

            json response = {
                {"success", true},
                {"count", process_ids.size()},
                {"stopped_processes", process_ids}
            };

            res.set_content(response.dump(), "application/json");
        });

    // ==================================================================
    // Statistics Endpoint (MUST be before /processes/:id)
    // ==================================================================

    // GET /api/v1/processes/stats - Get process manager statistics
    api_server->registerGetRoute("/processes/stats",
        [process_manager](const Request& req, Response& res) {
            auto stats = process_manager->getStats();

            json response = {
                {"success", true},
                {"stats", {
                    {"total_processes", stats.total_processes},
                    {"running_processes", stats.running_processes},
                    {"stopped_processes", stats.stopped_processes},
                    {"failed_processes", stats.failed_processes},
                    {"total_restarts", stats.total_restarts},
                    {"total_starts", stats.total_starts},
                    {"total_stops", stats.total_stops}
                }}
            };

            res.set_content(response.dump(2), "application/json");
        });

    // GET /api/v1/processes/:id - Get specific process
    api_server->registerGetRoute("/processes/:id",
        [process_manager](const Request& req, Response& res) {
            std::string process_id = req.path_params.at("id");

            auto status = process_manager->getProcessStatus(process_id);

            if (!status.has_value()) {
                res.status = 404;
                json error = {
                    {"error", "Process not found"},
                    {"process_id", process_id}
                };
                res.set_content(error.dump(), "application/json");
                return;
            }

            json response = {
                {"success", true},
                {"process", status->to_json()}
            };

            res.set_content(response.dump(2), "application/json");
        });

    // PUT /api/v1/processes/:id - Update process (must be stopped)
    api_server->registerPutRoute("/processes/:id",
        [process_manager](const Request& req, Response& res) {
            try {
                std::string process_id = req.path_params.at("id");
                auto body = json::parse(req.body);

                // Get current process
                auto current_opt = process_manager->getProcess(process_id);
                if (!current_opt.has_value()) {
                    res.status = 404;
                    json error = {{"error", "Process not found"}};
                    res.set_content(error.dump(), "application/json");
                    return;
                }

                // Parse update (simplified - just update from JSON)
                ProcessInfo updated;
                // Copy existing fields and update from JSON
                // TODO: Implement full update logic

                bool success = process_manager->updateProcess(process_id, updated);

                json response = {
                    {"success", success},
                    {"message", success ? "Process updated" : "Failed to update process"},
                    {"process_id", process_id}
                };

                res.status = success ? 200 : 400;
                res.set_content(response.dump(), "application/json");

            } catch (const std::exception& e) {
                res.status = 400;
                json error = {{"error", e.what()}};
                res.set_content(error.dump(), "application/json");
            }
        });

    // DELETE /api/v1/processes/:id - Delete process
    api_server->registerDeleteRoute("/processes/:id",
        [process_manager](const Request& req, Response& res) {
            std::string process_id = req.path_params.at("id");

            bool success = process_manager->removeProcess(process_id);

            json response = {
                {"success", success},
                {"message", success ? "Process deleted" : "Process not found"},
                {"process_id", process_id}
            };

            res.status = success ? 200 : 404;
            res.set_content(response.dump(), "application/json");
        });

    // ==================================================================
    // Lifecycle Endpoints
    // ==================================================================

    // POST /api/v1/processes/:id/start - Start process
    api_server->registerPostRoute("/processes/:id/start",
        [process_manager](const Request& req, Response& res) {
            std::string process_id = req.path_params.at("id");

            bool success = process_manager->startProcess(process_id);

            auto status = process_manager->getProcessStatus(process_id);

            json response = {
                {"success", success},
                {"message", success ? "Process started" : "Failed to start process"},
                {"process_id", process_id}
            };

            if (status.has_value()) {
                response["pid"] = status->pid;
                response["status"] = status->to_json();
            }

            res.status = success ? 200 : 400;
            res.set_content(response.dump(), "application/json");
        });

    // POST /api/v1/processes/:id/stop - Stop process
    api_server->registerPostRoute("/processes/:id/stop",
        [process_manager](const Request& req, Response& res) {
            std::string process_id = req.path_params.at("id");

            // Check for force parameter
            bool force = req.has_param("force") && req.get_param_value("force") == "true";

            bool success = process_manager->stopProcess(process_id, force);

            json response = {
                {"success", success},
                {"message", success ? "Process stopped" : "Failed to stop process"},
                {"process_id", process_id},
                {"force", force}
            };

            res.set_content(response.dump(), "application/json");
        });

    // POST /api/v1/processes/:id/restart - Restart process
    api_server->registerPostRoute("/processes/:id/restart",
        [process_manager](const Request& req, Response& res) {
            std::string process_id = req.path_params.at("id");

            bool success = process_manager->restartProcess(process_id);

            auto status = process_manager->getProcessStatus(process_id);

            json response = {
                {"success", success},
                {"message", success ? "Process restarted" : "Failed to restart process"},
                {"process_id", process_id}
            };

            if (status.has_value()) {
                response["restart_count"] = status->restart_count;
                response["status"] = status->to_json();
            }

            res.status = success ? 200 : 400;
            res.set_content(response.dump(), "application/json");
        });

    // POST /api/v1/processes/:id/kill - Force kill process
    api_server->registerPostRoute("/processes/:id/kill",
        [process_manager](const Request& req, Response& res) {
            std::string process_id = req.path_params.at("id");

            bool success = process_manager->killProcess(process_id);

            json response = {
                {"success", success},
                {"message", success ? "Process killed" : "Failed to kill process"},
                {"process_id", process_id}
            };

            res.set_content(response.dump(), "application/json");
        });

    // ==================================================================
    // Health & Status Endpoints
    // ==================================================================

    // GET /api/v1/processes/:id/status - Get process status
    api_server->registerGetRoute("/processes/:id/status",
        [process_manager](const Request& req, Response& res) {
            std::string process_id = req.path_params.at("id");

            auto status = process_manager->getProcessStatus(process_id);

            if (!status.has_value()) {
                res.status = 404;
                json error = {{"error", "Process not found"}};
                res.set_content(error.dump(), "application/json");
                return;
            }

            json response = {
                {"success", true},
                {"process_id", process_id},
                {"status", status->to_json()}
            };

            res.set_content(response.dump(2), "application/json");
        });

    // POST /api/v1/processes/:id/health - Check process health
    api_server->registerPostRoute("/processes/:id/health",
        [process_manager](const Request& req, Response& res) {
            std::string process_id = req.path_params.at("id");

            bool healthy = process_manager->checkProcessHealth(process_id);

            auto status = process_manager->getProcessStatus(process_id);

            json response = {
                {"success", true},
                {"process_id", process_id},
                {"healthy", healthy}
            };

            if (status.has_value()) {
                response["status"] = status->to_json();
            }

            res.set_content(response.dump(), "application/json");
        });

    Logger::info("Process Manager API endpoints registered successfully");
}

} // namespace microservice
