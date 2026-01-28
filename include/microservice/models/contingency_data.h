/**
 * @file contingency_data.h
 * @brief Contingency entity model - represents a contingency scenario
 * 
 * Based on RTAC Gateway GW_CONTINGENCY_DATA and CONT_DATA structures
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <chrono>

namespace microservice {
namespace models {

/**
 * @brief Contingency status enumeration
 */
enum class ContingencyStatus {
    INACTIVE = 0,
    ACTIVE = 1
};

/**
 * @brief Contingency type
 */
enum class ContingencyType {
    NORMAL = 0,      // Normal contingency (generator/line loss)
    UF = 1          // Under Frequency contingency
};

/**
 * @brief Contingency entity model
 * 
 * Represents a single contingency with IRM values and load shedding requirements.
 * Based on RTAC GW_CONTINGENCY_DATA structure.
 */
struct ContingencyData {
    // Basic identification
    uint32_t id;                    // Contingency ID (1-based)
    std::string name;               // Contingency name (max 32 chars)
    ContingencyType type;           // Normal or UF
    
    // Status flags (from CONTINGENCY_DATA bitpacking)
    ContingencyStatus status;       // Contingency detected/active
    bool satisfied;                 // Enough load identified to satisfy requirement
    bool enabled;                   // Contingency enabled
    bool operator_disable;          // Operator disabled this contingency
    bool bad_status;                // Bad data quality
    bool alarm;                     // Contingency alarm
    
    // IRM (Instantaneous Reserve Margin) values
    double irm_max;                 // Maximum IRM limit (kW)
    double irm_operator;            // Operator-set IRM (kW)
    double irm_actual;              // Actual IRM (min of operator and spinning reserve) (kW)
    
    // Load shedding values
    double required_to_shed;         // Calculated load required to shed (kW)
    double load_to_shed;            // Available load selected to be shed (kW)
    
    // Power measurements
    double present_p;                // Current real power (kW)
    double present_q;                // Current reactive power (kVAR)
    double available_cap;            // Available generation capacity (kW)
    double measured_load;            // Total measured load (kW)
    double current_max_p;           // Current maximum power (Present_P + IRM_Actual) (kW)
    
    // Load selection/inhibition (dynamic size based on max_loads)
    std::vector<bool> select_list;    // Bit array of loads selected for this contingency
    std::vector<bool> inhibit_list;   // Bit array of loads inhibited from shedding
    
    // Bus connection
    uint32_t load_bus_conn;         // Bus connection identifier (bit-packed DWORD)
    
    // Metadata
    std::chrono::system_clock::time_point last_updated;
    
    /**
     * @brief Default constructor
     */
    ContingencyData() 
        : id(0)
        , type(ContingencyType::NORMAL)
        , status(ContingencyStatus::INACTIVE)
        , satisfied(false)
        , enabled(true)
        , operator_disable(false)
        , bad_status(false)
        , alarm(false)
        , irm_max(0.0)
        , irm_operator(0.0)
        , irm_actual(0.0)
        , required_to_shed(0.0)
        , load_to_shed(0.0)
        , present_p(0.0)
        , present_q(0.0)
        , available_cap(0.0)
        , measured_load(0.0)
        , current_max_p(0.0)
        , load_bus_conn(0)
        , last_updated(std::chrono::system_clock::now())
    {}
    
    /**
     * @brief Check if contingency is active and needs shedding
     */
    bool needsShedding() const {
        return status == ContingencyStatus::ACTIVE && 
               enabled && 
               !operator_disable && 
               !satisfied &&
               required_to_shed > 0.0;
    }
    
    /**
     * @brief Check if load is selected for this contingency
     */
    bool isLoadSelected(uint32_t load_id, uint32_t max_loads) const {
        if (load_id == 0 || load_id > max_loads) return false;
        if (load_id > select_list.size()) return false;
        return select_list[load_id - 1];
    }
    
    /**
     * @brief Check if load is inhibited for this contingency
     */
    bool isLoadInhibited(uint32_t load_id, uint32_t max_loads) const {
        if (load_id == 0 || load_id > max_loads) return false;
        if (load_id > inhibit_list.size()) return false;
        return inhibit_list[load_id - 1];
    }
    
    /**
     * @brief Resize select/inhibit lists to match max_loads
     */
    void resizeLists(uint32_t max_loads) {
        select_list.resize(max_loads, false);
        inhibit_list.resize(max_loads, false);
    }
};

/**
 * @brief Vector of normal contingencies (size is configurable)
 */
using ContingencyArray = std::vector<ContingencyData>;

/**
 * @brief Vector of UF contingencies (size is configurable)
 */
using UFContingencyArray = std::vector<ContingencyData>;

} // namespace models
} // namespace microservice
