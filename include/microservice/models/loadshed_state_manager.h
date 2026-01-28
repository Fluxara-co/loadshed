/**
 * @file loadshed_state_manager.h
 * @brief Loadshed state manager for SLD-based configuration
 * 
 * Manages LoadShedState initialization and updates from SLD configuration
 */

#pragma once

#include "loadshed_state.h"
#include "sld_config.h"
#include "tag_mapping.h"
#include <nlohmann/json.hpp>
#include <map>
#include <memory>
#include <mutex>

namespace microservice {
namespace models {

/**
 * @brief LoadShed State Manager
 * 
 * Manages the loadshed system state, initialized from SLD configuration.
 * Supports runtime configuration updates.
 */
class LoadShedStateManager {
public:
    LoadShedStateManager();
    ~LoadShedStateManager() = default;
    
    /**
     * @brief Initialize state from SLD configuration
     * 
     * Parses SLD JSON and initializes LoadShedState with appropriate counts
     * 
     * @param sld_json Complete SLD JSON from ui-manager
     * @return true if initialization successful
     */
    bool initializeFromSLD(const nlohmann::json& sld_json);
    
    /**
     * @brief Update configuration at runtime
     * 
     * Parses new SLD JSON and updates state. Reinitializes if counts changed.
     * 
     * @param sld_json Complete SLD JSON from ui-manager
     * @return true if update successful
     */
    bool updateConfiguration(const nlohmann::json& sld_json);
    
    /**
     * @brief Get current state (mutable)
     */
    LoadShedState& getState();
    
    /**
     * @brief Get current state (const)
     */
    const LoadShedState& getState() const;
    
    /**
     * @brief Get SLD configuration
     */
    const SLDConfig& getSLDConfig() const { return sld_config_; }
    
    /**
     * @brief Get tag mapping for a node and property
     * 
     * @param node_id SLD node_id
     * @param property_key Property key (e.g., "realPower", "status")
     * @return TagMapping pointer, or nullptr if not found
     */
    const TagMapping* getTagMapping(const std::string& node_id, 
                                    const std::string& property_key) const;
    
    /**
     * @brief Get load index by SLD node_id
     * 
     * @param node_id SLD node_id
     * @return Load index (1-based), or 0 if not found
     */
    uint32_t getLoadIndex(const std::string& node_id) const;
    
    /**
     * @brief Get contingency index by SLD node_id
     * 
     * @param node_id SLD node_id
     * @param is_uf If true, search UF contingencies; if false, search normal
     * @return Contingency index (1-based), or 0 if not found
     */
    uint32_t getContingencyIndex(const std::string& node_id, bool is_uf = false) const;
    
    /**
     * @brief Check if state is initialized
     */
    bool isInitialized() const { return initialized_; }
    
private:
    /**
     * @brief Internal initialization from parsed SLD config
     */
    bool initializeFromSLDConfig(const SLDConfig& sld_config);
    
    /**
     * @brief Map SLD nodes to entities and populate state
     */
    void mapSLDNodesToEntities(const SLDConfig& sld_config);
    
    /**
     * @brief Build node index maps
     */
    void buildNodeIndexMaps(const SLDConfig& sld_config);
    
    /**
     * @brief Preserve existing state values when reinitializing
     * 
     * Attempts to preserve priorities, IRM values, etc. when counts don't change
     */
    void preserveStateValues(LoadShedState& old_state, LoadShedState& new_state);
    
    LoadShedState state_;
    SLDConfig sld_config_;
    bool initialized_;
    
    // Tag mappings: node_id -> property_key -> TagMapping
    std::map<std::string, TagMappingMap> tag_mappings_;
    
    // Node index maps: node_id -> entity index (1-based)
    std::map<std::string, uint32_t> load_node_map_;        // node_id -> load index
    std::map<std::string, uint32_t> contingency_node_map_; // node_id -> cont index
    std::map<std::string, uint32_t> uf_contingency_node_map_; // node_id -> UF cont index
    
    // Thread safety
    mutable std::mutex state_mutex_;
};

} // namespace models
} // namespace microservice
