# RTAC Gateway Loadshed Logic Analysis

## Overview
This document analyzes the loadshed logic implemented in the RTAC Gateway (IEC 61131-1 PLC code). The Gateway acts as a data aggregator and coordinator between:
- **LSP (Load Shedding Processor)** - Controllers that make loadshed decisions
- **HMI/IOSRV (IO Server)** - Human Machine Interface for operator control
- **GCS (Generator Control System)** - Generator control and monitoring

## System Architecture

```
┌─────────────┐         ┌──────────────┐         ┌─────────────┐
│   LSP A/B   │◄────────┤   Gateway    │────────►│  HMI/IOSRV  │
│ (Decision   │         │  (Data       │         │  (Operator  │
│  Logic)     │         │  Mapping)    │         │  Interface) │
└─────────────┘         └──────────────┘         └─────────────┘
                              │
                              ▼
                        ┌─────────────┐
                        │     GCS     │
                        │ (Generator  │
                        │  Control)   │
                        └─────────────┘
```

## Key Constants

- **nLOAD**: 47 (Maximum number of sheddable loads)
- **nCONT**: 8 (Number of contingencies)
- **nUFCONT**: 6 (Number of Under Frequency contingencies)
- **nTOPO**: 15 (Number of topology breakers)

## Core Loadshed Components

### 1. SHED_FLAG Function Block
**Location**: `POUs/FUNCTION BLOCKS/SHED_FLAG.xml`

**Purpose**: Processes load trip outputs from LSP controllers and generates shed flags.

**Inputs**:
- `LOAD_TRIP_OUTPUTS_IN_A` / `LOAD_TRIP_OUTPUTS_IN_B`: Array[1..4] of DWORD
  - Bit-packed flags from LSP A and LSP B indicating which loads should be shed
  - Each DWORD contains 32 bits, total 128 loads (but only 47 used)
- `UF_LOAD_TRIP_OUTPUTS_IN_A` / `UF_LOAD_TRIP_OUTPUTS_IN_B`: Array[1..4] of DWORD
  - Under Frequency load trip outputs
- `CONT_DETECTED_A` / `CONT_DETECTED_B`: DWORD
  - Contingency detection flags
- `CLEAR`: BOOL
  - Clear/reset all shed flags

**Outputs**:
- `SHED_FLAGS`: Array[1..47] of INT
  - Values: 0 = No shed, 1 = UF shed, 2 = Contingency shed
- `CONT_FLAGS`: Array[1..8] of BOOL
  - Contingency active flags
- `UF_CONT_FLAGS`: Array[1..6] of BOOL
  - Under Frequency contingency flags

**Logic**:
1. Extracts individual load bits from DWORD arrays using EXTRACT function
2. Uses SR (Set-Reset) latches to maintain shed state until CLEAR
3. Prioritizes contingency shed (value 2) over UF shed (value 1)
4. Combines inputs from both LSP A and LSP B (OR logic)

**Key Code**:
```iec
FOR i:=1 TO nLOAD DO
    LOAD_SHED_SR[i](SET1:=LS_A[i] OR LS_B[i], RESET:=CLEAR);
    LS_FLAG[i]:=LOAD_SHED_SR[i].Q1;
    UF_LOAD_SHED_SR[i](SET1:=UF_A[i] OR UF_B[i], RESET:=CLEAR);
    UF_FLAG[i]:=UF_LOAD_SHED_SR[i].Q1;
END_FOR

FOR i:=1 TO nLOAD DO
    IF LS_FLAG[i] THEN
        SHED_FLAGS[i] := 2;  // Contingency shed
    ELSIF UF_FLAG[i] THEN
        SHED_FLAGS[i] := 1;  // UF shed
    ELSE
        SHED_FLAGS[i] := 0;  // No shed
    END_IF
END_FOR
```

### 2. Priority Management (PSORT)
**Location**: `POUs/FUNCTION BLOCKS/PSORT.xml`

**Purpose**: Manages load priorities to ensure unique, contiguous priority numbers.

**Key Features**:
- Ensures all non-zero priorities are unique
- Makes priorities contiguous (no gaps)
- Handles priority changes dynamically:
  - If priority decreases: increments all priorities >= new and < old
  - If priority increases: decrements all priorities <= new and > old
  - If priority set to 0: decrements all priorities > old
  - If priority set from 0: increments all priorities >= new

**Outputs**:
- `List_Out`: Sorted priority list
- `Max_Item`: Highest priority number
- `Changed`: Flag indicating if priorities changed

**Why Important**: Loads are shed in priority order. Priority management ensures proper sequencing.

### 3. Priority Check (PCHECK)
**Location**: `POUs/FUNCTION BLOCKS/PCHECK.xml`

**Purpose**: Finds the maximum priority number in the list.

**Output**: `Max_Item` - Used to determine how many priority levels exist.

### 4. Data Mapping Programs

#### P04_MAP_LSP_TO_GW_DATA
**Purpose**: Maps data from LSP controllers to Gateway internal data structures.

**Key Operations**:
- Maps contingency data (IRM, Required_to_Shed, Load_to_Shed, etc.)
- Maps load data (Status, Test_Shed, Value_Override, Trip, etc.)
- Processes Select_List and Inhibit_List for each contingency
- Creates X_Point mapping (0=not selected, 1=inhibited, 2=selected)
- Calls SHED_FLAG function block to process Load_Trip_Outputs
- Tracks contingency detection flags using SR latches

**Contingency Data Mapped**:
- `IRM_MAX`: Maximum IRM (Instantaneous Reserve Margin) limit
- `IRM_Operator`: Operator-set IRM value
- `IRM_Actual`: Actual IRM (min of Operator IRM and Spinning Reserve)
- `Required_to_Shed`: Calculated load that must be shed
- `Load_to_Shed`: Available load selected to be shed
- `Present_P`: Current real power
- `Available_Cap`: Available generation capacity
- `Measured_Load`: Total measured load
- `Select_List`: Bit array of loads selected for this contingency
- `Inhibit_List`: Bit array of loads inhibited from shedding

**Load Data Mapped**:
- `Status`: Load status (Good/Bad)
- `Test_Shed`: Test shed flag
- `Value_Override`: Manual value override flag
- `Trip`: Load trip command
- `Bad_Status`: Communication/data quality issue
- `Alarm`: Load alarm condition
- `PRESENT_P`: Current real power of load
- `BUS_CONN`: Bus connection identifier

#### P06_MAP_GW_TO_IOSRV_LSP_DATA
**Purpose**: Maps Gateway data to HMI/IO Server for display.

**Key Operations**:
- Maps contingency summary data to HMI points
- Maps load status data to HMI points
- Maps UF (Under Frequency) data
- Maps diagnostic information
- Maps test shed status for all 47 loads

**Data Sent to HMI**:
- Contingency status, IRM values, Required/S elected to Shed
- Load status, test shed flags, priority lists
- UF Level 1 and Level 2 load to shed values
- Diagnostic flags (LSP disabled, test mode, etc.)

#### P09_MAP_IOSRV_TO_GW_LSP_DATA
**Purpose**: Maps operator commands from HMI to Gateway.

**Key Operations**:
- Maps operator-set IRM_MAX and IRM_Operator values
- Maps operator disable commands for contingencies
- Maps load estimated values and priority lists
- Processes TEST_SHED commands (Set/Reset via SR latches)
- Processes VALUE_OVERRIDE commands
- Calls PSORT to manage priority list
- Calls PCHECK to find max priority

**Operator Commands Processed**:
- `IRM_MAX`: Maximum IRM limit per contingency
- `IRM_Operator`: Operator-set IRM value
- `OPERATOR_DISABLE`: Disable contingency shedding
- `VALUE_ESTIMATED`: Estimated load value
- `Priority_List`: Load priority assignments
- `TEST_SHED`: Test shed enable/disable per load
- `VALUE_OVERRIDE`: Override load value
- `CLEAR`: Clear all shed flags
- `DISABLE_LSP_A/B`: Disable LSP controllers
- `Test_Mode`: Enable test mode

#### P11_MAP_GW_TO_LSP_DATA
**Purpose**: Maps Gateway data back to LSP controllers.

**Key Operations**:
- Sends contingency IRM values to LSP (with limiter checks)
- Sends load test shed, value override, and estimated values
- Sends priority lists
- Sends UF load to shed values
- Sends diagnostic flags (disable LSP, test mode, clear)

**IRM Limiter Logic**:
- For certain contingencies, IRM_Operator is limited based on generator upper regulation limits
- Example: C1 IRM limited by B3_G1_KW_Upp_Reg_limit
- Prevents operator from setting IRM that would exceed generator capability

### 5. Contingency Management

**Contingency Types**:
1. **Normal Contingencies (nCONT = 8)**: Generator or line loss contingencies
2. **Under Frequency Contingencies (nUFCONT = 6)**: Frequency-based load shedding

**Contingency Data Structure**:
```iec
STRUCT
    Status: BOOL              // Contingency detected/active
    Satisfied: BOOL           // Enough load identified to satisfy requirement
    Enabled: BOOL             // Contingency enabled
    Operator_Disable: BOOL    // Operator disabled this contingency
    Bad_Status: BOOL          // Bad data quality
    Alarm: BOOL               // Contingency alarm
    
    IRM_MAX: REAL             // Maximum IRM limit
    IRM_Operator: REAL        // Operator-set IRM
    IRM_Actual: REAL          // Actual IRM (min of operator and spinning reserve)
    Required_to_Shed: REAL   // Calculated load required to shed
    Load_to_Shed: REAL       // Available load selected to shed
    Present_P: REAL          // Current real power
    Available_Cap: REAL       // Available generation capacity
    Measured_Load: REAL       // Total measured load
    
    Select_List: ARRAY[1..8] OF DWORD  // Bit array of selected loads
    Inhibit_List: ARRAY[1..8] OF DWORD // Bit array of inhibited loads
END_STRUCT
```

**Contingency Detection**:
- Detected via `Contingency_Detected1` DWORD from LSP
- Bits 0-7: Normal contingencies (C1-C8)
- Bits 8-13: UF contingencies (UF1-UF6)
- Uses SR latches to maintain state until CLEAR

### 6. Load Management

**Load Data Structure**:
```iec
STRUCT
    Status: BOOL              // Load status (Good/Bad)
    Test_Shed: BOOL           // Test shed active
    Value_Override: BOOL     // Manual value override
    Trip: BOOL                // Load trip command
    Bad_Status: BOOL          // Bad data quality
    Alarm: BOOL               // Load alarm
    
    VALUE_ESTIMATED: REAL    // Estimated load value
    PRESENT_P: REAL          // Current real power
    Priority: WORD           // Shedding priority (1-47, 0=not sheddable)
    BUS_CONN: DWORD          // Bus connection identifier
    INHIBIT: BOOL            // Load inhibited from shedding
    SHED: BOOL                // Load currently shed
    SELECTED: BOOL            // Load selected for shedding
END_STRUCT
```

**Load Priority System**:
- Each load has a priority (1-47, or 0 for not sheddable)
- Lower number = higher priority (shed first)
- Priorities must be unique and contiguous
- Managed by PSORT function block

### 7. Test Shed Functionality

**Purpose**: Allows operator to test loadshed without actual contingency.

**Mechanism**:
- Operator sets TEST_SHED via HMI (Close command = Set, Trip command = Reset)
- Uses SR (Set-Reset) latch to maintain state
- Test shed flag sent to LSP controllers
- LSP can use test shed to verify load control without real contingency

**Flow**:
1. Operator sets TEST_SHED on HMI
2. P09 maps to `IOSRV_TO_GWB_LSP_DATA.LOAD[i].TEST_SHED`
3. P11 sends to LSP: `GW_B_TO_LSP_B3.LOAD[i].TEST_SHED`
4. LSP can use this flag to trip load for testing

### 8. Under Frequency (UF) Load Shedding

**Purpose**: Separate load shedding for frequency-based contingencies.

**Key Data**:
- `UF_L1_Load_To_Shed`: Level 1 UF load to shed
- `UF_L2_Load_To_Shed`: Level 2 UF load to shed
- `UF_Load_Trip_Outputs`: Bit array of UF-shed loads
- `UF_CONTINGENCY_DATA_ARRAY`: Array of UF contingency data

**UF Contingencies**: 6 levels (UF1-UF6) for different frequency thresholds.

### 9. Setpoint Discrepancy Check (P08)

**Purpose**: Detects discrepancies between two Gateway units (GW and GW3).

**Checks**:
- Contingency IRM_MAX, IRM_Operator, OPERATOR_DISABLE
- Load VALUE_ESTIMATED, VALUE_OVERRIDE, Priority_List
- Diagnostic flags (Disable_LSP_A/B, UF_L1/L2_Load_To_Shed)

**Logic**:
- Compares setpoints between GW and GW3
- Uses timers for pickup and dropout
- Sets DISCREPANCY_ALARM if mismatch detected
- Disables priority sorting when discrepancy exists

## Data Flow Summary

### LSP → Gateway → HMI
1. LSP calculates loadshed requirements based on contingencies
2. LSP sends `Load_Trip_Outputs` (bit array) to Gateway
3. Gateway processes via SHED_FLAG function block
4. Gateway maps to HMI for display

### HMI → Gateway → LSP
1. Operator sets IRM values, priorities, test shed, etc. on HMI
2. Gateway receives via P09_MAP_IOSRV_TO_GW_LSP_DATA
3. Gateway processes (priority sorting, validation)
4. Gateway sends to LSP via P11_MAP_GW_TO_LSP_DATA

### Gateway Internal Processing
1. **P03_DATA_SELECTION**: Selects active LSP (A or B) based on communication status
2. **P04_MAP_LSP_TO_GW_DATA**: Processes LSP data, calls SHED_FLAG
3. **P06_MAP_GW_TO_IOSRV_LSP_DATA**: Sends data to HMI
4. **P08_SP_DISCREPANCY_CHECK**: Checks for setpoint mismatches
5. **P09_MAP_IOSRV_TO_GW_LSP_DATA**: Processes operator commands
6. **P11_MAP_GW_TO_LSP_DATA**: Sends commands to LSP

## Key Loadshed Logic Points

### 1. Shed Flag Processing
- **Input**: Bit-packed DWORD arrays from LSP (128 bits total, 47 used)
- **Processing**: Extract bits, OR from both LSPs, latch with SR
- **Output**: Integer array (0=no shed, 1=UF shed, 2=contingency shed)

### 2. Priority Management
- **Purpose**: Ensure loads are shed in correct order
- **Mechanism**: PSORT maintains unique, contiguous priorities
- **Dynamic**: Adjusts priorities when operator changes them

### 3. Contingency Status Tracking
- **Detection**: From LSP `Contingency_Detected1` DWORD
- **Latching**: SR latches maintain state until CLEAR
- **Types**: Normal (8) and UF (6) contingencies

### 4. Operator Controls
- **IRM Management**: Operator can set IRM_MAX and IRM_Operator
- **Load Priority**: Operator can assign priorities (1-47)
- **Test Shed**: Operator can test individual loads
- **Value Override**: Operator can override load values
- **Disable**: Operator can disable contingencies or LSPs

### 5. Data Quality
- **Bad_Status**: Flags for communication/data quality issues
- **Alarm**: General alarm conditions
- **Status**: Good/Bad status indicators

## What Should Move to LoadShed Service

### High Priority (Core Logic)
1. **SHED_FLAG Processing**
   - Extract load trip outputs from bit arrays
   - Maintain shed state with latches
   - Combine inputs from multiple sources (redundancy)
   - Generate shed flags (0/1/2)

2. **Priority Management**
   - PSORT algorithm for unique, contiguous priorities
   - PCHECK for max priority detection
   - Dynamic priority adjustment

3. **Contingency Status Tracking**
   - Contingency detection and latching
   - Contingency satisfaction checking
   - Multiple contingency handling

### Medium Priority (Data Management)
4. **Load Data Management**
   - Load status tracking
   - Test shed handling
   - Value override management
   - Load selection/inhibition lists

5. **Contingency Data Management**
   - IRM calculation and limiting
   - Required_to_Shed vs Load_to_Shed tracking
   - Contingency enable/disable logic

### Lower Priority (Interface/Display)
6. **HMI Data Mapping**
   - Data formatting for display
   - Operator command processing
   - Diagnostic information

7. **Setpoint Discrepancy Checking**
   - Multi-unit synchronization
   - Discrepancy detection and alarming

## Implementation Considerations

### Data Structures Needed
- Load array (47 loads) with status, priority, shed flags
- Contingency array (8 normal + 6 UF) with IRM, shed requirements
- Priority list management
- Bit array extraction utilities
- Set-Reset latch functionality

### State Management
- Shed flags must persist until CLEAR
- Contingency detection must latch
- Priority changes must be atomic

### Redundancy Handling
- Support multiple LSP inputs (A and B)
- OR logic for combining inputs
- Communication status monitoring

### Real-time Requirements
- Loadshed decisions must be fast (< 100ms typical)
- Priority sorting should be efficient
- State updates must be atomic

## Notes
- The actual loadshed **decision logic** (which loads to shed based on contingencies) appears to be in the LSP controllers, not the Gateway
- The Gateway primarily **aggregates**, **processes**, and **routes** loadshed data
- The Gateway provides **operator interface** and **coordination** between systems
- Test shed allows **testing without real contingencies**
