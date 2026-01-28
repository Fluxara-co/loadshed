/**
 * @file controller_interface.h
 * @brief Interface for load shedding controllers (external and native)
 * 
 * Provides abstraction for communicating with load shedding controllers.
 * Supports both external controllers (via MQTT) and native controller (placeholder).
 */

#pragma once

#include "load_data.h"
#include "contingency_data.h"
#include "shed_flag.h"
#include "loadshed_state.h"
#include "device_config.h"
#include <memory>
#include <functional>
#include <string>

namespace microservice {
namespace models {

/**
 * @brief Controller interface - abstract base class
 * 
 * Defines the interface for all load shedding controllers
 * (both external via MQTT and native placeholder)
 */
class ILoadShedController {
public:
    virtual ~ILoadShedController() = default;
    
    /**
     * @brief Initialize the controller
     */
    virtual bool initialize() = 0;
    
    /**
     * @brief Start the controller
     */
    virtual bool start() = 0;
    
    /**
     * @brief Stop the controller
     */
    virtual bool stop() = 0;
    
    /**
     * @brief Check if controller is connected
     */
    virtual bool isConnected() const = 0;
    
    /**
     * @brief Send commands to controller
     */
    virtual bool sendCommands(const LoadShedState& state) = 0;
    
    /**
     * @brief Receive data from controller
     * @return true if new data received
     */
    virtual bool receiveData(LoadShedState& state) = 0;
    
    /**
     * @brief Get controller device ID
     */
    virtual std::string getDeviceId() const = 0;
    
    /**
     * @brief Set callback for when data is received
     */
    virtual void setDataCallback(std::function<void(const LoadShedState&)> callback) = 0;
};

/**
 * @brief External LSP controller (MQTT-based)
 * 
 * Communicates with external load shedding controllers via MQTT
 * based on device configuration.
 */
class ExternalLSPController : public ILoadShedController {
public:
    /**
     * @brief Constructor
     * @param device_config Device configuration for this controller
     */
    explicit ExternalLSPController(const DeviceConfig& device_config);
    
    bool initialize() override;
    bool start() override;
    bool stop() override;
    bool isConnected() const override;
    bool sendCommands(const LoadShedState& state) override;
    bool receiveData(LoadShedState& state) override;
    std::string getDeviceId() const override;
    void setDataCallback(std::function<void(const LoadShedState&)> callback) override;
    
private:
    DeviceConfig device_config_;
    bool connected_;
    std::function<void(const LoadShedState&)> data_callback_;
    
    // MQTT publisher/subscriber (will be injected)
    // For now, placeholder
};

/**
 * @brief Native load shedding controller (placeholder)
 * 
 * Placeholder for future native loadshed decision logic.
 * For now, this is a no-op implementation.
 */
class NativeLoadShedController : public ILoadShedController {
public:
    NativeLoadShedController();
    
    bool initialize() override;
    bool start() override;
    bool stop() override;
    bool isConnected() const override;
    bool sendCommands(const LoadShedState& state) override;
    bool receiveData(LoadShedState& state) override;
    std::string getDeviceId() const override;
    void setDataCallback(std::function<void(const LoadShedState&)> callback) override;
    
private:
    bool initialized_;
    bool running_;
    std::function<void(const LoadShedState&)> data_callback_;
    
    // TODO: Future implementation
    // - Load selection algorithm
    // - Contingency detection logic
    // - Priority-based shedding
};

} // namespace models
} // namespace microservice
