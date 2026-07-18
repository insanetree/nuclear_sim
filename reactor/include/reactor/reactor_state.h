#pragma once

#include "reactor/constants.h"

#include <atomic>

namespace reactor {

// Default member initializers below mirror the *Params default values in
// constants.h (referenced directly, rather than duplicated as literals) so
// there is a single source of truth for nominal operating values. Fields
// with no direct *Params counterpart (rod_position/target, neutronics state,
// coolant_outlet_temp, heat_removal_rate, electrical_power) are steady-state
// derived quantities computed precisely by Coordinator::init_state; the
// literals here are just reasonable approximations for ad hoc/test use of a
// default-constructed ReactorState.
struct ReactorState
{
	// Control rods
	double rod_position = 100.0; // Current rod position [%], 0=fully inserted, 100=fully withdrawn
	double rod_target = 100.0;   // Target rod position [%]

	// Neutronics
	double neutron_population = 1.0;      // Normalized neutron population (1.0 = nominal)
	double precursor_concentration = 0.0; // Delayed neutron precursor concentration
	double reactivity = 0.0;              // Total reactivity [Δk/k]

	// Thermal
	double thermal_power = constants::neutronics::nominal_power; // Fission thermal power [W]
	double fuel_temperature = constants::fuel::ref_temperature;  // Fuel temperature [°C]

	// Coolant
	double coolant_inlet_temp = constants::coolant::inlet_temperature; // Coolant inlet temperature [°C]
	double coolant_outlet_temp = 338.0;                                // Coolant outlet temperature [°C]
	double pump_flow_rate = constants::coolant::nominal_flow_rate;     // Pump mass flow rate [kg/s]
	double heat_removal_rate = constants::neutronics::nominal_power;   // Heat removed by coolant [W]

	// Power conversion (secondary side)
	double electrical_power = 9.9e8; // Gross electrical power output [W]
	double steam_generator_effectiveness =
		constants::turbine::steam_generator_effectiveness; // Commanded secondary loop effectiveness [%, 0-100]
};

// Commands from external interface, applied atomically at tick boundaries
struct Commands
{
	std::atomic<double> rod_target{100.0};
	std::atomic<double> steam_generator_effectiveness_target{constants::turbine::steam_generator_effectiveness};
};

} // namespace reactor
