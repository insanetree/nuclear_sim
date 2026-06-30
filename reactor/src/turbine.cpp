#include "reactor/turbine.h"

#include <algorithm>

namespace reactor {

namespace {
constexpr double kCelsiusToKelvin = 273.15;
}

Turbine::Turbine(const TurbineParams& params)
    : params_(params) {}

void Turbine::tick(std::chrono::duration<double> /*dt*/, const ReactorState& read, ReactorState& write) {
    // Convert heat delivered by the primary coolant into gross electrical
    // output. Efficiency is a fixed fraction of the ideal Carnot limit set by
    // the hot-leg (outlet) and condenser temperatures, so hotter coolant
    // converts more efficiently.
    double T_hot = read.coolant_outlet_temp + kCelsiusToKelvin;
    double T_cold = params_.condenser_temperature + kCelsiusToKelvin;
    double efficiency = params_.carnot_fraction * (1.0 - T_cold / T_hot);
    efficiency = std::clamp(efficiency, 0.0, 1.0);

    write.electrical_power = efficiency * read.heat_removal_rate;
}

} // namespace reactor
