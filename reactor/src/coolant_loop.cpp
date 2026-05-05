#include "reactor/coolant_loop.h"

namespace reactor {

CoolantLoop::CoolantLoop(const CoolantParams& params, const FuelParams& fuel)
    : params_(params)
    , fuel_(fuel) {}

void CoolantLoop::tick(double dt, const ReactorState& read, ReactorState& write) {
    double T_out = read.coolant_outlet_temp;
    double T_in = params_.inlet_temperature;
    double flow = read.pump_flow_rate;

    // Heat input from fuel to coolant
    double T_coolant_avg = (T_in + T_out) / 2.0;
    double Q_core = fuel_.gap_conductance * (read.fuel_temperature - T_coolant_avg);

    // Heat removed by flow
    double Q_removed = flow * params_.specific_heat * (T_out - T_in);

    // Coolant outlet temperature (forward Euler)
    double dT_out_dt = (Q_core - Q_removed) / (params_.mass_in_core * params_.specific_heat);
    T_out += dT_out_dt * dt;

    write.coolant_inlet_temp = T_in;
    write.coolant_outlet_temp = T_out;
    write.pump_flow_rate = read.pump_flow_rate;
    write.heat_removal_rate = Q_removed;
}

} // namespace reactor
