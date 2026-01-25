#pragma once

#include <memory>

namespace microservice {

// Forward declarations
class ApiServer;
class ProcessManager;

/**
 * @brief Setup Process Manager API endpoints
 *
 * Registers all RESTful API endpoints for process management
 *
 * @param api_server Shared pointer to API server
 * @param process_manager Shared pointer to Process Manager
 */
void setupProcessManagerEndpoints(std::shared_ptr<ApiServer> api_server,
                                   std::shared_ptr<ProcessManager> process_manager);

} // namespace microservice
