# SLD-Based Dynamic Configuration Implementation Summary

## Overview

Successfully implemented SLD (Single Line Diagram) based dynamic configuration for the LoadShed service. The service now parses ReactFlow SLD JSON from ui-manager to dynamically determine the number of loads, contingencies, and topology breakers.

## Implementation Status

✅ **All core components implemented**

### Completed Components

1. ✅ **SLD Models** (`sld_config.h`, `tag_mapping.h`)
   - `SLDNode` structure with loadshed_role identification
   - `SLDConfig` structure with parsed configuration
   - `TagMapping` structure for device tag mappings
   - `SLDConnection` structure for electrical connections

2. ✅ **SLD Parser** (`sld_parser.h`, `sld_parser.cpp`)
   - Parses SLD JSON and extracts nodes by `loadshed_role`
   - Extracts tag mappings from `custom_properties.tagMappings`
   - Maps electrical nodes (buses) from connections array
   - Maps SLD nodes to `LoadData` and `ContingencyData` entities
   - Extracts `LoadShedConfig` with dynamic counts

3. ✅ **State Manager** (`loadshed_state_manager.h`, `loadshed_state_manager.cpp`)
   - Initializes `LoadShedState` from SLD configuration
   - Handles runtime configuration updates
   - Preserves state values when counts don't change
   - Maintains node index maps for lookups
   - Thread-safe state access

4. ✅ **REST API Endpoints**
   - `POST /api/v1/loadshed/config/sld` - Update SLD configuration
   - `GET /api/v1/loadshed/config/sld` - Get current configuration
   - `GET /api/v1/loadshed/state` - Get complete loadshed state

5. ✅ **Tag Mapping Extraction**
   - Extracts tag mappings from `custom_properties.tagMappings`
   - Maps `propertyKey` to entity fields
   - Supports constant values and device tags

6. ✅ **Bus Mapping**
   - Identifies bus nodes from connections
   - Maps loads/contingencies to buses via connections array
   - Assigns `bus_conn` values automatically

7. ✅ **Runtime Update Logic**
   - Detects when counts change
   - Reinitializes state when needed
   - Preserves user-set values (priorities, IRM) where possible

8. ✅ **Integration**
   - Integrated into `main.cpp`
   - Supports `SLD_CONFIG_PATH` environment variable for startup config
   - Added to CMakeLists.txt build system

## Property Names for ui-manager

**TODO: Update ui-manager SLD config to add these properties**

### Required Properties

Each node that should be part of the loadshed system must have:

```json
{
  "custom_properties": {
    "loadshed_role": "load" | "contingency" | "topology_breaker" | null
  }
}
```

For contingency nodes, also include:

```json
{
  "custom_properties": {
    "loadshed_role": "contingency",
    "contingency_type": "normal" | "uf"
  }
}
```

## File Structure

```
loadshed/
├── include/microservice/models/
│   ├── sld_config.h              ✅ SLD node and config structures
│   ├── tag_mapping.h              ✅ Tag mapping structure
│   ├── sld_parser.h               ✅ SLD parser interface
│   └── loadshed_state_manager.h  ✅ State manager interface
├── src/models/
│   ├── sld_parser.cpp             ✅ SLD parsing implementation
│   └── loadshed_state_manager.cpp ✅ State management
├── src/
│   └── main.cpp                   ✅ Integrated SLD config endpoints
└── docs/
    ├── SLD_CONFIGURATION.md       ✅ SLD format documentation
    └── SLD_IMPLEMENTATION_SUMMARY.md ✅ This file
```

## API Endpoints

### POST /api/v1/loadshed/config/sld

Update SLD configuration at runtime.

**Request**: Complete SLD JSON in request body

**Response**:
```json
{
  "success": true,
  "message": "Configuration updated successfully",
  "loads_count": 47,
  "contingencies_count": 8,
  "uf_contingencies_count": 6,
  "topology_breakers_count": 15,
  "buses_count": 3,
  "sld_name": "Main System Diagram",
  "initialized": true
}
```

### GET /api/v1/loadshed/config/sld

Get current SLD-based configuration.

**Response**:
```json
{
  "initialized": true,
  "sld_name": "Main System Diagram",
  "loads_count": 47,
  "contingencies_count": 8,
  "uf_contingencies_count": 6,
  "topology_breakers_count": 15,
  "buses_count": 3,
  "config": {
    "max_loads": 47,
    "max_contingencies": 8,
    "max_uf_contingencies": 6,
    "max_topology_breakers": 15,
    "max_buses": 3,
    "max_generators": 6
  }
}
```

### GET /api/v1/loadshed/state

Get complete loadshed state including all loads, contingencies, priorities, etc.

## Tag Mapping

Tag mappings are extracted from `custom_properties.tagMappings` and mapped to entity fields:

| propertyKey | Entity Field | Description |
|-------------|-------------|-------------|
| `realPower` | `present_p` | Current real power (kW) |
| `status` | `status` | Load/contingency status |
| `fault` | `bad_status` | Communication fault |
| `tripIn` | (command) | Load trip command |
| `label` | `name` | Entity name |
| `irmMax` | `irm_max` | Maximum IRM limit |
| `irmOperator` | `irm_operator` | Operator-set IRM |

## Bus Mapping

The service automatically maps loads and contingencies to buses:

1. Identifies bus nodes: `node_type == "connections"` and `symbol_type == "connections_bus"`
2. Assigns sequential bus numbers (1, 2, 3, ...)
3. Maps loads/contingencies to buses via `connections` array
4. Sets `bus_conn` field in entities

## Runtime Configuration Updates

When SLD configuration is updated:

1. **Parse new SLD** → Extract counts and nodes
2. **Compare counts**:
   - If changed → Reinitialize state with new sizes
   - If unchanged → Update tag mappings only
3. **Preserve state**: Priorities, IRM values, and other user-set values are preserved where possible

## Startup Configuration

The service can be initialized from a file at startup:

```bash
export SLD_CONFIG_PATH=/path/to/sld.json
./loadshed
```

## Next Steps

1. **ui-manager Integration**: Update ui-manager to add `loadshed_role` and `contingency_type` properties to SLD nodes
2. **MQTT Integration**: Subscribe to MQTT topics based on tag mappings
3. **Data Updates**: Implement MQTT message handlers to update entity fields from tag values
4. **Testing**: Test with actual SLD JSON from ui-manager

## Notes

- All limits are now configurable (no hardcoded values)
- State is thread-safe with mutex protection
- Configuration can be updated at runtime without restart
- Tag mappings are stored for future MQTT subscription
- Bus mapping is automatic based on connections
