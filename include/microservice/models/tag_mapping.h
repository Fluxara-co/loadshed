/**
 * @file tag_mapping.h
 * @brief Tag mapping structure for mapping SLD tag mappings to entity fields
 * 
 * Maps device tags from SLD custom_properties.tagMappings to LoadData/ContingencyData fields
 */

#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace microservice {
namespace models {

/**
 * @brief Tag mapping from SLD to entity fields
 * 
 * Represents a single tag mapping from custom_properties.tagMappings
 */
struct TagMapping {
    std::string tag_name;        // e.g., "L22_LIVEPWR"
    std::string device_name;     // e.g., "SEL-3354-LSP-PMS-B3"
    std::string property_key;    // e.g., "realPower", "status", "fault"
    std::string transform;       // "direct", "threshold", etc.
    nlohmann::json transform_options;  // Transform-specific options
    
    // Maps to entity field name
    std::string entity_field;    // e.g., "present_p", "status", "bad_status"
    
    // Additional metadata
    bool is_constant;            // True if constant value (not a tag)
    std::string constant_value;  // Constant value if is_constant is true
    
    /**
     * @brief Default constructor
     */
    TagMapping()
        : transform("direct")
        , is_constant(false)
    {}
    
    /**
     * @brief Check if this mapping has a valid tag
     */
    bool hasTag() const {
        return !is_constant && !tag_name.empty() && !device_name.empty();
    }
    
    /**
     * @brief Get MQTT topic for this tag (if applicable)
     * Format: {device_name}/tags/{tag_name}
     */
    std::string getMQTTTopic() const {
        if (!hasTag()) return "";
        return device_name + "/tags/" + tag_name;
    }
};

/**
 * @brief Tag mapping collection for a node
 * 
 * Maps property_key to TagMapping
 */
using TagMappingMap = std::map<std::string, TagMapping>;

} // namespace models
} // namespace microservice
