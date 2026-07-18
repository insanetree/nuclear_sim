#include "reactor/turbine.h"

#include "reactor/constants.h"

#include <algorithm>

namespace reactor {

namespace {
constexpr double kCelsiusToKelvin = 273.15;
}

void
Turbine::tick(std::chrono::duration<double> /*dt*/, const ReactorState& read, ReactorState& write)
{
	// Secondary loop / steam generator: extracts heat from the hot-leg
	// (outlet) coolant, bounded by the maximum possible if the coolant were
	// cooled all the way down to the secondary-side saturation temperature
	// (the pinch-point limit of the heat exchanger). The configured
	// effectiveness determines both how much steam (electrical power) is
	// generated and how much the primary coolant is cooled before it
	// returns to the core as the next tick's inlet temperature.
	double T_out = read.coolant_outlet_temp;
	double flow = read.pump_flow_rate;

	double Q_max = std::max(
		flow * constants::coolant::specific_heat * (T_out - constants::turbine::secondary_saturation_temperature), 0.0);
	double Q_extracted = (read.steam_generator_effectiveness / 100.0) * Q_max;

	double T_in_new = T_out;
	if (flow > 0.0) {
		T_in_new = T_out - Q_extracted / (flow * constants::coolant::specific_heat);
	}

	// Efficiency is a fixed fraction of the ideal Carnot limit set by the
	// hot-leg (outlet) and condenser temperatures, so hotter coolant
	// converts more efficiently.
	double T_hot = T_out + kCelsiusToKelvin;
	double T_cold = constants::turbine::condenser_temperature + kCelsiusToKelvin;
	double efficiency = constants::turbine::carnot_fraction * (1.0 - T_cold / T_hot);
	efficiency = std::clamp(efficiency, 0.0, 1.0);

	write.coolant_inlet_temp = T_in_new;
	write.electrical_power = efficiency * Q_extracted;
}

} // namespace reactor
