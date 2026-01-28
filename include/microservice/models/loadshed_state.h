/**
 * @file loadshed_state.h
 * @brief Complete loadshed system state
 * 
 * Aggregates all loadshed entities into a single state object
 */

#pragma once

#include "load_data.h"
#include "contingency_data.h"
#include "shed_flag.h"
#include "loadshed_config.h"
#include <chrono>
#include <vector>
#include <cstdint>

namespace microservice {
namespace models {

/**
 * @brief Complete loadshed system state
 * 
 * Contains all loads, contingencies, and shed flags.
 * This is the main state object for the loadshed service.
 * Sizes are configurable per project.
 */
struct LoadShedState {
    // Configuration
    LoadShedConfig config;
    
    // Loads (size is configurable)
    LoadArray loads;
    
    // Normal contingencies (size is configurable)
    ContingencyArray contingencies;
    
    // UF contingencies (size is configurable)
    UFContingencyArray uf_contingencies;
    
    // Shed flag state
    ShedFlagState shed_flags;
    
    // Priority list (for all loads, size is configurable)
    std::vector<uint16_t> priority_list;
    
    // UF load to shed values
    double uf_l1_load_to_shed;      // Level 1 UF load to shed (kW)
    double uf_l2_load_to_shed;      // Level 2 UF load to shed (kW)
    
    // System flags
    bool test_mode;                 // Test mode active
    bool lsp_a_disabled;            // LSP A disabled
    bool lsp_b_disabled;            // LSP B disabled
    bool clear_pending;              // CLEAR command pending
    
    // Metadata
    std::chrono::system_clock::time_point last_updated;
    uint32_t version;                // State version (for change tracking)
    
    /**
     * @brief Default constructor
     */
    LoadShedState() 
        : uf_l1_load_to_shed(0.0)
        , uf_l2_load_to_shed(0.0)
        , test_mode(false)
        , lsp_a_disabled(false)
        , lsp_b_disabled(false)
        , clear_pending(false)
        , last_updated(std::chrono::system_clock::now())
        , version(1)
    {}
    
    /**
     * @brief Initialize state with configuration
     * @param cfg Loadshed configuration
     */
    void initialize(const LoadShedConfig& cfg) {
        config = cfg;
        
        // Resize vectors to match configuration
        loads.resize(config.max_loads);
        contingencies.resize(config.max_contingencies);
        uf_contingencies.resize(config.max_uf_contingencies);
        priority_list.resize(config.max_loads, 0);
        
        // Initialize shed flags
        shed_flags.initialize(config.max_loads, config.max_contingencies, config.max_uf_contingencies);
        
        // Initialize load IDs
        for (uint32_t i = 0; i < loads.size(); ++i) {
            loads[i].id = i + 1;
        }
        
        // Initialize contingency IDs
        for (uint32_t i = 0; i < contingencies.size(); ++i) {
            contingencies[i].id = i + 1;
            contingencies[i].type = ContingencyType::NORMAL;
            contingencies[i].resizeLists(config.max_loads);
        }
        
        for (uint32_t i = 0; i < uf_contingencies.size(); ++i) {
            uf_contingencies[i].id = i + 1;
            uf_contingencies[i].type = ContingencyType::UF;
            uf_contingencies[i].resizeLists(config.max_loads);
        }
    }
    
    /**
     * @brief Clear all shed flags
     */
    void clearShedFlags() {
        shed_flags.clear();
        clear_pending = false;
        version++;
    }
    
    /**
     * @brief Get load by ID
     */
    LoadData* getLoad(uint32_t load_id) {
        if (load_id == 0 || load_id > config.max_loads) return nullptr;
        if (load_id > loads.size()) return nullptr;
        return &loads[load_id - 1];
    }
    
    /**
     * @brief Get contingency by ID
     */
    ContingencyData* getContingency(uint32_t cont_id, bool is_uf = false) {
        if (is_uf) {
            if (cont_id == 0 || cont_id > config.max_uf_contingencies) return nullptr;
            if (cont_id > uf_contingencies.size()) return nullptr;
            return &uf_contingencies[cont_id - 1];
        } else {
            if (cont_id == 0 || cont_id > config.max_contingencies) return nullptr;
            if (cont_id > contingencies.size()) return nullptr;
            return &contingencies[cont_id - 1];
        }
    }
};

} // namespace models
} // namespace microservice
