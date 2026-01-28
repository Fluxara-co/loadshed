/**
 * @file load_data.h
 * @brief Load entity model - represents a sheddable load
 * 
 * Based on RTAC Gateway LOAD_DATA and LOAD_DATAOUT structures
 */

#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <vector>

namespace microservice {
namespace models {

/**
 * @brief Load status enumeration
 */
enum class LoadStatus {
    GOOD = 0,
    BAD = 1
};

/**
 * @brief Shed flag enumeration
 * Maps to RTAC SHED_FLAGS values
 */
enum class ShedFlag {
    NO_SHED = 0,        // No shed active
    UF_SHED = 1,        // Under Frequency shed
    CONTINGENCY_SHED = 2 // Contingency shed
};

/**
 * @brief Load entity model
 * 
 * Represents a single sheddable load with all its properties.
 * Based on RTAC LOAD_DATA and LOAD_DATAOUT structures.
 */
struct LoadData {
    // Basic identification
    uint32_t id;                    // Load ID (1-based)
    std::string name;                // Load name (max 32 chars)
    
    // Status flags (from LOAD_DATA bitpacking)
    LoadStatus status;               // Status (Good/Bad)
    bool test_shed;                  // Test shed active
    bool value_override;             // Manual value override
    bool trip;                       // Load trip command
    bool bad_status;                 // Communication/data quality issue
    bool alarm;                      // Load alarm condition
    
    // Load values
    double value_estimated;          // Estimated load value (kW)
    double present_p;                // Current real power (kW)
    double present_q;                // Current reactive power (kVAR) - unused in RTAC
    double value;                    // Measured value (kW)
    
    // Shedding configuration
    uint16_t priority;               // Shedding priority (1-based, 0=not sheddable)
    bool inhibit;                    // Load inhibited from shedding
    bool selected;                   // Load selected for shedding
    bool shed;                       // Load currently shed
    
    // Bus connection
    uint32_t bus_conn;               // Bus connection identifier (bit-packed DWORD)
    
    // Metadata
    std::chrono::system_clock::time_point last_updated;
    
    /**
     * @brief Default constructor
     */
    LoadData() 
        : id(0)
        , status(LoadStatus::GOOD)
        , test_shed(false)
        , value_override(false)
        , trip(false)
        , bad_status(false)
        , alarm(false)
        , value_estimated(0.0)
        , present_p(0.0)
        , present_q(0.0)
        , value(0.0)
        , priority(0)
        , inhibit(false)
        , selected(false)
        , shed(false)
        , bus_conn(0)
        , last_updated(std::chrono::system_clock::now())
    {}
    
    /**
     * @brief Check if load is available for shedding
     */
    bool isAvailable() const {
        return status == LoadStatus::GOOD && 
               !bad_status && 
               !inhibit && 
               priority > 0;
    }
    
    /**
     * @brief Check if load should be shed
     */
    bool shouldShed() const {
        return shed || selected;
    }
};

/**
 * @brief Vector of loads (size is configurable)
 */
using LoadArray = std::vector<LoadData>;

} // namespace models
} // namespace microservice
