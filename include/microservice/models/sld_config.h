/**
 * @file sld_config.h
 * @brief SLD (Single Line Diagram) configuration structures
 * 
 * Structures for parsing and representing ReactFlow SLD JSON configuration
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "tag_mapping.h"

namespace microservice {
namespace models {

/**
 * @brief SLD node role for loadshed identification
 */
enum class SLDNodeRole {
    NONE = 0,
    LOAD = 1,
    CONTINGENCY = 2,
    TOPOLOGY_BREAKER = 3
};

/**
 * @brief Contingency type (already defined in contingency_data.h, but needed here)
 */
enum class SLDContingencyType {
    NORMAL = 0,
    UF = 1  // Under Frequency
};

/**
 * @brief SLD connection structure
 */
struct SLDConnection {
    std::string connection_id;
    std::string from_node_id;
    std::string from_handle_id;
    std::string to_node_id;
    std::string to_handle_id;
    std::string connection_type;  // "power", etc.
    bool is_active;
    
    SLDConnection()
        : is_active(true)
    {}
};

/**
 * @brief SLD Node representation
 * 
 * Represents a single node from the SLD JSON
 */
struct SLDNode {
    std::string node_id;          // UUID from SLD
    std::string node_name;        // Human-readable name
    std::string node_type;        // From SLD: "loads", "generation", "switching", "connections"
    std::string symbol_type;      // From SLD: "loads_load", "switching_circuitBreaker", etc.
    std::string description;      // Node description
    
    // Loadshed identification (from custom_properties)
    SLDNodeRole loadshed_role;    // "load", "contingency", "topology_breaker", or NONE
    SLDContingencyType contingency_type;  // For contingencies: "normal" or "uf"
    
    // Tag mappings (from custom_properties.tagMappings)
    TagMappingMap tag_mappings;   // property_key -> TagMapping
    
    // Electrical properties
    std::string bus_id;           // Connected bus node_id (derived from connections)
    uint32_t bus_number;          // Bus number (1, 2, 3, etc.) - derived from bus mapping
    
    // Raw JSON data
    nlohmann::json custom_properties;
    nlohmann::json electrical_properties;
    
    // Active status
    bool is_active;
    
    /**
     * @brief Default constructor
     */
    SLDNode()
        : loadshed_role(SLDNodeRole::NONE)
        , contingency_type(SLDContingencyType::NORMAL)
        , bus_number(0)
        , is_active(true)
    {}
    
    /**
     * @brief Check if node is part of loadshed system
     */
    bool isLoadshedNode() const {
        return loadshed_role != SLDNodeRole::NONE;
    }
    
    /**
     * @brief Get tag mapping by property key
     */
    const TagMapping* getTagMapping(const std::string& property_key) const {
        auto it = tag_mappings.find(property_key);
        if (it != tag_mappings.end()) {
            return &it->second;
        }
        return nullptr;
    }
};

/**
 * @brief SLD Configuration
 * 
 * Complete parsed SLD configuration with extracted loadshed information
 */
struct SLDConfig {
    std::string sld_name;         // SLD name from sld.name
    std::string sld_description;   // SLD description
    
    // All nodes from SLD
    std::vector<SLDNode> nodes;
    
    // All connections from SLD
    std::vector<SLDConnection> connections;
    
    // Derived counts (filtered by loadshed_role)
    uint32_t load_count;
    uint32_t normal_contingency_count;
    uint32_t uf_contingency_count;
    uint32_t topology_breaker_count;
    uint32_t bus_count;
    
    // Filtered node lists (for easy access)
    std::vector<SLDNode*> loads;              // Pointers to load nodes
    std::vector<SLDNode*> normal_contingencies;  // Pointers to normal contingency nodes
    std::vector<SLDNode*> uf_contingencies;     // Pointers to UF contingency nodes
    std::vector<SLDNode*> topology_breakers;    // Pointers to topology breaker nodes
    std::vector<SLDNode*> buses;               // Pointers to bus nodes
    
    // Bus mapping: bus node_id -> bus_number
    std::map<std::string, uint32_t> bus_map;
    
    /**
     * @brief Default constructor
     */
    SLDConfig()
        : load_count(0)
        , normal_contingency_count(0)
        , uf_contingency_count(0)
        , topology_breaker_count(0)
        , bus_count(0)
    {}
    
    /**
     * @brief Get load node by index
     */
    const SLDNode* getLoadNode(uint32_t index) const {
        if (index >= loads.size()) return nullptr;
        return loads[index];
    }
    
    /**
     * @brief Get contingency node by index
     */
    const SLDNode* getContingencyNode(uint32_t index, bool is_uf = false) const {
        if (is_uf) {
            if (index >= uf_contingencies.size()) return nullptr;
            return uf_contingencies[index];
        } else {
            if (index >= normal_contingencies.size()) return nullptr;
            return normal_contingencies[index];
        }
    }
};

} // namespace models
} // namespace microservice
