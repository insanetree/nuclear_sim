#pragma once

#include <chrono>

namespace reactor {

struct NeutronicsParams
{
	double beta = 0.0065;          // Delayed neutron fraction [Δk/k]
	double lambda = 0.08;          // Precursor decay constant [1/s]
	double generation_time = 0.08; // Prompt neutron generation time Λ [s]
	double nominal_power = 3.0e9;  // Nominal thermal power [W]
};

struct FuelParams
{
	double mass = 100'000.0;        // Fuel mass [kg]
	double specific_heat = 300.0;   // Fuel specific heat [J/(kg·K)]
	double gap_conductance = 1.0e7; // h_gap * A_gap [W/K]
	double ref_temperature = 614.0; // Reference fuel temperature [°C]
};

struct ReactivityParams
{
	double rod_worth = 0.08;              // Total control rod worth [Δk/k]
	double max_rod_speed = 2.0;           // Max rod speed [%/s]
	double doppler_coefficient = -2.5e-5; // Fuel temperature coefficient [Δk/k/°C]
};

struct CoolantParams
{
	double mass_in_core = 20'000.0;      // Coolant mass in core [kg]
	double specific_heat = 4'180.0;      // Coolant specific heat [J/(kg·K)]
	double inlet_temperature = 290.0;    // Cold leg temperature [°C]
	double nominal_flow_rate = 15'000.0; // Nominal pump flow rate [kg/s]
	double max_flow_rate = 18'000.0;     // Maximum pump flow rate [kg/s]
};

struct TurbineParams
{
	double condenser_temperature = 30.0; // Cold-sink (condenser) temperature [°C]
	double carnot_fraction = 0.655;      // Fraction of ideal Carnot efficiency achieved [-]
};

struct SimulatorConfig
{
	std::chrono::duration<double> tick_period{0.1}; // Tick period [s] (10 Hz)
	NeutronicsParams neutronics;
	FuelParams fuel;
	ReactivityParams reactivity;
	CoolantParams coolant;
	TurbineParams turbine;
};

} // namespace reactor
