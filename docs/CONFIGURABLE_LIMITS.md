# Configurable Limits

## Overview

The LoadShed service uses **configurable limits** instead of hardcoded values. This allows the service to be adapted to different projects with varying numbers of loads, contingencies, etc.

## Configuration Structure

The `LoadShedConfig` structure defines all configurable limits:

```cpp
struct LoadShedConfig {
    uint32_t max_loads;              // Maximum number of sheddable loads
    uint32_t max_contingencies;      // Maximum number of normal contingencies
    uint32_t max_uf_contingencies;   // Maximum number of UF contingencies
    uint32_t max_topology_breakers;  // Maximum number of topology breakers
    uint32_t max_generators;         // Maximum number of generators
    uint32_t max_buses;              // Maximum number of buses
};
```

## Default Values

The default constructor provides RTAC-compatible defaults (for backward compatibility):

- `max_loads`: 47
- `max_contingencies`: 8
- `max_uf_contingencies`: 6
- `max_topology_breakers`: 15
- `max_generators`: 6
- `max_buses`: 3

**Note**: These are just defaults. Actual values should be loaded from configuration file.

## Configuration File Format

Configuration should be loaded from JSON:

```json
{
  "loadshed": {
    "max_loads": 100,
    "max_contingencies": 12,
    "max_uf_contingencies": 8,
    "max_topology_breakers": 20,
    "max_generators": 10,
    "max_buses": 5
  }
}
```

## Changes from RTAC

### Before (Hardcoded)
```cpp
constexpr uint32_t MAX_LOADS = 47;
constexpr uint32_t MAX_CONTINGENCIES = 8;
std::array<LoadData, 47> loads;
std::bitset<47> select_list;
```

### After (Configurable)
```cpp
LoadShedConfig config;  // Loaded from file
std::vector<LoadData> loads;  // Resized to config.max_loads
std::vector<bool> select_list;  // Resized to config.max_loads
```

## Dynamic Sizing

All arrays have been changed to vectors:

| Old (Fixed) | New (Dynamic) |
|------------|---------------|
| `std::array<LoadData, 47>` | `std::vector<LoadData>` |
| `std::array<ContingencyData, 8>` | `std::vector<ContingencyData>` |
| `std::bitset<47>` | `std::vector<bool>` |
| `std::array<ShedFlag, 47>` | `std::vector<ShedFlag>` |

## Initialization

The `LoadShedState` must be initialized with configuration:

```cpp
LoadShedConfig config;
config.max_loads = 100;
config.max_contingencies = 12;
config.max_uf_contingencies = 8;

LoadShedState state;
state.initialize(config);
```

After initialization:
- All vectors are resized to match configuration
- Load IDs are set (1-based)
- Contingency IDs are set (1-based)
- Select/inhibit lists are resized
- Shed flags are initialized

## Validation

The configuration includes validation:

```cpp
bool isValid() const {
    return max_loads > 0 && 
           max_contingencies > 0 && 
           max_uf_contingencies > 0 &&
           max_loads <= 1000 &&      // Reasonable upper limit
           max_contingencies <= 100 &&
           max_uf_contingencies <= 100;
}
```

## Method Signatures Changed

Some methods now require the configuration or max values:

### Before
```cpp
bool isLoadSelected(uint32_t load_id) const {
    if (load_id == 0 || load_id > 47) return false;
    return select_list.test(load_id - 1);
}
```

### After
```cpp
bool isLoadSelected(uint32_t load_id, uint32_t max_loads) const {
    if (load_id == 0 || load_id > max_loads) return false;
    if (load_id > select_list.size()) return false;
    return select_list[load_id - 1];
}
```

## LoadTripOutputs

The `LoadTripOutputs` structure now dynamically resizes:

```cpp
void resizeForLoads(uint32_t max_loads) {
    uint32_t required_dwords = (max_loads + 31) / 32;  // Round up
    if (required_dwords > trip_outputs.size()) {
        trip_outputs.resize(required_dwords, 0);
    }
}
```

## Benefits

1. **Flexibility**: Adapt to any project size
2. **Scalability**: No need to recompile for different sizes
3. **Memory Efficiency**: Only allocate what's needed
4. **Maintainability**: Configuration in one place
5. **Testing**: Easy to test with different sizes

## Migration Notes

When migrating from RTAC:
1. Load configuration from file instead of using constants
2. Initialize `LoadShedState` with configuration
3. Pass `max_loads`/`max_contingencies` to methods that need bounds checking
4. Use `vector` instead of `array` for all collections
5. Use `vector<bool>` instead of `bitset<N>` for select/inhibit lists

## Example Usage

```cpp
// Load configuration
LoadShedConfig config;
config.max_loads = 100;
config.max_contingencies = 12;
config.max_uf_contingencies = 8;

// Initialize state
LoadShedState state;
state.initialize(config);

// Use state
LoadData* load = state.getLoad(50);  // Works for any load ID <= max_loads
ContingencyData* cont = state.getContingency(5);  // Works for any cont ID <= max_contingencies

// Check load selection
bool selected = cont->isLoadSelected(50, config.max_loads);
```
