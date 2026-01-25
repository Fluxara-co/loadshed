#pragma once

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <chrono>

// Forward declarations
namespace httplib {
    class Server;
    struct Request;
    struct Response;
}

namespace microservice {

/**
 * @brief Generic HTTP API Server for Microservices
 * 
 * Provides RESTful API endpoints for:
 * - Service lifecycle (start, stop, restart, reset)
 * - Health checks
 * - Status and metrics
 * - Custom endpoint registration
 */
class ApiServer {
public:
    ApiServer();
    ~ApiServer();
    
    // Copy operations disabled
    ApiServer(const ApiServer&) = delete;
    ApiServer& operator=(const ApiServer&) = delete;
    
    // Move operations disabled
    ApiServer(ApiServer&&) = delete;
    ApiServer& operator=(ApiServer&&) = delete;
    
    // Server lifecycle
    bool initialize(uint16_t port = 8080, const std::string& host = "0.0.0.0");
    bool start();
    bool stop();
    bool isRunning() const;
    void shutdown();
    
    // Configuration
    void setPort(uint16_t port);
    void setHost(const std::string& host);
    void enableCors(bool enable = true);
    void setApiPrefix(const std::string& prefix = "/api/v1");
    void setApiKey(const std::string& api_key);  // Optional: simple API key auth
    
    // Custom endpoint registration
    using RouteHandler = std::function<void(const httplib::Request&, httplib::Response&)>;
    void registerRoute(const std::string& method, const std::string& path, RouteHandler handler);
    void registerGetRoute(const std::string& path, RouteHandler handler);
    void registerPostRoute(const std::string& path, RouteHandler handler);
    void registerPutRoute(const std::string& path, RouteHandler handler);
    void registerDeleteRoute(const std::string& path, RouteHandler handler);
    
    // Service control callbacks (to be set by application)
    using ServiceCallback = std::function<bool()>;
    void setStartCallback(ServiceCallback callback);
    void setStopCallback(ServiceCallback callback);
    void setRestartCallback(ServiceCallback callback);
    void setResetCallback(ServiceCallback callback);
    void setStatusCallback(std::function<std::string()> callback);
    
    // Status
    uint16_t getPort() const;
    std::string getHost() const;
    std::string getApiPrefix() const;
    bool isCorsEnabled() const;
    
    // Statistics
    struct ServerStats {
        uint64_t total_requests{0};
        uint64_t successful_requests{0};
        uint64_t failed_requests{0};
        uint64_t active_connections{0};
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_request_time;
        
        // Request counts by endpoint
        std::map<std::string, uint64_t> endpoint_counts;
        
        // Response time statistics
        std::chrono::milliseconds average_response_time{0};
        std::chrono::milliseconds fastest_response_time{std::chrono::milliseconds::max()};
        std::chrono::milliseconds slowest_response_time{0};
    };
    
    ServerStats getStats() const;
    void resetStats();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    
    // Setup default endpoints
    void setupDefaultEndpoints();
    
    // === Default Service Lifecycle Endpoints ===
    
    // Health check
    void handleHealthCheck(const httplib::Request& req, httplib::Response& res);
    
    // Service status
    void handleServiceStatus(const httplib::Request& req, httplib::Response& res);
    
    // Service metrics
    void handleServiceMetrics(const httplib::Request& req, httplib::Response& res);
    
    // Start service
    void handleStartService(const httplib::Request& req, httplib::Response& res);
    
    // Stop service
    void handleStopService(const httplib::Request& req, httplib::Response& res);
    
    // Restart service
    void handleRestartService(const httplib::Request& req, httplib::Response& res);
    
    // Reset service (clear state, reinitialize)
    void handleResetService(const httplib::Request& req, httplib::Response& res);
    
    // API information
    void handleApiInfo(const httplib::Request& req, httplib::Response& res);
    
    // List all registered endpoints
    void handleListEndpoints(const httplib::Request& req, httplib::Response& res);
    
    // === Utility methods ===
    
    std::string createJsonResponse(bool success, const std::string& message, const std::string& data = "{}");
    std::string createErrorResponse(const std::string& error, int status_code = 400);
    std::string createSuccessResponse(const std::string& message, const std::string& data = "{}");
    
    // CORS handling
    void addCorsHeaders(httplib::Response& res);
    
    // Request logging and statistics
    void logRequest(const std::string& endpoint, const std::string& method, int status_code, 
                   std::chrono::milliseconds response_time);
    
    // Authentication
    bool validateApiKey(const httplib::Request& req);
    
    // Middleware
    void applyMiddleware(const httplib::Request& req, httplib::Response& res);
};

} // namespace microservice
