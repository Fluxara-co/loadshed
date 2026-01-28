# LoadShed Service Migration Recommendations

## Executive Summary

After analyzing the RTAC Gateway loadshed logic (IEC 61131-1), this document provides recommendations on what functionality should be migrated to the new LoadShed C++ service.

## Architecture Context

The RTAC Gateway acts as a **data aggregator and coordinator**, not the primary decision maker. The actual loadshed decision logic (which loads to shed based on contingencies) resides in the **LSP (Load Shedding Processor)** controllers.

The Gateway's role:
- ✅ Receives and processes loadshed outputs from LSP controllers
- ✅ Provides operator interface (HMI) for configuration
- ✅ Coordinates between LSP, HMI, and GCS systems
- ✅ Manages priority lists and test shed commands
- ❌ Does NOT make primary loadshed decisions

## Recommended Migration Strategy

### Phase 1: Core Loadshed Processing (Essential)

#### 1.1 Shed Flag Processing Engine
**Priority**: **CRITICAL**

**What to Move**:
- SHED_FLAG function block logic
- Bit array extraction from DWORD arrays
- Set-Reset latch functionality for state management
- Shed flag generation (0=no shed, 1=UF shed, 2=contingency shed)

**C++ Implementation**:
```cpp
class ShedFlagProcessor {
    // Process Load_Trip_Outputs from LSP controllers
    // Extract bits from DWORD arrays
    // Maintain shed state with latches
    // Generate shed flags array
};
```

**Why Critical**: This is the core loadshed state tracking mechanism.

#### 1.2 Priority Management System
**Priority**: **CRITICAL**

**What to Move**:
- PSORT algorithm for maintaining unique, contiguous priorities
- PCHECK for finding maximum priority
- Dynamic priority adjustment when priorities change

**C++ Implementation**:
```cpp
class PriorityManager {
    // Maintain priority list (1-47)
    // Ensure uniqueness and contiguity
    // Handle priority changes dynamically
    // Find max priority
};
```

**Why Critical**: Loads are shed in priority order. This ensures correct sequencing.

#### 1.3 Contingency Status Tracker
**Priority**: **HIGH**

**What to Move**:
- Contingency detection and latching
- Contingency satisfaction tracking
- Multiple contingency handling (8 normal + 6 UF)

**C++ Implementation**:
```cpp
class ContingencyTracker {
    // Track 8 normal contingencies
    // Track 6 UF contingencies
    // Maintain state with latches
    // Handle CLEAR command
};
```

**Why Important**: Tracks which contingencies are active and satisfied.

### Phase 2: Load and Contingency Data Management

#### 2.1 Load Data Manager
**Priority**: **HIGH**

**What to Move**:
- Load status tracking (47 loads)
- Test shed state management
- Value override handling
- Load selection/inhibition lists per contingency

**C++ Implementation**:
```cpp
struct LoadData {
    bool status;
    bool test_shed;
    bool value_override;
    bool trip;
    bool bad_status;
    bool alarm;
    double value_estimated;
    double present_p;
    uint16_t priority;
    uint32_t bus_conn;
    bool inhibit;
    bool shed;
    bool selected;
};

class LoadManager {
    std::array<LoadData, 47> loads;
    // Manage load states
    // Handle test shed commands
    // Process value overrides
};
```

#### 2.2 Contingency Data Manager
**Priority**: **HIGH**

**What to Move**:
- IRM (Instantaneous Reserve Margin) calculation
- IRM limiter logic (based on generator limits)
- Required_to_Shed vs Load_to_Shed tracking
- Contingency enable/disable logic

**C++ Implementation**:
```cpp
struct ContingencyData {
    bool status;
    bool satisfied;
    bool enabled;
    bool operator_disable;
    bool bad_status;
    bool alarm;
    
    double irm_max;
    double irm_operator;
    double irm_actual;
    double required_to_shed;
    double load_to_shed;
    double present_p;
    double available_cap;
    double measured_load;
    
    std::bitset<47> select_list;  // Selected loads
    std::bitset<47> inhibit_list; // Inhibited loads
};

class ContingencyManager {
    std::array<ContingencyData, 8> normal_contingencies;
    std::array<ContingencyData, 6> uf_contingencies;
    // Manage contingency states
    // Calculate IRM values
    // Apply limiters
};
```

### Phase 3: Operator Interface and Coordination

#### 3.1 Operator Command Processor
**Priority**: **MEDIUM**

**What to Move**:
- Process operator commands from HMI
- IRM value setting and validation
- Priority list updates
- Test shed commands
- Value override commands
- CLEAR command processing

**C++ Implementation**:
```cpp
class OperatorCommandProcessor {
    // Process IRM_MAX, IRM_Operator commands
    // Handle priority list updates
    // Process test shed set/reset
    // Handle value overrides
    // Process CLEAR command
};
```

#### 3.2 Data Mapping Layer
**Priority**: **MEDIUM**

**What to Move**:
- Map data between internal structures and external interfaces
- Format data for LSP controllers
- Format data for HMI display
- Handle data source selection (LSP A vs B)

**C++ Implementation**:
```cpp
class DataMapper {
    // Map to/from LSP format
    // Map to/from HMI format
    // Select active data source
    // Handle data quality checks
};
```

### Phase 4: Advanced Features (Optional)

#### 4.1 Setpoint Discrepancy Checker
**Priority**: **LOW** (Only if multi-unit redundancy needed)

**What to Move**:
- Compare setpoints between redundant units
- Detect discrepancies with timers
- Generate discrepancy alarms

#### 4.2 Diagnostic and Alarm Management
**Priority**: **LOW** (Can use existing alarm-manager service)

**What to Move**:
- LSP communication status
- Data quality alarms
- Contingency alarms

## What NOT to Move

### Keep in LSP Controllers (or separate service):
1. **Primary Loadshed Decision Logic**
   - Which loads to select based on contingency
   - Load selection algorithm
   - Shed amount calculation
   - These are in the LSP controllers, not the Gateway

2. **Generator Control Logic**
   - IRM calculation from generator data
   - Spinning reserve calculation
   - These belong in GCS or generator control service

3. **HMI Display Logic**
   - Screen formatting
   - Display updates
   - These belong in the HMI/IOSRV service

## Implementation Plan

### Step 1: Core Engine (Week 1-2)
1. Implement ShedFlagProcessor
2. Implement PriorityManager
3. Implement basic state management (latches)

### Step 2: Data Structures (Week 2-3)
1. Define LoadData and ContingencyData structures
2. Implement LoadManager
3. Implement ContingencyManager

### Step 3: Operator Interface (Week 3-4)
1. Implement REST API endpoints for operator commands
2. Implement command validation
3. Implement data mapping

### Step 4: Integration (Week 4-5)
1. Integrate with MQTT for LSP communication
2. Integrate with existing services (load-flow-service, etc.)
3. Add monitoring and logging

### Step 5: Testing (Week 5-6)
1. Unit tests for core logic
2. Integration tests with LSP simulators
3. Test shed functionality verification

## API Design Recommendations

### REST Endpoints Needed

```
POST   /api/v1/loadshed/contingency/{id}/irm
       Set IRM_MAX or IRM_Operator for contingency

POST   /api/v1/loadshed/load/{id}/priority
       Set load priority

POST   /api/v1/loadshed/load/{id}/test-shed
       Enable/disable test shed for load

POST   /api/v1/loadshed/load/{id}/value-override
       Override load value

POST   /api/v1/loadshed/contingency/{id}/disable
       Disable contingency

POST   /api/v1/loadshed/clear
       Clear all shed flags

GET    /api/v1/loadshed/status
       Get current loadshed status

GET    /api/v1/loadshed/contingency/{id}
       Get contingency details

GET    /api/v1/loadshed/load/{id}
       Get load details
```

### MQTT Topics Needed

```
Subscriptions:
- loadshed/lsp/{id}/trip-outputs      # Load trip outputs from LSP
- loadshed/lsp/{id}/contingency       # Contingency detection from LSP
- loadshed/lsp/{id}/status            # LSP status updates

Publications:
- loadshed/gw/commands                # Commands to LSP
- loadshed/gw/status                  # Gateway status
- loadshed/gw/contingency-status      # Contingency status
- loadshed/gw/load-status              # Load status
```

## Data Models

### Load Model
```json
{
  "id": 1,
  "name": "Load_1",
  "status": "good",
  "priority": 1,
  "present_p": 1500.5,
  "value_estimated": 1500.0,
  "test_shed": false,
  "value_override": false,
  "shed": false,
  "inhibit": false,
  "bus_conn": 1,
  "bad_status": false,
  "alarm": false
}
```

### Contingency Model
```json
{
  "id": 1,
  "name": "C1",
  "status": "active",
  "satisfied": true,
  "enabled": true,
  "operator_disable": false,
  "irm_max": 5000.0,
  "irm_operator": 3000.0,
  "irm_actual": 2800.0,
  "required_to_shed": 2000.0,
  "load_to_shed": 2100.0,
  "present_p": 10000.0,
  "available_cap": 8000.0,
  "measured_load": 12000.0
}
```

## Testing Strategy

### Unit Tests
- ShedFlagProcessor: Test bit extraction, latching, flag generation
- PriorityManager: Test priority sorting, uniqueness, contiguity
- ContingencyTracker: Test detection, latching, satisfaction

### Integration Tests
- Test with LSP simulator sending Load_Trip_Outputs
- Test operator commands via REST API
- Test MQTT communication
- Test CLEAR functionality

### System Tests
- Test full loadshed sequence
- Test test shed functionality
- Test priority changes during active shedding
- Test multiple concurrent contingencies

## Migration Checklist

- [ ] Implement ShedFlagProcessor
- [ ] Implement PriorityManager
- [ ] Implement ContingencyTracker
- [ ] Define data structures (LoadData, ContingencyData)
- [ ] Implement LoadManager
- [ ] Implement ContingencyManager
- [ ] Create REST API endpoints
- [ ] Implement MQTT communication
- [ ] Add state persistence (config files or database)
- [ ] Add logging and monitoring
- [ ] Write unit tests
- [ ] Write integration tests
- [ ] Document API
- [ ] Performance testing

## Notes

1. **The Gateway is NOT the decision maker** - It processes outputs from LSP controllers
2. **LSP controllers make the actual loadshed decisions** - They calculate which loads to shed
3. **Gateway provides coordination** - Between LSP, HMI, and GCS
4. **Test shed is important** - Allows testing without real contingencies
5. **Priority management is critical** - Ensures loads are shed in correct order
6. **State persistence needed** - Shed flags and contingency states must persist
7. **Real-time requirements** - Loadshed processing must be fast (< 100ms)

## Questions to Resolve

1. Will the LoadShed service replace the Gateway entirely, or work alongside it?
2. Will LSP controllers still exist, or will decision logic move to LoadShed service?
3. What is the communication protocol with LSP? (Currently appears to be NGVL/DNP)
4. Do we need to support multiple redundant LSP controllers?
5. What is the expected response time for loadshed commands?
6. Do we need to persist state to database, or is config file sufficient?
