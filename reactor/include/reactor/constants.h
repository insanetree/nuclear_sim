#pragma once

#include <chrono>

// Fixed physics parameters for the plant model. These are compile-time
// constants (not runtime-configurable) since no caller ever varies them
// across instances; the only runtime-configurable quantities live in
// ReactorState/Commands (see reactor_state.h), e.g. steam_generator_effectiveness.
namespace reactor::constants {

namespace neutronics {
inline constexpr double beta = 0.0065;          // Delayed neutron fraction [Δk/k]
inline constexpr double lambda = 0.08;          // Precursor decay constant [1/s]
inline constexpr double generation_time = 0.08; // Prompt neutron generation time Λ [s]
inline constexpr double nominal_power = 3.0e9;  // Nominal thermal power [W]
} // namespace neutronics

namespace fuel {
inline constexpr double mass = 100'000.0;        // Fuel mass [kg]
inline constexpr double specific_heat = 300.0;   // Fuel specific heat [J/(kg·K)]
inline constexpr double gap_conductance = 1.0e7; // h_gap * A_gap [W/K]
inline constexpr double ref_temperature = 614.0; // Reference fuel temperature [°C]
} // namespace fuel

namespace reactivity {
inline constexpr double rod_worth = 0.08;                 // Total control rod worth [Δk/k]
inline constexpr double max_rod_speed = 2.0;              // Max rod speed [%/s]
inline constexpr double doppler_coefficient = -1.7284e-4; // Fuel temperature coefficient [Δk/k/°C]; sized so
                                                          // max Doppler feedback (as T_fuel -> coolant inlet
                                                          // temp at near-zero power) exactly offsets rod
                                                          // reactivity at rod_position = 30%, making the
                                                          // reactor subcritical below it.
} // namespace reactivity

namespace coolant {
inline constexpr double mass_in_core = 20'000.0;      // Coolant mass in core [kg]
inline constexpr double specific_heat = 4'180.0;      // Coolant specific heat [J/(kg·K)]
inline constexpr double inlet_temperature = 290.0;    // Initial cold-leg temperature [°C]; the secondary
                                                      // loop (Turbine) sets this dynamically every tick
                                                      // thereafter.
inline constexpr double nominal_flow_rate = 15'000.0; // Pump flow rate [kg/s]; fixed, not commandable.
} // namespace coolant

namespace turbine {
inline constexpr double condenser_temperature = 30.0;             // Turbine exhaust condensing temperature [°C]; the
                                                                  // Carnot cold-sink for electrical efficiency only.
                                                                  // Reached downstream of the turbine (after
                                                                  // expansion), not a bound on primary coolant
                                                                  // cooldown.
inline constexpr double carnot_fraction = 0.655;                  // Fraction of ideal Carnot efficiency achieved [-]
inline constexpr double secondary_saturation_temperature = 270.0; // Boiling point of the secondary-side
                                                                  // water in the steam generator [°C], set
                                                                  // by its operating pressure. This is the
                                                                  // pinch-point limit: the primary coolant
                                                                  // cannot be cooled below this
                                                                  // temperature, no matter the
                                                                  // effectiveness, since heat only flows
                                                                  // from hotter to colder fluid inside the
                                                                  // exchanger.
inline constexpr double steam_generator_effectiveness = 70.52;    // Initial/default percentage [0-100] of
                                                                  // the maximum possible heat (cooling the
                                                                  // primary coolant down to
                                                                  // secondary_saturation_temperature, the
                                                                  // pinch-point limit) that the secondary
                                                                  // loop extracts. Controls both
                                                                  // steam/electrical output and how much
                                                                  // the primary coolant is cooled before
                                                                  // returning to the core. Commandable at
                                                                  // runtime via
                                                                  // Simulator::set_steam_generator_effectiveness;
                                                                  // this value only seeds the initial
                                                                  // state. Default tuned so nominal
                                                                  // full-power operation (rods withdrawn)
                                                                  // reproduces the ~290/338 °C
                                                                  // inlet/outlet steady state.
} // namespace turbine

} // namespace reactor::constants

namespace reactor {

struct SimulatorConfig
{
	std::chrono::duration<double> tick_period{0.1}; // Tick period [s] (10 Hz)
};

} // namespace reactor
