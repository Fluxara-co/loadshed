/**
 * @file sld_parser.cpp
 * @brief SLD configuration parser implementation
 */

#include "microservice/models/sld_parser.h"
#include "microservice/models/load_data.h"
#include "microservice/models/contingency_data.h"
#include "microservice/utils/logger.h"
#include <algorithm>
#include <sstream>

namespace microservice {
namespace models {

bool SLDConfigParser::parseSLD(const nlohmann::json& sld_json, SLDConfig& config) {
    try {
        // Extract SLD metadata
        if (sld_json.contains("sld")) {
            const auto& sld = sld_json["sld"];
            config.sld_name = sld.value("name", "");
            config.sld_description = sld.value("description", "");
            
            // Parse nodes from sld.nodes array
            if (sld.contains("nodes") && sld["nodes"].is_array()) {
                for (const auto& node_json : sld["nodes"]) {
                    SLDNode node;
                    if (parseNode(node_json, node)) {
                        config.nodes.push_back(node);
                    }
                }
            }
        } else {
            // Fallback: try direct nodes array
            if (sld_json.contains("nodes") && sld_json["nodes"].is_array()) {
                for (const auto& node_json : sld_json["nodes"]) {
                    SLDNode node;
                    if (parseNode(node_json, node)) {
                        config.nodes.push_back(node);
                    }
                }
            }
        }
        
        // Parse connections
        if (sld_json.contains("connections") && sld_json["connections"].is_array()) {
            parseConnections(sld_json["connections"], config.connections);
        }
        
        // Map bus connections
        mapBusConnections(config);
        
        // Categorize nodes by loadshed_role
        categorizeNodes(config);
        
        Logger::info("Parsed SLD: " + config.sld_name + 
                    " - " + std::to_string(config.nodes.size()) + " nodes, " +
                    std::to_string(config.load_count) + " loads, " +
                    std::to_string(config.normal_contingency_count) + " normal contingencies, " +
                    std::to_string(config.uf_contingency_count) + " UF contingencies");
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Failed to parse SLD: " + std::string(e.what()));
        return false;
    }
}

bool SLDConfigParser::parseNode(const nlohmann::json& node_json, SLDNode& node) {
    try {
        // Extract basic node properties
        node.node_id = node_json.value("node_id", "");
        node.node_name = node_json.value("node_name", "");
        node.node_type = node_json.value("node_type", "");
        node.symbol_type = node_json.value("symbol_type", "");
        node.description = node_json.value("description", "");
        node.is_active = node_json.value("is_active", true);
        
        // Extract custom_properties
        if (node_json.contains("custom_properties")) {
            node.custom_properties = node_json["custom_properties"];
            node.loadshed_role = extractLoadshedRole(node.custom_properties);
            node.contingency_type = extractContingencyType(node.custom_properties);
            
            // Extract tag mappings
            node.tag_mappings = extractTagMappings(node);
        }
        
        // Extract electrical_properties
        if (node_json.contains("electrical_properties")) {
            node.electrical_properties = node_json["electrical_properties"];
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Failed to parse node " + node.node_id + ": " + std::string(e.what()));
        return false;
    }
}

SLDNodeRole SLDConfigParser::extractLoadshedRole(const nlohmann::json& custom_props) {
    if (!custom_props.is_object()) {
        return SLDNodeRole::NONE;
    }
    
    if (custom_props.contains("loadshed_role")) {
        std::string role = custom_props["loadshed_role"];
        
        if (role == "load") {
            return SLDNodeRole::LOAD;
        } else if (role == "contingency") {
            return SLDNodeRole::CONTINGENCY;
        } else if (role == "topology_breaker") {
            return SLDNodeRole::TOPOLOGY_BREAKER;
        }
    }
    
    return SLDNodeRole::NONE;
}

SLDContingencyType SLDConfigParser::extractContingencyType(const nlohmann::json& custom_props) {
    if (!custom_props.is_object()) {
        return SLDContingencyType::NORMAL;
    }
    
    if (custom_props.contains("contingency_type")) {
        std::string type = custom_props["contingency_type"];
        if (type == "uf") {
            return SLDContingencyType::UF;
        }
    }
    
    return SLDContingencyType::NORMAL;
}

TagMappingMap SLDConfigParser::extractTagMappings(const SLDNode& node) {
    TagMappingMap mappings;
    
    if (!node.custom_properties.is_object()) {
        return mappings;
    }
    
    if (!node.custom_properties.contains("tagMappings") || 
        !node.custom_properties["tagMappings"].is_array()) {
        return mappings;
    }
    
    for (const auto& tag_mapping_json : node.custom_properties["tagMappings"]) {
        try {
            TagMapping mapping;
            
            // Extract basic properties
            mapping.tag_name = tag_mapping_json.value("tagName", "");
            mapping.device_name = tag_mapping_json.value("deviceName", "");
            mapping.property_key = tag_mapping_json.value("propertyKey", "");
            mapping.transform = tag_mapping_json.value("transform", "direct");
            mapping.is_constant = tag_mapping_json.value("isConstant", false);
            mapping.constant_value = tag_mapping_json.value("constantValue", "");
            
            // Extract transform options
            if (tag_mapping_json.contains("transformOptions")) {
                mapping.transform_options = tag_mapping_json["transformOptions"];
            }
            
            // Map property_key to entity field
            bool is_contingency = (node.loadshed_role == SLDNodeRole::CONTINGENCY);
            mapping.entity_field = mapPropertyKeyToEntityField(mapping.property_key, is_contingency);
            
            // Store mapping by property_key
            if (!mapping.property_key.empty()) {
                mappings[mapping.property_key] = mapping;
            }
            
        } catch (const std::exception& e) {
            Logger::warn("Failed to parse tag mapping: " + std::string(e.what()));
            continue;
        }
    }
    
    return mappings;
}

std::string SLDConfigParser::mapPropertyKeyToEntityField(const std::string& property_key, 
                                                          bool is_contingency) {
    // Common mappings
    if (property_key == "realPower" || property_key == "kpi3Value") {
        return is_contingency ? "present_p" : "present_p";
    }
    if (property_key == "status") {
        return is_contingency ? "status" : "status";
    }
    if (property_key == "fault" || property_key == "offline") {
        return is_contingency ? "bad_status" : "bad_status";
    }
    if (property_key == "tripIn" || property_key == "trip") {
        return is_contingency ? "" : "trip";  // Trip is a command, not a field
    }
    if (property_key == "label") {
        return "name";
    }
    
    // Load-specific mappings
    if (!is_contingency) {
        if (property_key == "kpi4Value") {
            return "present_q";  // Reactive power
        }
    }
    
    // Contingency-specific mappings
    if (is_contingency) {
        if (property_key == "irmMax") {
            return "irm_max";
        }
        if (property_key == "irmOperator") {
            return "irm_operator";
        }
    }
    
    return "";  // No mapping found
}

bool SLDConfigParser::parseConnections(const nlohmann::json& connections_json, 
                                       std::vector<SLDConnection>& connections) {
    try {
        for (const auto& conn_json : connections_json) {
            SLDConnection conn;
            conn.connection_id = conn_json.value("connection_id", "");
            conn.from_node_id = conn_json.value("from_node_id", "");
            conn.from_handle_id = conn_json.value("from_handle_id", "");
            conn.to_node_id = conn_json.value("to_node_id", "");
            conn.to_handle_id = conn_json.value("to_handle_id", "");
            conn.connection_type = conn_json.value("connection_type", "");
            conn.is_active = conn_json.value("is_active", true);
            
            connections.push_back(conn);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        Logger::error("Failed to parse connections: " + std::string(e.what()));
        return false;
    }
}

void SLDConfigParser::mapBusConnections(SLDConfig& config) {
    // 1. Identify bus nodes and create bus map
    uint32_t bus_num = 1;
    
    for (auto& node : config.nodes) {
        if (node.node_type == "connections" && 
            node.symbol_type == "connections_bus") {
            config.bus_map[node.node_id] = bus_num++;
            config.bus_count++;
        }
    }
    
    // 2. Map loads/contingencies to buses via connections
    for (auto& node : config.nodes) {
        if (node.loadshed_role == SLDNodeRole::LOAD ||
            node.loadshed_role == SLDNodeRole::CONTINGENCY) {
            
            // Find connection where this node is connected to a bus
            for (const auto& conn : config.connections) {
                if (!conn.is_active) continue;
                
                // Check if this node is connected to a bus
                if (conn.to_node_id == node.node_id) {
                    auto bus_it = config.bus_map.find(conn.from_node_id);
                    if (bus_it != config.bus_map.end()) {
                        node.bus_id = conn.from_node_id;
                        node.bus_number = bus_it->second;
                        break;
                    }
                }
                if (conn.from_node_id == node.node_id) {
                    auto bus_it = config.bus_map.find(conn.to_node_id);
                    if (bus_it != config.bus_map.end()) {
                        node.bus_id = conn.to_node_id;
                        node.bus_number = bus_it->second;
                        break;
                    }
                }
            }
        }
    }
}

void SLDConfigParser::categorizeNodes(SLDConfig& config) {
    // Clear filtered lists
    config.loads.clear();
    config.normal_contingencies.clear();
    config.uf_contingencies.clear();
    config.topology_breakers.clear();
    config.buses.clear();
    
    // Reset counts
    config.load_count = 0;
    config.normal_contingency_count = 0;
    config.uf_contingency_count = 0;
    config.topology_breaker_count = 0;
    
    // Categorize nodes
    for (auto& node : config.nodes) {
        if (!node.is_active) continue;
        
        switch (node.loadshed_role) {
            case SLDNodeRole::LOAD:
                config.loads.push_back(&node);
                config.load_count++;
                break;
                
            case SLDNodeRole::CONTINGENCY:
                if (node.contingency_type == SLDContingencyType::UF) {
                    config.uf_contingencies.push_back(&node);
                    config.uf_contingency_count++;
                } else {
                    config.normal_contingencies.push_back(&node);
                    config.normal_contingency_count++;
                }
                break;
                
            case SLDNodeRole::TOPOLOGY_BREAKER:
                config.topology_breakers.push_back(&node);
                config.topology_breaker_count++;
                break;
                
            default:
                // Check if it's a bus node
                if (node.node_type == "connections" && 
                    node.symbol_type == "connections_bus") {
                    config.buses.push_back(&node);
                }
                break;
        }
    }
}

LoadShedConfig SLDConfigParser::extractLoadShedConfig(const SLDConfig& sld_config) {
    LoadShedConfig config;
    
    // Set limits based on SLD counts
    config.max_loads = sld_config.load_count;
    config.max_contingencies = sld_config.normal_contingency_count;
    config.max_uf_contingencies = sld_config.uf_contingency_count;
    config.max_topology_breakers = sld_config.topology_breaker_count;
    config.max_buses = sld_config.bus_count;
    
    // Set generators count (from generation nodes)
    // Count generation nodes that aren't contingencies
    uint32_t gen_count = 0;
    for (const auto& node : sld_config.nodes) {
        if (node.node_type == "generation" && 
            node.loadshed_role != SLDNodeRole::CONTINGENCY) {
            gen_count++;
        }
    }
    config.max_generators = gen_count;
    
    return config;
}

std::vector<LoadData> SLDConfigParser::mapLoads(const SLDConfig& sld_config) {
    std::vector<LoadData> loads;
    loads.reserve(sld_config.loads.size());
    
    uint32_t load_index = 1;  // 1-based indexing
    
    for (const auto* load_node : sld_config.loads) {
        LoadData load;
        
        // Basic identification
        load.id = load_index++;
        load.name = load_node->node_name;
        
        // Extract name from label tag mapping if available
        const TagMapping* label_mapping = load_node->getTagMapping("label");
        if (label_mapping && !label_mapping->constant_value.empty()) {
            load.name = label_mapping->constant_value;
        }
        
        // Bus connection
        load.bus_conn = load_node->bus_number;  // Use bus_number as bus_conn
        
        // Extract values from tag mappings
        const TagMapping* real_power = load_node->getTagMapping("realPower");
        if (real_power) {
            // Tag name will be used for MQTT subscription
            // Actual value will come from MQTT messages
        }
        
        const TagMapping* status = load_node->getTagMapping("status");
        if (status) {
            // Status tag mapping stored for later use
        }
        
        const TagMapping* fault = load_node->getTagMapping("fault");
        if (fault) {
            // Fault tag mapping stored for later use
        }
        
        // Default values
        load.status = LoadStatus::GOOD;
        load.priority = 0;  // Will be set by operator
        load.value_estimated = 0.0;
        load.present_p = 0.0;
        
        loads.push_back(load);
    }
    
    return loads;
}

std::vector<ContingencyData> SLDConfigParser::mapContingencies(
    const SLDConfig& sld_config, bool is_uf) {
    
    const auto& cont_nodes = is_uf ? sld_config.uf_contingencies : 
                                      sld_config.normal_contingencies;
    
    std::vector<ContingencyData> contingencies;
    contingencies.reserve(cont_nodes.size());
    
    uint32_t cont_index = 1;  // 1-based indexing
    
    for (const auto* cont_node : cont_nodes) {
        ContingencyData cont;
        
        // Basic identification
        cont.id = cont_index++;
        cont.name = cont_node->node_name;
        
        // Convert SLDContingencyType to ContingencyType
        if (cont_node->contingency_type == SLDContingencyType::UF) {
            cont.type = ContingencyType::UF;
        } else {
            cont.type = ContingencyType::NORMAL;
        }
        
        // Extract name from label tag mapping if available
        const TagMapping* label_mapping = cont_node->getTagMapping("label");
        if (label_mapping && !label_mapping->constant_value.empty()) {
            cont.name = label_mapping->constant_value;
        }
        
        // Bus connection
        cont.load_bus_conn = cont_node->bus_number;
        
        // Resize select/inhibit lists (will be resized properly in state initialization)
        // For now, just initialize empty
        
        // Default values
        cont.status = ContingencyStatus::INACTIVE;
        cont.enabled = true;
        cont.irm_max = 0.0;
        cont.irm_operator = 0.0;
        cont.required_to_shed = 0.0;
        cont.load_to_shed = 0.0;
        
        contingencies.push_back(cont);
    }
    
    return contingencies;
}

} // namespace models
} // namespace microservice
