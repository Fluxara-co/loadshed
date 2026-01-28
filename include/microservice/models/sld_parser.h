/**
 * @file sld_parser.h
 * @brief SLD configuration parser
 * 
 * Parses ReactFlow SLD JSON and extracts loadshed configuration
 */

#pragma once

#include "sld_config.h"
#include "loadshed_config.h"
#include "load_data.h"
#include "contingency_data.h"
#include <nlohmann/json.hpp>
#include <string>

namespace microservice {
namespace models {

/**
 * @brief SLD Configuration Parser
 * 
 * Parses ReactFlow SLD JSON and extracts loadshed-specific configuration
 */
class SLDConfigParser {
public:
    /**
     * @brief Parse SLD JSON and extract loadshed configuration
     * 
     * @param sld_json Complete SLD JSON from ui-manager
     * @param config Output SLD configuration
     * @return true if parsing successful
     */
    static bool parseSLD(const nlohmann::json& sld_json, SLDConfig& config);
    
    /**
     * @brief Extract LoadShedConfig from SLD configuration
     * 
     * Creates LoadShedConfig with counts derived from SLD
     * 
     * @param sld_config Parsed SLD configuration
     * @return LoadShedConfig with appropriate limits
     */
    static LoadShedConfig extractLoadShedConfig(const SLDConfig& sld_config);
    
    /**
     * @brief Map SLD load nodes to LoadData entities
     * 
     * @param sld_config Parsed SLD configuration
     * @return Vector of LoadData entities mapped from SLD nodes
     */
    static std::vector<LoadData> mapLoads(const SLDConfig& sld_config);
    
    /**
     * @brief Map SLD contingency nodes to ContingencyData entities
     * 
     * @param sld_config Parsed SLD configuration
     * @param is_uf If true, map UF contingencies; if false, map normal contingencies
     * @return Vector of ContingencyData entities mapped from SLD nodes
     */
    static std::vector<ContingencyData> mapContingencies(
        const SLDConfig& sld_config, bool is_uf = false);
    
    /**
     * @brief Extract tag mappings for a node
     * 
     * Parses custom_properties.tagMappings and creates TagMappingMap
     * 
     * @param node SLD node with custom_properties
     * @return Map of property_key -> TagMapping
     */
    static TagMappingMap extractTagMappings(const SLDNode& node);
    
    /**
     * @brief Map electrical nodes (buses) from connections
     * 
     * Identifies bus nodes and maps loads/contingencies to buses
     * 
     * @param config SLD configuration (modified in place)
     */
    static void mapBusConnections(SLDConfig& config);
    
private:
    /**
     * @brief Extract loadshed_role from custom_properties
     * 
     * Reads custom_properties.loadshed_role property
     * Expected values: "load", "contingency", "topology_breaker", or null/missing
     * 
     * @param custom_props custom_properties JSON object
     * @return SLDNodeRole enum value
     */
    static SLDNodeRole extractLoadshedRole(const nlohmann::json& custom_props);
    
    /**
     * @brief Extract contingency_type from custom_properties
     * 
     * Reads custom_properties.contingency_type property
     * Expected values: "normal", "uf", or defaults to "normal"
     * 
     * @param custom_props custom_properties JSON object
     * @return SLDContingencyType enum value
     */
    static SLDContingencyType extractContingencyType(const nlohmann::json& custom_props);
    
    /**
     * @brief Parse a single node from SLD JSON
     * 
     * @param node_json Node JSON object from SLD
     * @param node Output SLDNode structure
     * @return true if parsing successful
     */
    static bool parseNode(const nlohmann::json& node_json, SLDNode& node);
    
    /**
     * @brief Parse connections array from SLD JSON
     * 
     * @param connections_json Connections array from SLD
     * @param connections Output vector of SLDConnection
     * @return true if parsing successful
     */
    static bool parseConnections(const nlohmann::json& connections_json, 
                                 std::vector<SLDConnection>& connections);
    
    /**
     * @brief Map property_key to entity field name
     * 
     * Maps SLD propertyKey to LoadData/ContingencyData field names
     * 
     * @param property_key Property key from tag mapping
     * @param is_contingency If true, map for ContingencyData; if false, map for LoadData
     * @return Entity field name, or empty string if no mapping
     */
    static std::string mapPropertyKeyToEntityField(const std::string& property_key, 
                                                    bool is_contingency = false);
    
    /**
     * @brief Filter and categorize nodes by loadshed_role
     * 
     * Populates filtered lists (loads, contingencies, etc.) in config
     * 
     * @param config SLD configuration (modified in place)
     */
    static void categorizeNodes(SLDConfig& config);
};

} // namespace models
} // namespace microservice
