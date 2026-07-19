#include "reactor/core.h"

#include "reactor/constants.h"

namespace reactor {

void
ReactorCore::tick(std::chrono::duration<double> dt, const ReactorState& read, ReactorState& write)
{
	double n = read.neutron_population;
	double C = read.precursor_concentration;
	double T_fuel = read.fuel_temperature;
	double T_coolant = (read.coolant_inlet_temp + read.coolant_outlet_temp) / 2.0;

	// Reactivity: rod contribution + Doppler (fuel temp) + moderator (coolant temp) feedback
	double rho_rod = -constants::reactivity::rod_worth * (1.0 - read.rod_position / 100.0);
	double rho_doppler = constants::reactivity::doppler_coefficient * (T_fuel - constants::fuel::ref_temperature);
	double rho_moderator =
		constants::reactivity::moderator_coefficient * (T_coolant - constants::reactivity::moderator_ref_temperature);
	double rho = rho_rod + rho_doppler + rho_moderator;

	// Point kinetics (forward Euler)
	double dn_dt = ((rho - constants::neutronics::beta) / constants::neutronics::generation_time) * n +
	               constants::neutronics::lambda * C;
	double dC_dt =
		(constants::neutronics::beta / constants::neutronics::generation_time) * n - constants::neutronics::lambda * C;

	n += dn_dt * dt.count();
	C += dC_dt * dt.count();

	// Clamp neutron population to prevent negative values
	if (n < 0.0) {
		n = 0.0;
	}

	// Thermal power
	double P_fission = n * constants::neutronics::nominal_power;

	// Fuel temperature (forward Euler)
	double Q_to_coolant = constants::fuel::gap_conductance * (T_fuel - T_coolant);
	double dT_fuel_dt = (P_fission - Q_to_coolant) / (constants::fuel::mass * constants::fuel::specific_heat);
	T_fuel += dT_fuel_dt * dt.count();

	write.neutron_population = n;
	write.precursor_concentration = C;
	write.reactivity = rho;
	write.thermal_power = P_fission;
	write.fuel_temperature = T_fuel;
}

} // namespace reactor
