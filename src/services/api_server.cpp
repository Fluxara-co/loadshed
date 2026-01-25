/**
 * Generic API Server Implementation
 * Provides basic service lifecycle endpoints and custom route registration
 */

#include "microservice/services/api_server.h"
#include "microservice/utils/logger.h"
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <chrono>
#include <mutex>
#include <map>

using json = nlohmann::json;

namespace microservice {

// ========================================================================
// ApiServer::Impl - Private Implementation
// ========================================================================

class ApiServer::Impl {
public:
    Impl() : server_(nullptr), port_(8080), host_("0.0.0.0"), 
             api_prefix_("/api/v1"), cors_enabled_(true), api_key_(""), 
             running_(false), initialized_(false) {}
    
    ~Impl() {
        if (server_) {
            server_->stop();
        }
    }
    
    // HTTP Server
    std::unique_ptr<httplib::Server> server_;
    
    // Configuration
    uint16_t port_;
    std::string host_;
    std::string api_prefix_;
    bool cors_enabled_;
    std::string api_key_;
    bool running_;
    bool initialized_;
    
    // Service Callbacks
    std::function<bool()> start_callback_;
    std::function<bool()> stop_callback_;
    std::function<bool()> restart_callback_;
    std::function<bool()> reset_callback_;
    std::function<std::string()> status_callback_;
    
    // Custom routes
    struct RouteInfo {
        std::string method;
        std::string path;
        ApiServer::RouteHandler handler;
    };
    std::vector<RouteInfo> custom_routes_;
    
    // Statistics
    mutable std::mutex stats_mutex_;
    ServerStats stats_;
    
    // Registered endpoints for listing
    std::vector<std::string> registered_endpoints_;
    
    // Helper methods
    void addCorsHeaders(httplib::Response& res) {
        if (cors_enabled_) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, X-API-Key");
        }
    }
    
    std::string createJsonResponse(bool success, const std::string& message, const std::string& data = "{}") {
        json response = {
            {"success", success},
            {"message", message},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()
            }
        };
        
        if (!data.empty() && data != "{}") {
            try {
                response["data"] = json::parse(data);
            } catch (...) {
                response["data"] = data;
            }
        }
        
        return response.dump(2);
    }
    
    std::string createErrorResponse(const std::string& error, int status_code = 400) {
        json response = {
            {"success", false},
            {"error", error},
            {"status_code", status_code},
            {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()
            }
        };
        return response.dump(2);
    }
    
    std::string createSuccessResponse(const std::string& message, const std::string& data = "{}") {
        return createJsonResponse(true, message, data);
    }
    
    void logRequest(const std::string& endpoint, const std::string& method, int status_code, 
                   std::chrono::milliseconds response_time) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        stats_.total_requests++;
        if (status_code >= 200 && status_code < 400) {
            stats_.successful_requests++;
        } else {
            stats_.failed_requests++;
        }
        
        stats_.last_request_time = std::chrono::steady_clock::now();
        stats_.endpoint_counts[endpoint]++;
        
        // Update response time statistics
        if (response_time < stats_.fastest_response_time) {
            stats_.fastest_response_time = response_time;
        }
        if (response_time > stats_.slowest_response_time) {
            stats_.slowest_response_time = response_time;
        }
        
        // Calculate running average
        if (stats_.total_requests > 0) {
            auto total_time = stats_.average_response_time * (stats_.total_requests - 1) + response_time;
            stats_.average_response_time = total_time / stats_.total_requests;
        }
        
        Logger::debug("API Request: " + method + " " + endpoint + " -> " + 
                     std::to_string(status_code) + " (" + 
                     std::to_string(response_time.count()) + "ms)");
    }
    
    bool validateApiKey(const httplib::Request& req) {
        if (api_key_.empty()) return true; // No API key required
        
        auto api_key_header = req.get_header_value("X-API-Key");
        if (!api_key_header.empty() && api_key_header == api_key_) {
            return true;
        }
        
        auto auth_header = req.get_header_value("Authorization");
        if (!auth_header.empty() && auth_header == "Bearer " + api_key_) {
            return true;
        }
        
        return false;
    }
};

// ========================================================================
// ApiServer - Public Interface
// ========================================================================

ApiServer::ApiServer() : pImpl(std::make_unique<Impl>()) {
    pImpl->stats_.start_time = std::chrono::steady_clock::now();
}

ApiServer::~ApiServer() = default;

// Server lifecycle
bool ApiServer::initialize(uint16_t port, const std::string& host) {
    if (pImpl->initialized_) {
        Logger::warn("API Server already initialized");
        return true;
    }
    
    pImpl->port_ = port;
    pImpl->host_ = host;
    
    // Create HTTP server
    pImpl->server_ = std::make_unique<httplib::Server>();
    
    // Setup default endpoints
    setupDefaultEndpoints();
    
    pImpl->initialized_ = true;
    Logger::info("API Server initialized on " + host + ":" + std::to_string(port));
    return true;
}

bool ApiServer::start() {
    if (!pImpl->initialized_) {
        Logger::error("API Server not initialized");
        return false;
    }
    
    if (pImpl->running_) {
        Logger::warn("API Server already running");
        return true;
    }
    
    Logger::info("Starting API Server on " + pImpl->host_ + ":" + std::to_string(pImpl->port_));
    
    // Start server in separate thread (non-blocking)
    pImpl->running_ = true;
    
    std::thread([this]() {
        bool started = pImpl->server_->listen(pImpl->host_.c_str(), pImpl->port_);
        if (!started) {
            Logger::error("Failed to start API Server");
            pImpl->running_ = false;
        }
    }).detach();
    
    // Give server time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    if (pImpl->running_) {
        Logger::info("API Server started successfully");
        pImpl->stats_.start_time = std::chrono::steady_clock::now();
    }
    
    return pImpl->running_;
}

bool ApiServer::stop() {
    if (!pImpl->running_) {
        return true;
    }
    
    Logger::info("Stopping API Server...");
    pImpl->server_->stop();
    pImpl->running_ = false;
    Logger::info("API Server stopped");
    return true;
}

bool ApiServer::isRunning() const {
    return pImpl->running_;
}

void ApiServer::shutdown() {
    stop();
}

// Configuration
void ApiServer::setPort(uint16_t port) { pImpl->port_ = port; }
void ApiServer::setHost(const std::string& host) { pImpl->host_ = host; }
void ApiServer::enableCors(bool enable) { pImpl->cors_enabled_ = enable; }
void ApiServer::setApiPrefix(const std::string& prefix) { pImpl->api_prefix_ = prefix; }
void ApiServer::setApiKey(const std::string& api_key) { pImpl->api_key_ = api_key; }

// Status
uint16_t ApiServer::getPort() const { return pImpl->port_; }
std::string ApiServer::getHost() const { return pImpl->host_; }
std::string ApiServer::getApiPrefix() const { return pImpl->api_prefix_; }
bool ApiServer::isCorsEnabled() const { return pImpl->cors_enabled_; }

// Statistics
ApiServer::ServerStats ApiServer::getStats() const {
    std::lock_guard<std::mutex> lock(pImpl->stats_mutex_);
    return pImpl->stats_;
}

void ApiServer::resetStats() {
    std::lock_guard<std::mutex> lock(pImpl->stats_mutex_);
    pImpl->stats_ = ServerStats{};
    pImpl->stats_.start_time = std::chrono::steady_clock::now();
}

// Service callbacks
void ApiServer::setStartCallback(ServiceCallback callback) {
    pImpl->start_callback_ = callback;
}

void ApiServer::setStopCallback(ServiceCallback callback) {
    pImpl->stop_callback_ = callback;
}

void ApiServer::setRestartCallback(ServiceCallback callback) {
    pImpl->restart_callback_ = callback;
}

void ApiServer::setResetCallback(ServiceCallback callback) {
    pImpl->reset_callback_ = callback;
}

void ApiServer::setStatusCallback(std::function<std::string()> callback) {
    pImpl->status_callback_ = callback;
}

// Custom route registration
void ApiServer::registerRoute(const std::string& method, const std::string& path, RouteHandler handler) {
    if (!pImpl->server_) {
        Logger::error("Cannot register route: server not initialized");
        return;
    }
    
    std::string full_path = pImpl->api_prefix_ + path;
    
    // Wrap handler with CORS and logging
    auto wrapped_handler = [this, handler, path, method](const httplib::Request& req, httplib::Response& res) {
        auto start_time = std::chrono::steady_clock::now();
        
        pImpl->addCorsHeaders(res);
        handler(req, res);
        
        auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time);
        pImpl->logRequest(path, method, res.status, response_time);
    };
    
    // Register with httplib server
    if (method == "GET") {
        pImpl->server_->Get(full_path.c_str(), wrapped_handler);
    } else if (method == "POST") {
        pImpl->server_->Post(full_path.c_str(), wrapped_handler);
    } else if (method == "PUT") {
        pImpl->server_->Put(full_path.c_str(), wrapped_handler);
    } else if (method == "DELETE") {
        pImpl->server_->Delete(full_path.c_str(), wrapped_handler);
    }
    
    pImpl->registered_endpoints_.push_back(method + " " + full_path);
    Logger::debug("Registered route: " + method + " " + full_path);
}

void ApiServer::registerGetRoute(const std::string& path, RouteHandler handler) {
    registerRoute("GET", path, handler);
}

void ApiServer::registerPostRoute(const std::string& path, RouteHandler handler) {
    registerRoute("POST", path, handler);
}

void ApiServer::registerPutRoute(const std::string& path, RouteHandler handler) {
    registerRoute("PUT", path, handler);
}

void ApiServer::registerDeleteRoute(const std::string& path, RouteHandler handler) {
    registerRoute("DELETE", path, handler);
}

// ========================================================================
// Default Endpoint Implementations
// ========================================================================

void ApiServer::setupDefaultEndpoints() {
    if (!pImpl->server_) return;
    
    auto& server = *pImpl->server_;
    
    // CORS preflight handler
    server.Options(".*", [this](const httplib::Request& req, httplib::Response& res) {
        pImpl->addCorsHeaders(res);
        res.status = 200;
    });
    
    // === Core Service Endpoints ===
    
    // GET /api/v1/health - Health check
    server.Get(pImpl->api_prefix_ + "/health", [this](const httplib::Request& req, httplib::Response& res) {
        handleHealthCheck(req, res);
    });
    
    // GET /api/v1/service/status - Service status
    server.Get(pImpl->api_prefix_ + "/service/status", [this](const httplib::Request& req, httplib::Response& res) {
        handleServiceStatus(req, res);
    });
    
    // GET /api/v1/service/metrics - Service metrics
    server.Get(pImpl->api_prefix_ + "/service/metrics", [this](const httplib::Request& req, httplib::Response& res) {
        handleServiceMetrics(req, res);
    });
    
    // POST /api/v1/service/start - Start service
    server.Post(pImpl->api_prefix_ + "/service/start", [this](const httplib::Request& req, httplib::Response& res) {
        handleStartService(req, res);
    });
    
    // POST /api/v1/service/stop - Stop service
    server.Post(pImpl->api_prefix_ + "/service/stop", [this](const httplib::Request& req, httplib::Response& res) {
        handleStopService(req, res);
    });
    
    // POST /api/v1/service/restart - Restart service
    server.Post(pImpl->api_prefix_ + "/service/restart", [this](const httplib::Request& req, httplib::Response& res) {
        handleRestartService(req, res);
    });
    
    // POST /api/v1/service/reset - Reset service
    server.Post(pImpl->api_prefix_ + "/service/reset", [this](const httplib::Request& req, httplib::Response& res) {
        handleResetService(req, res);
    });
    
    // GET /api/v1/api/info - API information
    server.Get(pImpl->api_prefix_ + "/api/info", [this](const httplib::Request& req, httplib::Response& res) {
        handleApiInfo(req, res);
    });
    
    // GET /api/v1/endpoints - List all endpoints
    server.Get(pImpl->api_prefix_ + "/endpoints", [this](const httplib::Request& req, httplib::Response& res) {
        handleListEndpoints(req, res);
    });
    
    // Register default endpoints in list
    pImpl->registered_endpoints_ = {
        "GET " + pImpl->api_prefix_ + "/health",
        "GET " + pImpl->api_prefix_ + "/service/status",
        "GET " + pImpl->api_prefix_ + "/service/metrics",
        "POST " + pImpl->api_prefix_ + "/service/start",
        "POST " + pImpl->api_prefix_ + "/service/stop",
        "POST " + pImpl->api_prefix_ + "/service/restart",
        "POST " + pImpl->api_prefix_ + "/service/reset",
        "GET " + pImpl->api_prefix_ + "/api/info",
        "GET " + pImpl->api_prefix_ + "/endpoints"
    };
    
    Logger::info("Default API endpoints registered");
}

// ========================================================================
// Default Endpoint Handlers
// ========================================================================

void ApiServer::handleHealthCheck(const httplib::Request& req, httplib::Response& res) {
    auto start_time = std::chrono::steady_clock::now();
    
    pImpl->addCorsHeaders(res);
    
    json health = {
        {"status", "healthy"},
        {"uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - pImpl->stats_.start_time).count()},
        {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()}
    };
    
    res.set_content(pImpl->createSuccessResponse("Service is healthy", health.dump()), "application/json");
    
    auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    pImpl->logRequest("/health", "GET", 200, response_time);
}

void ApiServer::handleServiceStatus(const httplib::Request& req, httplib::Response& res) {
    auto start_time = std::chrono::steady_clock::now();
    
    pImpl->addCorsHeaders(res);
    
    json status = {
        {"service", "microservice_template"},
        {"version", "1.0.0"},
        {"running", pImpl->running_},
        {"initialized", pImpl->initialized_},
        {"uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - pImpl->stats_.start_time).count()},
        {"api_server", {
            {"host", pImpl->host_},
            {"port", pImpl->port_},
            {"prefix", pImpl->api_prefix_},
            {"cors_enabled", pImpl->cors_enabled_}
        }}
    };
    
    // Add custom status if callback is set
    if (pImpl->status_callback_) {
        try {
            std::string custom_status = pImpl->status_callback_();
            status["custom"] = json::parse(custom_status);
        } catch (...) {
            status["custom"] = "error_getting_custom_status";
        }
    }
    
    res.set_content(pImpl->createSuccessResponse("Service status retrieved", status.dump()), "application/json");
    
    auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    pImpl->logRequest("/service/status", "GET", 200, response_time);
}

void ApiServer::handleServiceMetrics(const httplib::Request& req, httplib::Response& res) {
    auto start_time = std::chrono::steady_clock::now();
    
    pImpl->addCorsHeaders(res);
    
    auto stats = getStats();
    
    json metrics = {
        {"api_requests", {
            {"total", stats.total_requests},
            {"successful", stats.successful_requests},
            {"failed", stats.failed_requests},
            {"active_connections", stats.active_connections}
        }},
        {"response_times", {
            {"average_ms", stats.average_response_time.count()},
            {"fastest_ms", stats.fastest_response_time == std::chrono::milliseconds::max() ? 0 : stats.fastest_response_time.count()},
            {"slowest_ms", stats.slowest_response_time.count()}
        }},
        {"endpoint_counts", stats.endpoint_counts},
        {"uptime_seconds", std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - stats.start_time).count()},
        {"last_request", stats.total_requests > 0 ? std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - stats.last_request_time).count() : 0}
    };
    
    res.set_content(pImpl->createSuccessResponse("Service metrics retrieved", metrics.dump()), "application/json");
    
    auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    pImpl->logRequest("/service/metrics", "GET", 200, response_time);
}

void ApiServer::handleStartService(const httplib::Request& req, httplib::Response& res) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (!pImpl->validateApiKey(req)) {
        res.status = 401;
        res.set_content(pImpl->createErrorResponse("Unauthorized", 401), "application/json");
        return;
    }
    
    pImpl->addCorsHeaders(res);
    
    bool success = false;
    if (pImpl->start_callback_) {
        success = pImpl->start_callback_();
    } else {
        Logger::warn("No start callback registered - operation not supported");
        res.status = 501;
        res.set_content(pImpl->createErrorResponse("Start operation not implemented", 501), "application/json");
        return;
    }
    
    json result = {
        {"operation", "start"},
        {"success", success}
    };
    
    if (success) {
        res.set_content(pImpl->createSuccessResponse("Service started successfully", result.dump()), "application/json");
    } else {
        res.status = 500;
        res.set_content(pImpl->createErrorResponse("Failed to start service"), "application/json");
    }
    
    auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    pImpl->logRequest("/service/start", "POST", res.status, response_time);
}

void ApiServer::handleStopService(const httplib::Request& req, httplib::Response& res) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (!pImpl->validateApiKey(req)) {
        res.status = 401;
        res.set_content(pImpl->createErrorResponse("Unauthorized", 401), "application/json");
        return;
    }
    
    pImpl->addCorsHeaders(res);
    
    bool success = false;
    if (pImpl->stop_callback_) {
        success = pImpl->stop_callback_();
    } else {
        Logger::warn("No stop callback registered - operation not supported");
        res.status = 501;
        res.set_content(pImpl->createErrorResponse("Stop operation not implemented", 501), "application/json");
        return;
    }
    
    json result = {
        {"operation", "stop"},
        {"success", success}
    };
    
    if (success) {
        res.set_content(pImpl->createSuccessResponse("Service stopped successfully", result.dump()), "application/json");
    } else {
        res.status = 500;
        res.set_content(pImpl->createErrorResponse("Failed to stop service"), "application/json");
    }
    
    auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    pImpl->logRequest("/service/stop", "POST", res.status, response_time);
}

void ApiServer::handleRestartService(const httplib::Request& req, httplib::Response& res) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (!pImpl->validateApiKey(req)) {
        res.status = 401;
        res.set_content(pImpl->createErrorResponse("Unauthorized", 401), "application/json");
        return;
    }
    
    pImpl->addCorsHeaders(res);
    
    bool success = false;
    if (pImpl->restart_callback_) {
        success = pImpl->restart_callback_();
    } else {
        Logger::warn("No restart callback registered - operation not supported");
        res.status = 501;
        res.set_content(pImpl->createErrorResponse("Restart operation not implemented", 501), "application/json");
        return;
    }
    
    json result = {
        {"operation", "restart"},
        {"success", success}
    };
    
    if (success) {
        res.set_content(pImpl->createSuccessResponse("Service restarted successfully", result.dump()), "application/json");
    } else {
        res.status = 500;
        res.set_content(pImpl->createErrorResponse("Failed to restart service"), "application/json");
    }
    
    auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    pImpl->logRequest("/service/restart", "POST", res.status, response_time);
}

void ApiServer::handleResetService(const httplib::Request& req, httplib::Response& res) {
    auto start_time = std::chrono::steady_clock::now();
    
    if (!pImpl->validateApiKey(req)) {
        res.status = 401;
        res.set_content(pImpl->createErrorResponse("Unauthorized", 401), "application/json");
        return;
    }
    
    pImpl->addCorsHeaders(res);
    
    bool success = false;
    if (pImpl->reset_callback_) {
        success = pImpl->reset_callback_();
    } else {
        Logger::warn("No reset callback registered - operation not supported");
        res.status = 501;
        res.set_content(pImpl->createErrorResponse("Reset operation not implemented", 501), "application/json");
        return;
    }
    
    json result = {
        {"operation", "reset"},
        {"success", success}
    };
    
    if (success) {
        res.set_content(pImpl->createSuccessResponse("Service reset successfully", result.dump()), "application/json");
    } else {
        res.status = 500;
        res.set_content(pImpl->createErrorResponse("Failed to reset service"), "application/json");
    }
    
    auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    pImpl->logRequest("/service/reset", "POST", res.status, response_time);
}

void ApiServer::handleApiInfo(const httplib::Request& req, httplib::Response& res) {
    auto start_time = std::chrono::steady_clock::now();
    
    pImpl->addCorsHeaders(res);
    
    json info = {
        {"service_name", "microservice_template"},
        {"version", "1.0.0"},
        {"api_version", "v1"},
        {"api_prefix", pImpl->api_prefix_},
        {"documentation", "/api/v1/endpoints"},
        {"health_check", "/api/v1/health"},
        {"features", json::array({
            "REST API",
            "MQTT Pub/Sub",
            "Service Lifecycle Management",
            "Health Monitoring",
            "Custom Route Registration"
        })}
    };
    
    res.set_content(pImpl->createSuccessResponse("API information retrieved", info.dump()), "application/json");
    
    auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    pImpl->logRequest("/api/info", "GET", 200, response_time);
}

void ApiServer::handleListEndpoints(const httplib::Request& req, httplib::Response& res) {
    auto start_time = std::chrono::steady_clock::now();
    
    pImpl->addCorsHeaders(res);
    
    json endpoints = json::array();
    for (const auto& endpoint : pImpl->registered_endpoints_) {
        endpoints.push_back(endpoint);
    }
    
    json result = {
        {"total_endpoints", endpoints.size()},
        {"endpoints", endpoints},
        {"api_prefix", pImpl->api_prefix_}
    };
    
    res.set_content(pImpl->createSuccessResponse("Endpoints listed successfully", result.dump()), "application/json");
    
    auto response_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time);
    pImpl->logRequest("/endpoints", "GET", 200, response_time);
}

// Utility methods
std::string ApiServer::createJsonResponse(bool success, const std::string& message, const std::string& data) {
    return pImpl->createJsonResponse(success, message, data);
}

std::string ApiServer::createErrorResponse(const std::string& error, int status_code) {
    return pImpl->createErrorResponse(error, status_code);
}

std::string ApiServer::createSuccessResponse(const std::string& message, const std::string& data) {
    return pImpl->createSuccessResponse(message, data);
}

void ApiServer::addCorsHeaders(httplib::Response& res) {
    pImpl->addCorsHeaders(res);
}

void ApiServer::logRequest(const std::string& endpoint, const std::string& method, int status_code, 
                           std::chrono::milliseconds response_time) {
    pImpl->logRequest(endpoint, method, status_code, response_time);
}

bool ApiServer::validateApiKey(const httplib::Request& req) {
    return pImpl->validateApiKey(req);
}

void ApiServer::applyMiddleware(const httplib::Request& req, httplib::Response& res) {
    // Apply CORS
    pImpl->addCorsHeaders(res);
    
    // Add standard headers
    res.set_header("X-Service-Name", "microservice_template");
    res.set_header("X-API-Version", "1.0");
}

} // namespace microservice
