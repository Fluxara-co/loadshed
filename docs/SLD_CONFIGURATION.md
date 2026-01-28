# SLD Configuration Format

## Overview

The LoadShed service receives configuration from ui-manager in ReactFlow SLD (Single Line Diagram) JSON format. The service dynamically determines the number of loads, contingencies, and topology breakers based on nodes in the SLD.

## SLD Node Identification

Each node in the SLD must have a `custom_properties.loadshed_role` property to identify its role in the loadshed system.

### Property Names

**TODO: Update ui-manager SLD config to include these properties**

#### `custom_properties.loadshed_role`

Identifies the node's role in the loadshed system:

- **`"load"`** - Identifies a sheddable load
- **`"contingency"`** - Identifies a contingency scenario
- **`"topology_breaker"`** - Identifies a topology breaker
- **`null` or missing** - Node is not part of loadshed system (ignored)

#### `custom_properties.contingency_type` (for contingency nodes)

Identifies the type of contingency:

- **`"normal"`** - Normal contingency (generator/line loss)
- **`"uf"`** - Under Frequency contingency
- **Default**: `"normal"` if not specified

## SLD JSON Structure

The SLD JSON follows this structure:

```json
{
  "export_version": "1.0",
  "export_type": "sld",
  "sld": {
    "name": "Main System Diagram",
    "description": "Complete electrical system",
    "nodes": [
      {
        "node_id": "uuid-here",
        "node_name": "L22-MCC CRITICAL",
        "node_type": "loads",
        "symbol_type": "loads_load",
        "custom_properties": {
          "loadshed_role": "load",
          "tagMappings": [
            {
              "tagName": "L22_LIVEPWR",
              "deviceName": "SEL-3354-LSP-PMS-B3",
              "propertyKey": "realPower",
              "transform": "direct",
              "isConstant": false
            },
            {
              "tagName": "L22_Status",
              "deviceName": "SEL-3354-LSP-PMS-B3",
              "propertyKey": "status",
              "transform": "direct",
              "isConstant": false
            },
            {
              "propertyKey": "label",
              "constantValue": "L22",
              "isConstant": true
            }
          ]
        },
        "electrical_properties": {},
        "is_active": true
      }
    ],
    "connections": [
      {
        "connection_id": "uuid-here",
        "from_node_id": "bus-uuid",
        "to_node_id": "load-uuid",
        "connection_type": "power",
        "is_active": true
      }
    ]
  }
}
```

## Node Examples

### Load Node

```json
{
  "node_id": "b346a984-29b5-4f36-b0b8-d4314ce296c6",
  "node_name": "L22-MCC CRITICAL-4FD-PLDP-2",
  "node_type": "loads",
  "symbol_type": "loads_load",
  "custom_properties": {
    "loadshed_role": "load",
    "tagMappings": [
      {
        "propertyKey": "label",
        "constantValue": "L22",
        "isConstant": true
      },
      {
        "tagName": "L22_LIVEPWR",
        "deviceName": "SEL-3354-LSP-PMS-B3",
        "propertyKey": "realPower",
        "transform": "direct",
        "isConstant": false
      },
      {
        "tagName": "L22_Status",
        "deviceName": "SEL-3354-LSP-PMS-B3",
        "propertyKey": "status",
        "transform": "direct",
        "isConstant": false
      },
      {
        "tagName": "L22_LSP_Bad_Status",
        "deviceName": "SEL-3354-LSP-PMS-B3",
        "propertyKey": "fault",
        "transform": "direct",
        "isConstant": false
      },
      {
        "tagName": "L22_TRIPIN",
        "deviceName": "SEL-3354-LSP-PMS-B3",
        "propertyKey": "tripIn",
        "transform": "direct",
        "isConstant": false
      }
    ]
  }
}
```

### Contingency Node (Normal)

```json
{
  "node_id": "contingency-uuid",
  "node_name": "C1 - Generator Loss",
  "node_type": "contingency",
  "symbol_type": "contingency_normal",
  "custom_properties": {
    "loadshed_role": "contingency",
    "contingency_type": "normal",
    "tagMappings": [
      {
        "propertyKey": "label",
        "constantValue": "C1",
        "isConstant": true
      },
      {
        "tagName": "C1_IRM_MAX",
        "deviceName": "SEL-3354-LSP-PMS-B3",
        "propertyKey": "irmMax",
        "transform": "direct",
        "isConstant": false
      }
    ]
  }
}
```

### Contingency Node (UF)

```json
{
  "node_id": "uf-contingency-uuid",
  "node_name": "UF1 - Under Frequency Level 1",
  "node_type": "contingency",
  "symbol_type": "contingency_uf",
  "custom_properties": {
    "loadshed_role": "contingency",
    "contingency_type": "uf",
    "tagMappings": [...]
  }
}
```

### Topology Breaker Node

```json
{
  "node_id": "breaker-uuid",
  "node_name": "52-F5",
  "node_type": "switching",
  "symbol_type": "switching_circuitBreaker",
  "custom_properties": {
    "loadshed_role": "topology_breaker",
    "tagMappings": [...]
  }
}
```

## Tag Mapping to Entity Fields

The service maps `propertyKey` values from tag mappings to entity fields:

| propertyKey | Entity Field | Entity Type | Description |
|-------------|-------------|-------------|-------------|
| `realPower` | `present_p` | LoadData | Current real power (kW) |
| `kpi3Value` | `present_p` | LoadData/ContingencyData | Real power value |
| `status` | `status` | LoadData/ContingencyData | Status (Good/Bad, Active/Inactive) |
| `fault` | `bad_status` | LoadData/ContingencyData | Communication/data quality fault |
| `offline` | `bad_status` | LoadData/ContingencyData | Offline status |
| `tripIn` | (command) | LoadData | Load trip command |
| `trip` | (command) | LoadData | Load trip command |
| `label` | `name` | LoadData/ContingencyData | Entity name (from constant) |
| `irmMax` | `irm_max` | ContingencyData | Maximum IRM limit |
| `irmOperator` | `irm_operator` | ContingencyData | Operator-set IRM |

## Electrical Node (Bus) Mapping

The service automatically maps loads and contingencies to buses via the `connections` array:

1. **Identify bus nodes**: Nodes where `node_type == "connections"` and `symbol_type == "connections_bus"`
2. **Assign bus numbers**: Sequential numbering starting from 1
3. **Map loads/contingencies**: Find connections where load/contingency is connected to a bus
4. **Assign `bus_conn`**: Set `bus_number` as the `bus_conn` value

## Configuration Update Flow

1. **ui-manager** sends SLD JSON via `POST /api/v1/loadshed/config/sld`
2. **LoadShed service** parses SLD and extracts:
   - Count of loads (nodes with `loadshed_role == "load"`)
   - Count of normal contingencies (`loadshed_role == "contingency"` and `contingency_type == "normal"`)
   - Count of UF contingencies (`loadshed_role == "contingency"` and `contingency_type == "uf"`)
   - Count of topology breakers (`loadshed_role == "topology_breaker"`)
3. **Service reinitializes** `LoadShedState` with new counts
4. **Service maps** SLD nodes to entities:
   - Load nodes → `LoadData` entities
   - Contingency nodes → `ContingencyData` entities
   - Tag mappings extracted and stored
   - Bus connections mapped
5. **Service responds** with updated configuration summary

## Runtime Updates

When SLD configuration is updated:

- **If counts changed**: State is reinitialized with new sizes
- **If only tag mappings changed**: Only tag mappings are updated (state preserved)
- **State preservation**: Priorities, IRM values, and other user-set values are preserved where possible

## API Endpoints

### POST /api/v1/loadshed/config/sld

Update SLD configuration.

**Request Body**: Complete SLD JSON

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

Get current loadshed state (all loads, contingencies, priorities, etc.).

## Startup Configuration

The service can be initialized from a file at startup using the `SLD_CONFIG_PATH` environment variable:

```bash
export SLD_CONFIG_PATH=/path/to/sld.json
./loadshed
```

The service will load and parse the SLD configuration on startup.

## Tag Mappings for MQTT

Tag mappings extracted from SLD are used to:

1. **Subscribe to MQTT topics**: Format `{device_name}/tags/{tag_name}`
2. **Update entity fields**: When MQTT messages are received, update corresponding entity fields
3. **Publish commands**: Send commands to devices via MQTT

Example MQTT topics:
- `SEL-3354-LSP-PMS-B3/tags/L22_LIVEPWR` → Updates `LoadData.present_p`
- `SEL-3354-LSP-PMS-B3/tags/L22_Status` → Updates `LoadData.status`

## Notes

- Nodes without `loadshed_role` or with `loadshed_role == null` are ignored
- Only active nodes (`is_active == true`) are processed
- Bus mapping is automatic based on connections
- Tag mappings are optional - entities will have default values if not mapped
- Configuration can be updated at runtime without service restart
