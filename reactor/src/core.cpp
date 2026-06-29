#include "reactor/core.h"

namespace reactor {

ReactorCore::ReactorCore(const NeutronicsParams& neutronics,
                         const FuelParams& fuel,
                         const ReactivityParams& reactivity)
    : neutronics_(neutronics)
    , fuel_(fuel)
    , reactivity_(reactivity) {}

void ReactorCore::tick(std::chrono::duration<double> dt, const ReactorState& read, ReactorState& write) {
    double n = read.neutron_population;
    double C = read.precursor_concentration;
    double T_fuel = read.fuel_temperature;
    double T_coolant = (read.coolant_inlet_temp + read.coolant_outlet_temp) / 2.0;

    // Reactivity: rod contribution + Doppler feedback
    double rho_rod = -reactivity_.rod_worth * (1.0 - read.rod_position / 100.0);
    double rho_doppler = reactivity_.doppler_coefficient * (T_fuel - fuel_.ref_temperature);
    double rho = rho_rod + rho_doppler;

    // Point kinetics (forward Euler)
    double dn_dt = ((rho - neutronics_.beta) / neutronics_.generation_time) * n
                   + neutronics_.lambda * C;
    double dC_dt = (neutronics_.beta / neutronics_.generation_time) * n
                   - neutronics_.lambda * C;

    n += dn_dt * dt.count();
    C += dC_dt * dt.count();

    // Clamp neutron population to prevent negative values
    if (n < 0.0) {
        n = 0.0;
    }

    // Thermal power
    double P_fission = n * neutronics_.nominal_power;

    // Fuel temperature (forward Euler)
    double Q_to_coolant = fuel_.gap_conductance * (T_fuel - T_coolant);
    double dT_fuel_dt = (P_fission - Q_to_coolant) / (fuel_.mass * fuel_.specific_heat);
    T_fuel += dT_fuel_dt * dt.count();

    write.neutron_population = n;
    write.precursor_concentration = C;
    write.reactivity = rho;
    write.thermal_power = P_fission;
    write.fuel_temperature = T_fuel;
}

} // namespace reactor
