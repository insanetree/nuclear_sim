#pragma once

#include <atomic>

namespace reactor {

struct ReactorState {
    // Control rods
    double rod_position = 100.0;       // Current rod position [%], 0=fully inserted, 100=fully withdrawn
    double rod_target = 100.0;         // Target rod position [%]

    // Neutronics
    double neutron_population = 1.0;   // Normalized neutron population (1.0 = nominal)
    double precursor_concentration = 0.0; // Delayed neutron precursor concentration
    double reactivity = 0.0;           // Total reactivity [Δk/k]

    // Thermal
    double thermal_power = 3.0e9;      // Fission thermal power [W]
    double fuel_temperature = 614.0;   // Fuel temperature [°C]

    // Coolant
    double coolant_inlet_temp = 290.0; // Coolant inlet temperature [°C]
    double coolant_outlet_temp = 338.0;// Coolant outlet temperature [°C]
    double pump_flow_rate = 15'000.0;  // Pump mass flow rate [kg/s]
    double heat_removal_rate = 3.0e9;  // Heat removed by coolant [W]
};

// Commands from external interface, applied atomically at tick boundaries
struct Commands {
    std::atomic<double> rod_target{100.0};
    std::atomic<double> pump_flow_rate{15'000.0};
};

} // namespace reactor
