#include "reactor/coolant_loop.h"

#include "reactor/constants.h"

namespace reactor {

void
CoolantLoop::tick(std::chrono::duration<double> dt, const ReactorState& read, ReactorState& write)
{
	double T_out = read.coolant_outlet_temp;
	double T_in = read.coolant_inlet_temp; // Set dynamically each tick by the secondary loop (Turbine).
	double flow = read.pump_flow_rate;

	// Heat input from fuel to coolant
	double T_coolant_avg = (T_in + T_out) / 2.0;
	double Q_core = constants::fuel::gap_conductance * (read.fuel_temperature - T_coolant_avg);

	// Heat removed by flow
	double Q_removed = flow * constants::coolant::specific_heat * (T_out - T_in);

	// Coolant outlet temperature (forward Euler)
	double dT_out_dt = (Q_core - Q_removed) / (constants::coolant::mass_in_core * constants::coolant::specific_heat);
	T_out += dT_out_dt * dt.count();

	// coolant_inlet_temp is owned by the Turbine module (secondary loop).
	write.coolant_outlet_temp = T_out;
	write.pump_flow_rate = read.pump_flow_rate;
	write.heat_removal_rate = Q_removed;
}

} // namespace reactor
