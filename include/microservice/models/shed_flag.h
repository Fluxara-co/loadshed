/**
 * @file shed_flag.h
 * @brief Shed flag processing model
 * 
 * Based on RTAC Gateway SHED_FLAG function block
 */

#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>
#include "load_data.h"
#include <chrono>

namespace microservice {
namespace models {

/**
 * @brief DWORD array size for bit-packed load trip outputs
 * 4 DWORDs = 128 bits (supports up to 128 loads)
 */
constexpr uint32_t DWORD_ARRAY_SIZE = 4;

/**
 * @brief Load trip outputs from LSP controller
 * 
 * Bit-packed DWORD array representing which loads should be shed.
 * Each DWORD contains 32 bits, total 128 bits (supports up to 128 loads).
 * Size is configurable per project.
 */
struct LoadTripOutputs {
    std::vector<uint32_t> trip_outputs;  // Bit-packed load trip flags
    
    /**
     * @brief Constructor - initializes DWORD array
     */
    LoadTripOutputs() : trip_outputs(DWORD_ARRAY_SIZE, 0) {}
    
    /**
     * @brief Extract bit for specific load (1-based)
     * @param load_id Load ID (1-based)
     * @param max_loads Maximum number of loads (for validation)
     */
    bool getLoadBit(uint32_t load_id, uint32_t max_loads) const {
        if (load_id == 0 || load_id > max_loads) return false;
        if (load_id > (trip_outputs.size() * 32)) return false;
        uint32_t index = load_id - 1;
        uint32_t dword_index = index / 32;
        uint32_t bit_index = index % 32;
        if (dword_index >= trip_outputs.size()) return false;
        return (trip_outputs[dword_index] >> bit_index) & 1;
    }
    
    /**
     * @brief Set bit for specific load
     * @param load_id Load ID (1-based)
     * @param value Bit value
     * @param max_loads Maximum number of loads (for validation)
     */
    void setLoadBit(uint32_t load_id, bool value, uint32_t max_loads) {
        if (load_id == 0 || load_id > max_loads) return;
        if (load_id > (trip_outputs.size() * 32)) return;
        uint32_t index = load_id - 1;
        uint32_t dword_index = index / 32;
        uint32_t bit_index = index % 32;
        if (dword_index >= trip_outputs.size()) return;
        if (value) {
            trip_outputs[dword_index] |= (1U << bit_index);
        } else {
            trip_outputs[dword_index] &= ~(1U << bit_index);
        }
    }
    
    /**
     * @brief Resize DWORD array if needed for more loads
     */
    void resizeForLoads(uint32_t max_loads) {
        uint32_t required_dwords = (max_loads + 31) / 32;  // Round up
        if (required_dwords > trip_outputs.size()) {
            trip_outputs.resize(required_dwords, 0);
        }
    }
};

/**
 * @brief Shed flag processor state
 * 
 * Maintains shed flags with Set-Reset latch behavior.
 * Based on RTAC SHED_FLAG function block.
 * Sizes are configurable per project.
 */
struct ShedFlagState {
    // Shed flags for each load (0=no shed, 1=UF shed, 2=contingency shed)
    std::vector<ShedFlag> shed_flags;
    
    // Contingency flags (active/inactive)
    std::vector<bool> cont_flags;
    
    // UF contingency flags
    std::vector<bool> uf_cont_flags;
    
    // Internal latch states
    std::vector<bool> load_shed_latch;      // Contingency shed latch
    std::vector<bool> uf_load_shed_latch;   // UF shed latch
    std::vector<bool> cont_latch;           // Contingency detection latch
    std::vector<bool> uf_cont_latch;        // UF contingency latch
    
    /**
     * @brief Default constructor - empty state
     */
    ShedFlagState() {}
    
    /**
     * @brief Initialize with configuration
     * @param max_loads Maximum number of loads
     * @param max_contingencies Maximum number of normal contingencies
     * @param max_uf_contingencies Maximum number of UF contingencies
     */
    void initialize(uint32_t max_loads, uint32_t max_contingencies, uint32_t max_uf_contingencies) {
        shed_flags.resize(max_loads, ShedFlag::NO_SHED);
        cont_flags.resize(max_contingencies, false);
        uf_cont_flags.resize(max_uf_contingencies, false);
        load_shed_latch.resize(max_loads, false);
        uf_load_shed_latch.resize(max_loads, false);
        cont_latch.resize(max_contingencies, false);
        uf_cont_latch.resize(max_uf_contingencies, false);
    }
    
    /**
     * @brief Clear all flags (CLEAR command)
     */
    void clear() {
        std::fill(shed_flags.begin(), shed_flags.end(), ShedFlag::NO_SHED);
        std::fill(cont_flags.begin(), cont_flags.end(), false);
        std::fill(uf_cont_flags.begin(), uf_cont_flags.end(), false);
        std::fill(load_shed_latch.begin(), load_shed_latch.end(), false);
        std::fill(uf_load_shed_latch.begin(), uf_load_shed_latch.end(), false);
        std::fill(cont_latch.begin(), cont_latch.end(), false);
        std::fill(uf_cont_latch.begin(), uf_cont_latch.end(), false);
    }
};

} // namespace models
} // namespace microservice
