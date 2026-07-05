#include "reactor/simulator.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

using namespace reactor;

TEST(SimulatorTest, StartsAndStops) {
    Simulator sim;
    sim.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    sim.stop();

    // Should have ticked at least a couple of times (10 Hz, 350ms → ~3 ticks)
    EXPECT_GE(sim.get_tick_count(), 2U);
}

TEST(SimulatorTest, InitialStateIsNominal) {
    SimulatorConfig config;
    Simulator sim(config);

    // Before starting, state should be at nominal
    const ReactorState s = sim.get_reactor_state();
    EXPECT_NEAR(s.thermal_power, config.neutronics.nominal_power, 1e6);
    EXPECT_NEAR(s.fuel_temperature, config.fuel.ref_temperature, 1.0);
    EXPECT_NEAR(s.coolant_inlet_temp, config.coolant.inlet_temperature, 0.1);
    EXPECT_NEAR(s.pump_flow_rate, config.coolant.nominal_flow_rate, 1.0);
    EXPECT_NEAR(s.rod_position, 100.0, 0.1);
}

TEST(SimulatorTest, ControlRodCommandIsApplied) {
    Simulator sim;
    sim.start();

    sim.move_control_rods(50.0);
    // Wait enough ticks for the rod to start moving
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    sim.stop();

    // Rod should have moved from 100 toward 50 (speed 2%/s, 0.5s → moved ~1%)
    EXPECT_LT(sim.get_reactor_state().rod_position, 100.0);
}

TEST(SimulatorTest, PumpFlowRateCommandIsApplied) {
    Simulator sim;
    sim.start();

    sim.set_pump_flow_rate(10'000.0);
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    sim.stop();

    EXPECT_NEAR(sim.get_reactor_state().pump_flow_rate, 10'000.0, 1.0);
}

TEST(SimulatorTest, StableAtNominalConditions) {
    // Run for 1 second at nominal and check nothing blows up
    SimulatorConfig config;
    Simulator sim(config);
    sim.start();

    std::this_thread::sleep_for(std::chrono::seconds(1));

    sim.stop();

    // Power should remain near nominal (within 10%)
    const ReactorState s = sim.get_reactor_state();
    EXPECT_NEAR(s.thermal_power, config.neutronics.nominal_power,
                config.neutronics.nominal_power * 0.1);
    // Temperatures should be reasonable
    EXPECT_GT(s.fuel_temperature, 200.0);
    EXPECT_LT(s.fuel_temperature, 2000.0);
    EXPECT_GT(s.coolant_outlet_temp, 280.0);
    EXPECT_LT(s.coolant_outlet_temp, 400.0);
}
