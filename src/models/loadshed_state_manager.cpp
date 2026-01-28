/**
 * @file loadshed_state_manager.cpp
 * @brief LoadShed state manager implementation
 */

#include "microservice/models/loadshed_state_manager.h"
#include "microservice/models/sld_parser.h"
#include "microservice/models/load_data.h"
#include "microservice/models/contingency_data.h"
#include "microservice/utils/logger.h"
#include <algorithm>

namespace microservice {
namespace models {

LoadShedStateManager::LoadShedStateManager()
    : initialized_(false)
{
}

bool LoadShedStateManager::initializeFromSLD(const nlohmann::json& sld_json) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    try {
        // Parse SLD
        SLDConfig sld_config;
        if (!SLDConfigParser::parseSLD(sld_json, sld_config)) {
            Logger::error("Failed to parse SLD JSON");
            return false;
        }
        
        // Initialize from parsed config
        return initializeFromSLDConfig(sld_config);
        
    } catch (const std::exception& e) {
        Logger::error("Exception during SLD initialization: " + std::string(e.what()));
        return false;
    }
}

bool LoadShedStateManager::updateConfiguration(const nlohmann::json& sld_json) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    try {
        // Parse new SLD
        SLDConfig new_sld_config;
        if (!SLDConfigParser::parseSLD(sld_json, new_sld_config)) {
            Logger::error("Failed to parse SLD JSON for update");
            return false;
        }
        
        // Check if counts changed
        bool counts_changed = false;
        if (!initialized_) {
            counts_changed = true;
        } else {
            counts_changed = 
                (new_sld_config.load_count != sld_config_.load_count) ||
                (new_sld_config.normal_contingency_count != sld_config_.normal_contingency_count) ||
                (new_sld_config.uf_contingency_count != sld_config_.uf_contingency_count) ||
                (new_sld_config.topology_breaker_count != sld_config_.topology_breaker_count);
        }
        
        // Save old state for preservation
        LoadShedState old_state = state_;
        
        // Reinitialize if counts changed
        if (counts_changed) {
            Logger::info("SLD configuration counts changed, reinitializing state");
            if (!initializeFromSLDConfig(new_sld_config)) {
                Logger::error("Failed to reinitialize state");
                return false;
            }
            
            // Preserve existing values where possible
            preserveStateValues(old_state, state_);
        } else {
            // Only update tag mappings and node maps
            Logger::info("SLD configuration updated (counts unchanged), updating mappings");
            sld_config_ = new_sld_config;
            buildNodeIndexMaps(sld_config_);
            
            // Update tag mappings
            tag_mappings_.clear();
            for (const auto& node : sld_config_.nodes) {
                if (node.isLoadshedNode()) {
                    tag_mappings_[node.node_id] = node.tag_mappings;
                }
            }
        }
        
        Logger::info("Configuration updated successfully");
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Exception during configuration update: " + std::string(e.what()));
        return false;
    }
}

bool LoadShedStateManager::initializeFromSLDConfig(const SLDConfig& sld_config) {
    try {
        // Extract LoadShedConfig from SLD
        LoadShedConfig config = SLDConfigParser::extractLoadShedConfig(sld_config);
        
        if (!config.isValid()) {
            Logger::error("Invalid LoadShedConfig extracted from SLD");
            return false;
        }
        
        // Initialize state with config
        state_.initialize(config);
        
        // Map SLD nodes to entities
        mapSLDNodesToEntities(sld_config);
        
        // Build node index maps
        buildNodeIndexMaps(sld_config);
        
        // Store SLD config
        sld_config_ = sld_config;
        
        // Store tag mappings
        tag_mappings_.clear();
        for (const auto& node : sld_config_.nodes) {
            if (node.isLoadshedNode()) {
                tag_mappings_[node.node_id] = node.tag_mappings;
            }
        }
        
        initialized_ = true;
        
        Logger::info("State initialized from SLD: " +
                    std::to_string(state_.loads.size()) + " loads, " +
                    std::to_string(state_.contingencies.size()) + " normal contingencies, " +
                    std::to_string(state_.uf_contingencies.size()) + " UF contingencies");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Exception during state initialization: " + std::string(e.what()));
        return false;
    }
}

void LoadShedStateManager::mapSLDNodesToEntities(const SLDConfig& sld_config) {
    // Map loads
    auto loads = SLDConfigParser::mapLoads(sld_config);
    for (size_t i = 0; i < loads.size() && i < state_.loads.size(); ++i) {
        state_.loads[i] = loads[i];
    }
    
    // Map normal contingencies
    auto contingencies = SLDConfigParser::mapContingencies(sld_config, false);
    for (size_t i = 0; i < contingencies.size() && i < state_.contingencies.size(); ++i) {
        // Resize select/inhibit lists to match max_loads
        contingencies[i].resizeLists(state_.config.max_loads);
        state_.contingencies[i] = contingencies[i];
    }
    
    // Map UF contingencies
    auto uf_contingencies = SLDConfigParser::mapContingencies(sld_config, true);
    for (size_t i = 0; i < uf_contingencies.size() && i < state_.uf_contingencies.size(); ++i) {
        // Resize select/inhibit lists to match max_loads
        uf_contingencies[i].resizeLists(state_.config.max_loads);
        state_.uf_contingencies[i] = uf_contingencies[i];
    }
}

void LoadShedStateManager::buildNodeIndexMaps(const SLDConfig& sld_config) {
    load_node_map_.clear();
    contingency_node_map_.clear();
    uf_contingency_node_map_.clear();
    
    // Map load nodes
    uint32_t load_index = 1;
    for (const auto* load_node : sld_config.loads) {
        load_node_map_[load_node->node_id] = load_index++;
    }
    
    // Map normal contingency nodes
    uint32_t cont_index = 1;
    for (const auto* cont_node : sld_config.normal_contingencies) {
        contingency_node_map_[cont_node->node_id] = cont_index++;
    }
    
    // Map UF contingency nodes
    uint32_t uf_cont_index = 1;
    for (const auto* uf_cont_node : sld_config.uf_contingencies) {
        uf_contingency_node_map_[uf_cont_node->node_id] = uf_cont_index++;
    }
}

void LoadShedStateManager::preserveStateValues(LoadShedState& old_state, LoadShedState& new_state) {
    // Preserve load priorities if load count unchanged
    if (old_state.loads.size() == new_state.loads.size()) {
        for (size_t i = 0; i < new_state.loads.size(); ++i) {
            // Preserve priority if load ID matches
            if (old_state.loads[i].id == new_state.loads[i].id) {
                new_state.loads[i].priority = old_state.loads[i].priority;
                new_state.loads[i].value_estimated = old_state.loads[i].value_estimated;
                new_state.loads[i].value_override = old_state.loads[i].value_override;
                new_state.loads[i].test_shed = old_state.loads[i].test_shed;
            }
        }
        
        // Preserve priority list
        if (old_state.priority_list.size() == new_state.priority_list.size()) {
            new_state.priority_list = old_state.priority_list;
        }
    }
    
    // Preserve contingency IRM values if contingency count unchanged
    if (old_state.contingencies.size() == new_state.contingencies.size()) {
        for (size_t i = 0; i < new_state.contingencies.size(); ++i) {
            if (old_state.contingencies[i].id == new_state.contingencies[i].id) {
                new_state.contingencies[i].irm_max = old_state.contingencies[i].irm_max;
                new_state.contingencies[i].irm_operator = old_state.contingencies[i].irm_operator;
                new_state.contingencies[i].enabled = old_state.contingencies[i].enabled;
                new_state.contingencies[i].operator_disable = old_state.contingencies[i].operator_disable;
            }
        }
    }
    
    // Preserve UF contingency values
    if (old_state.uf_contingencies.size() == new_state.uf_contingencies.size()) {
        for (size_t i = 0; i < new_state.uf_contingencies.size(); ++i) {
            if (old_state.uf_contingencies[i].id == new_state.uf_contingencies[i].id) {
                new_state.uf_contingencies[i].irm_max = old_state.uf_contingencies[i].irm_max;
                new_state.uf_contingencies[i].irm_operator = old_state.uf_contingencies[i].irm_operator;
                new_state.uf_contingencies[i].enabled = old_state.uf_contingencies[i].enabled;
            }
        }
    }
    
    // Preserve UF load to shed values
    new_state.uf_l1_load_to_shed = old_state.uf_l1_load_to_shed;
    new_state.uf_l2_load_to_shed = old_state.uf_l2_load_to_shed;
    
    // Preserve system flags
    new_state.test_mode = old_state.test_mode;
    new_state.lsp_a_disabled = old_state.lsp_a_disabled;
    new_state.lsp_b_disabled = old_state.lsp_b_disabled;
}

LoadShedState& LoadShedStateManager::getState() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_;
}

const LoadShedState& LoadShedStateManager::getState() const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    return state_;
}

const TagMapping* LoadShedStateManager::getTagMapping(const std::string& node_id, 
                                                      const std::string& property_key) const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    auto node_it = tag_mappings_.find(node_id);
    if (node_it == tag_mappings_.end()) {
        return nullptr;
    }
    
    auto prop_it = node_it->second.find(property_key);
    if (prop_it == node_it->second.end()) {
        return nullptr;
    }
    
    return &prop_it->second;
}

uint32_t LoadShedStateManager::getLoadIndex(const std::string& node_id) const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    auto it = load_node_map_.find(node_id);
    if (it != load_node_map_.end()) {
        return it->second;
    }
    return 0;
}

uint32_t LoadShedStateManager::getContingencyIndex(const std::string& node_id, bool is_uf) const {
    std::lock_guard<std::mutex> lock(state_mutex_);
    
    if (is_uf) {
        auto it = uf_contingency_node_map_.find(node_id);
        if (it != uf_contingency_node_map_.end()) {
            return it->second;
        }
    } else {
        auto it = contingency_node_map_.find(node_id);
        if (it != contingency_node_map_.end()) {
            return it->second;
        }
    }
    return 0;
}

} // namespace models
} // namespace microservice
