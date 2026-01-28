# Entity Model Mapping to RTAC Gateway

This document maps the C++ entity models to the RTAC Gateway IEC 61131-1 structures.

## Header File to Entity Model Mapping

| Header File | Entity Model | RTAC Source | Purpose |
|------------|--------------|-------------|---------|
| `models/loadshed_config.h` | `LoadShedConfig` | `CONSTANTS` (nLOAD, nCONT, etc.) | Configurable system limits |
| `models/load_data.h` | `LoadData` | `LOAD_DATA`, `LOAD_DATAOUT` | Represents a single sheddable load |
| `models/contingency_data.h` | `ContingencyData` | `GW_CONTINGENCY_DATA`, `CONT_DATA` | Represents a contingency scenario |
| `models/shed_flag.h` | `ShedFlagState`, `LoadTripOutputs` | `SHED_FLAG` function block | Processes and maintains shed flags |
| `models/loadshed_state.h` | `LoadShedState` | `GWB_TO_IOSRV_LSP_DATA` | Complete system state aggregation |
| `models/device_config.h` | `DeviceConfig` | N/A (new) | External LSP controller configuration |
| `models/controller_interface.h` | `ILoadShedController`, `ExternalLSPController`, `NativeLoadShedController` | N/A (new) | Controller abstraction layer |
| `models/mqtt_topic_config.h` | `MQTTTopicConfig` | N/A (new) | MQTT topic configuration |

## Detailed Mapping

### LoadData (`models/load_data.h`)

**RTAC Source**: `LOAD_DATA` and `LOAD_DATAOUT` structures

| C++ Field | RTAC Field | Type | Notes |
|-----------|-----------|------|-------|
| `id` | Load index (1-47) | uint32_t | Load identifier |
| `name` | `NAME` | string(32) | Load name |
| `status` | `LOAD_DATA.0` (Status) | LoadStatus enum | Good/Bad status |
| `test_shed` | `LOAD_DATA.1` (TEST_SHED) | bool | Test shed active |
| `value_override` | `LOAD_DATA.2` (VALUE_OVERRIDE) | bool | Manual value override |
| `trip` | `LOAD_DATA.3` (TRIP) | bool | Load trip command |
| `bad_status` | `LOAD_DATA.4` (BAD_STATUS) | bool | Communication/data quality issue |
| `alarm` | `LOAD_DATA.5` (ALARM) | bool | Load alarm condition |
| `value_estimated` | `VALUE_ESTIMATED` | double | Estimated load value (kW) |
| `present_p` | `PRESENT_P` | double | Current real power (kW) |
| `present_q` | `PRESENT_Q` | double | Current reactive power (kVAR) |
| `value` | `VALUE` | double | Measured value (kW) |
| `priority` | `Priority_List[i]` | uint16_t | Shedding priority (1-47, 0=not sheddable) |
| `inhibit` | `INHIBIT` | bool | Load inhibited from shedding |
| `shed` | `SHED` | bool | Load currently shed |
| `selected` | `SELECTED` | bool | Load selected for shedding |
| `bus_conn` | `BUS_CONN` | uint32_t | Bus connection identifier (bit-packed) |

**RTAC Bitpacking** (LOAD_DATA DWORD):
- Bit 0: Status
- Bit 1: TEST_SHED
- Bit 2: VALUE_OVERRIDE
- Bit 3: TRIP
- Bit 4: BAD_STATUS
- Bit 5: ALARM

### ContingencyData (`models/contingency_data.h`)

**RTAC Source**: `GW_CONTINGENCY_DATA` and `CONT_DATA` structures

| C++ Field | RTAC Field | Type | Notes |
|-----------|-----------|------|-------|
| `id` | Contingency index (1-8 or 1-6 for UF) | uint32_t | Contingency identifier |
| `name` | `Name` | string(32) | Contingency name |
| `type` | N/A (derived) | ContingencyType enum | Normal or UF |
| `status` | `CONTINGENCY_DATA.0` (Status) | ContingencyStatus enum | Contingency detected/active |
| `satisfied` | `CONTINGENCY_DATA.1` (Satisfied) | bool | Enough load identified |
| `enabled` | `CONTINGENCY_DATA.2` (ENABLED) | bool | Contingency enabled |
| `operator_disable` | `CONTINGENCY_DATA.3` (OPERATOR_DISABLE) | bool | Operator disabled |
| `bad_status` | `CONTINGENCY_DATA.4` (Bad_Status) | bool | Bad data quality |
| `alarm` | `CONTINGENCY_DATA.5` (Alarm) | bool | Contingency alarm |
| `irm_max` | `IRM_MAX` | double | Maximum IRM limit (kW) |
| `irm_operator` | `IRM_Operator` | double | Operator-set IRM (kW) |
| `irm_actual` | `IRM_Actual` | double | Actual IRM (kW) |
| `required_to_shed` | `Required_to_Shed` | double | Calculated load required (kW) |
| `load_to_shed` | `Load_to_Shed` | double | Available load selected (kW) |
| `present_p` | `Present_P` | double | Current real power (kW) |
| `present_q` | `Present_Q` | double | Current reactive power (kVAR) |
| `available_cap` | `Available_Cap` | double | Available generation capacity (kW) |
| `measured_load` | `Measured_Load` | double | Total measured load (kW) |
| `current_max_p` | `Current_Max_P` | double | Current maximum power (kW) |
| `select_list` | `Select_List` | vector<bool> | Bit array of selected loads (size = max_loads) |
| `inhibit_list` | `Inhibit_List` | vector<bool> | Bit array of inhibited loads (size = max_loads) |
| `load_bus_conn` | `LOAD_BUS_CONN` | uint32_t | Bus connection (bit-packed DWORD) |

**RTAC Bitpacking** (CONTINGENCY_DATA DWORD):
- Bit 0: Status
- Bit 1: Satisfied
- Bit 2: ENABLED
- Bit 3: OPERATOR_DISABLE
- Bit 4: Bad_Status
- Bit 5: Alarm

### ShedFlagState (`models/shed_flag.h`)

**RTAC Source**: `SHED_FLAG` function block

| C++ Field | RTAC Field | Type | Notes |
|-----------|-----------|------|-------|
| `shed_flags` | `SHED_FLAGS` | vector<ShedFlag> | 0=no shed, 1=UF, 2=contingency (size = max_loads) |
| `cont_flags` | `CONT_FLAGS` | vector<bool> | Contingency active flags (size = max_contingencies) |
| `uf_cont_flags` | `UF_CONT_FLAGS` | vector<bool> | UF contingency flags (size = max_uf_contingencies) |
| `load_shed_latch` | `LOAD_SHED_SR` | vector<bool> | Contingency shed latch state (size = max_loads) |
| `uf_load_shed_latch` | `UF_LOAD_SHED_SR` | vector<bool> | UF shed latch state (size = max_loads) |
| `cont_latch` | `CONT_SR` | vector<bool> | Contingency detection latch (size = max_contingencies) |
| `uf_cont_latch` | `UFCONT_SR` | vector<bool> | UF contingency latch (size = max_uf_contingencies) |

**RTAC Inputs**:
- `LOAD_TRIP_OUTPUTS_IN_A/B`: Array[1..4] of DWORD (bit-packed, 128 bits, 47 used)
- `UF_LOAD_TRIP_OUTPUTS_IN_A/B`: Array[1..4] of DWORD
- `CONT_DETECTED_A/B`: DWORD
- `CLEAR`: BOOL

**RTAC Logic**:
1. Extract bits from DWORD arrays
2. OR inputs from A and B (redundancy)
3. Use SR latches to maintain state until CLEAR
4. Prioritize contingency shed (2) over UF shed (1)

### LoadTripOutputs (`models/shed_flag.h`)

**RTAC Source**: `Load_Trip_Outputs` from LSP

| C++ Field | RTAC Field | Type | Notes |
|-----------|-----------|------|-------|
| `trip_outputs` | `Load_Trip_Outputs` | vector<uint32_t> | Bit-packed DWORD array (dynamically sized) |

**Bit Extraction**:
- Load 1-32: DWORD[0] bits 0-31
- Load 33-47: DWORD[1] bits 0-14
- DWORD[2] and DWORD[3] unused (reserved for future)

### LoadShedState (`models/loadshed_state.h`)

**RTAC Source**: `GWB_TO_IOSRV_LSP_DATA` structure

| C++ Field | RTAC Field | Type | Notes |
|-----------|-----------|------|-------|
| `config` | N/A | LoadShedConfig | Configuration (limits) |
| `loads` | `LOAD_DATA_ARRAY` | LoadArray (vector) | All loads (size = max_loads) |
| `contingencies` | `CONTINGENCY_DATA_ARRAY` | ContingencyArray (vector) | Normal contingencies (size = max_contingencies) |
| `uf_contingencies` | `UF_CONTINGENCY_DATA_ARRAY` | UFContingencyArray (vector) | UF contingencies (size = max_uf_contingencies) |
| `shed_flags` | `LoadFlag` | ShedFlagState | Shed flags state (initialized with config) |
| `priority_list` | `Priority_List` | vector<uint16_t> | Load priorities (size = max_loads) |
| `uf_l1_load_to_shed` | `UF_L1_Load_To_Shed` | double | Level 1 UF load (kW) |
| `uf_l2_load_to_shed` | `UF_L2_Load_To_Shed` | double | Level 2 UF load (kW) |
| `test_mode` | `HMI_TEST_MODE` | bool | Test mode active |
| `lsp_a_disabled` | `LSPA_DISABLED` | bool | LSP A disabled |
| `lsp_b_disabled` | `LSPB_DISABLED` | bool | LSP B disabled |
| `clear_pending` | `CLEAR` | bool | CLEAR command pending |

## Configuration

**Important**: The C++ implementation uses **configurable limits** instead of hardcoded constants. The RTAC code has fixed limits (47 loads, 8 contingencies, etc.), but our implementation allows these to be configured per project.

| RTAC Constant | C++ Configuration | Default | Notes |
|---------------|-------------------|---------|-------|
| `nLOAD` | `LoadShedConfig::max_loads` | 47 | Configurable maximum number of loads |
| `nCONT` | `LoadShedConfig::max_contingencies` | 8 | Configurable number of normal contingencies |
| `nUFCONT` | `LoadShedConfig::max_uf_contingencies` | 6 | Configurable number of UF contingencies |
| N/A | `LoadShedConfig::max_topology_breakers` | 15 | Configurable topology breakers |
| N/A | `LoadShedConfig::max_generators` | 6 | Configurable generators |
| N/A | `LoadShedConfig::max_buses` | 3 | Configurable buses |
| N/A | `DWORD_ARRAY_SIZE` | 4 | DWORD array size (128 bits, fixed) |

See `docs/CONFIGURABLE_LIMITS.md` for details on configuration.

## Simplifications Made

### 1. Removed RTAC-Specific Complexity
- **Bitpacking**: C++ uses separate fields instead of bit-packed DWORDs for clarity
- **Arrays**: Uses std::vector instead of fixed-size arrays (configurable sizes)
- **Types**: Uses standard C++ types (uint32_t, double, bool) instead of IEC types
- **Hardcoded Limits**: All limits are configurable via `LoadShedConfig` instead of constants

### 2. Added Modern C++ Features
- **Enums**: Strongly-typed enums instead of integer constants
- **std::vector<bool>**: For select/inhibit lists (configurable size) instead of fixed bitset
- **std::vector**: For all collections (configurable sizes) instead of fixed arrays
- **std::chrono**: For timestamps instead of custom time types
- **std::string**: For names instead of fixed-size char arrays
- **Configuration**: `LoadShedConfig` structure for project-specific limits

### 3. Abstraction Layers
- **Controller Interface**: Abstract base class for external and native controllers
- **Device Configuration**: JSON-based configuration for external LSP controllers
- **MQTT Topics**: Configurable topics per device instead of hardcoded

### 4. State Management
- **Single State Object**: `LoadShedState` aggregates all entities
- **Version Tracking**: State version for change detection
- **Timestamp Tracking**: Last updated timestamps for all entities

## MQTT Topic Mapping (External Controllers)

Where RTAC Gateway communicates with LSP via NGVL/DNP, we use MQTT topics:

| RTAC Communication | MQTT Topic Pattern | Direction |
|-------------------|-------------------|-----------|
| `LSP_B3_TO_GW_B.Load_Trip_Outputs` | `{device_id}/trip-outputs` | Subscribe |
| `LSP_B3_TO_GW_B.Contingency_Detected1` | `{device_id}/contingency` | Subscribe |
| `LSP_B3_TO_GW_B.LOAD_DATA_ARRAY` | `{device_id}/load-data` | Subscribe |
| `GW_B_TO_LSP_B3.LOAD[i].TEST_SHED` | `{device_id}/commands/test-shed` | Publish |
| `GW_B_TO_LSP_B3.CONTINGENCY[i].IRM_Operator` | `{device_id}/commands/irm` | Publish |
| `GW_B_TO_LSP_B3.Priority_List` | `{device_id}/commands/priority` | Publish |
| `GW_B_TO_LSP_B3.CLEAR` | `{device_id}/commands/clear` | Publish |

## Native Controller Placeholder

The `NativeLoadShedController` class is a placeholder for future native loadshed decision logic. Currently, it's a no-op implementation. Future implementation should include:

- Load selection algorithm based on priorities
- Contingency detection logic
- IRM calculation
- Priority-based shedding
- Integration with load-flow-service for power flow data

## Device Configuration Example

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
          "qos": 1,
          "retain": false
        },
        "contingency": {
          "subscribe_topic": "lsp_a/contingency",
          "qos": 1,
          "retain": false
        },
        "commands": {
          "publish_topic": "lsp_a/commands",
          "qos": 1,
          "retain": false
        }
      }
    }
  ]
}
```
