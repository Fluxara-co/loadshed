/**
 * @file loadshed_config.h
 * @brief Loadshed system configuration - defines configurable limits
 * 
 * These limits are configurable per project, not hardcoded like RTAC
 */

#pragma once

#include <cstdint>

namespace microservice {
namespace models {

/**
 * @brief Loadshed system configuration
 * 
 * Defines the maximum number of loads, contingencies, etc.
 * These are loaded from configuration file, not hardcoded.
 */
struct LoadShedConfig {
    uint32_t max_loads;              // Maximum number of sheddable loads
    uint32_t max_contingencies;      // Maximum number of normal contingencies
    uint32_t max_uf_contingencies;   // Maximum number of UF contingencies
    uint32_t max_topology_breakers;  // Maximum number of topology breakers
    uint32_t max_generators;         // Maximum number of generators
    uint32_t max_buses;              // Maximum number of buses
    
    /**
     * @brief Default constructor with RTAC-compatible defaults
     * These are just defaults - actual values come from config file
     */
    LoadShedConfig()
        : max_loads(47)
        , max_contingencies(8)
        , max_uf_contingencies(6)
        , max_topology_breakers(15)
        , max_generators(6)
        , max_buses(3)
    {}
    
    /**
     * @brief Validate configuration
     */
    bool isValid() const {
        return max_loads > 0 && 
               max_contingencies > 0 && 
               max_uf_contingencies > 0 &&
               max_loads <= 1000 &&      // Reasonable upper limit
               max_contingencies <= 100 &&
               max_uf_contingencies <= 100;
    }
};

} // namespace models
} // namespace microservice
