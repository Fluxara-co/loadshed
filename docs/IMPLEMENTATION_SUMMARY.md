# LoadShed Service Implementation Summary

## Overview

This document summarizes the simplified implementation of RTAC Gateway loadshed logic in the LoadShed C++ service.

## Entity Models Created

### Core Models

1. **`models/load_data.h`** - `LoadData`
   - Represents a single sheddable load (1-47)
   - Maps to RTAC `LOAD_DATA` and `LOAD_DATAOUT`
   - Includes status, values, priority, and shedding flags

2. **`models/contingency_data.h`** - `ContingencyData`
   - Represents a contingency scenario (8 normal + 6 UF)
   - Maps to RTAC `GW_CONTINGENCY_DATA` and `CONT_DATA`
   - Includes IRM values, shed requirements, and load selection

3. **`models/shed_flag.h`** - `ShedFlagState`, `LoadTripOutputs`
   - Processes load trip outputs from LSP controllers
   - Maps to RTAC `SHED_FLAG` function block
   - Maintains shed flags with Set-Reset latch behavior

4. **`models/loadshed_state.h`** - `LoadShedState`
   - Aggregates all loadshed entities
   - Maps to RTAC `GWB_TO_IOSRV_LSP_DATA`
   - Single source of truth for system state

5. **`models/device_config.h`** - `DeviceConfig`
   - Configuration for external LSP controllers
   - MQTT topic configuration per device
   - Device properties and redundancy groups

6. **`models/controller_interface.h`** - `ILoadShedController`
   - Abstract interface for controllers
   - `ExternalLSPController` - MQTT-based external controllers
   - `NativeLoadShedController` - Placeholder for native logic

7. **`models/mqtt_topic_config.h`** - `MQTTTopicConfig`
   - MQTT topic configuration structure
   - Subscribe/publish topics, QoS, retain flags

## Key Simplifications

### 1. Removed RTAC Complexity
- **No bitpacking**: Uses separate fields instead of DWORD bit arrays
- **Standard types**: Uses C++ standard types (uint32_t, double, bool)
- **Modern containers**: Uses std::array, std::bitset, std::string

### 2. Added Abstraction Layers
- **Controller Interface**: Abstract base class for external and native controllers
- **Device Configuration**: JSON-based configuration for external LSP controllers
- **MQTT Communication**: Configurable topics per device instead of hardcoded NGVL/DNP

### 3. State Management
- **Single State Object**: `LoadShedState` aggregates all entities
- **Version Tracking**: State version for change detection
- **Timestamps**: Last updated timestamps for all entities

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                  LoadShed Service                        │
│                                                          │
│  ┌──────────────────────────────────────────────────┐  │
│  │         LoadShedState (Single State)              │  │
│  │  - LoadArray (47 loads)                          │  │
│  │  - ContingencyArray (8 normal)                   │  │
│  │  - UFContingencyArray (6 UF)                     │  │
│  │  - ShedFlagState                                 │  │
│  │  - Priority lists, UF values, flags              │  │
│  └──────────────────────────────────────────────────┘  │
│                          │                               │
│         ┌────────────────┼────────────────┐             │
│         │                │                │             │
│  ┌──────▼──────┐  ┌─────▼─────┐  ┌──────▼──────┐     │
│  │  External   │  │  Native   │  │   REST API   │     │
│  │  LSP        │  │ Controller│  │   Server     │     │
│  │  Controller │  │(Placeholder)│ │              │     │
│  │  (MQTT)     │  │            │  │              │     │
│  └─────────────┘  └────────────┘  └──────────────┘     │
│         │                                                │
│         └────────────────┐                              │
│                          │                              │
│                  ┌───────▼────────┐                     │
│                  │  MQTT Broker   │                     │
│                  │  (External)   │                     │
│                  └────────────────┘                     │
└─────────────────────────────────────────────────────────┘
```

## Communication Flow

### External LSP Controllers (Current Implementation)

1. **Receive Data from LSP** (MQTT Subscribe):
   - `{device_id}/trip-outputs` → `LoadTripOutputs`
   - `{device_id}/contingency` → Contingency detection
   - `{device_id}/load-data` → Load data updates
   - `{device_id}/status` → Device status

2. **Send Commands to LSP** (MQTT Publish):
   - `{device_id}/commands/test-shed` → Test shed commands
   - `{device_id}/commands/irm` → IRM values
   - `{device_id}/commands/priority` → Priority lists
   - `{device_id}/commands/clear` → CLEAR command

### Native Controller (Placeholder)

- Currently a no-op implementation
- Future: Will implement loadshed decision logic
- Future: Will integrate with load-flow-service for power flow data

## Device Configuration

Devices are configured via JSON file:

```json
{
  "devices": [
    {
      "device_id": "lsp_a",
      "device_name": "LSP Controller A",
      "device_type": "LSP",
      "enabled": true,
      "redundant": true,
      "redundancy_group": "lsp_primary",
      "mqtt": {
        "trip_outputs": {
          "subscribe_topic": "lsp_a/trip-outputs",
          "qos": 1
        },
        "contingency": {
          "subscribe_topic": "lsp_a/contingency",
          "qos": 1
        },
        "commands": {
          "publish_topic": "lsp_a/commands",
          "qos": 1
        }
      }
    }
  ]
}
```

## Next Steps

### Phase 1: Core Implementation
1. Implement `ShedFlagProcessor` class
2. Implement `PriorityManager` class
3. Implement `ExternalLSPController` MQTT communication
4. Implement REST API endpoints

### Phase 2: Integration
1. Integrate with MQTT publisher/subscriber
2. Add device configuration loading
3. Add state persistence
4. Add monitoring and logging

### Phase 3: Native Controller (Future)
1. Implement load selection algorithm
2. Implement contingency detection
3. Integrate with load-flow-service
4. Add priority-based shedding logic

## File Structure

```
loadshed/
├── include/microservice/models/
│   ├── load_data.h              # Load entity
│   ├── contingency_data.h       # Contingency entity
│   ├── shed_flag.h              # Shed flag processing
│   ├── loadshed_state.h         # Complete state
│   ├── device_config.h          # Device configuration
│   ├── controller_interface.h   # Controller abstraction
│   └── mqtt_topic_config.h      # MQTT topic config
├── src/
│   └── models/                  # Implementation files (to be created)
│       ├── shed_flag_processor.cpp
│       ├── priority_manager.cpp
│       ├── external_lsp_controller.cpp
│       └── native_controller.cpp
└── docs/
    ├── ENTITY_MODEL_MAPPING.md  # RTAC to C++ mapping
    ├── RTAC_LOADSHED_LOGIC_ANALYSIS.md
    └── LOADSHED_MIGRATION_RECOMMENDATIONS.md
```

## Benefits of This Approach

1. **Simplified**: Removed RTAC-specific complexity (bitpacking, IEC types)
2. **Modern**: Uses modern C++ features (enums, std::array, std::bitset)
3. **Flexible**: Device configuration allows multiple external controllers
4. **Extensible**: Controller interface allows adding native controller later
5. **Maintainable**: Clear separation of concerns, well-documented
6. **Testable**: Entity models are simple data structures, easy to test

## RTAC Mapping Reference

See `docs/ENTITY_MODEL_MAPPING.md` for detailed mapping of C++ entities to RTAC structures.
