#include "reactor/core.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace reactor;

class ReactorCoreTest : public ::testing::Test {
protected:
    NeutronicsParams neutronics;
    FuelParams fuel;
    ReactivityParams reactivity;
    ReactorCore core{neutronics, fuel, reactivity};
    ReactorState read;
    ReactorState write;

    void SetUp() override {
        // Start at steady-state nominal conditions
        read.rod_position = 100.0;
        read.neutron_population = 1.0;
        // Steady-state precursor: C = (β/Λ) * n / λ
        read.precursor_concentration =
            (neutronics.beta / neutronics.generation_time) / neutronics.lambda;
        read.fuel_temperature = 614.0;
        read.coolant_inlet_temp = 290.0;
        read.coolant_outlet_temp = 338.0;
        read.thermal_power = neutronics.nominal_power;
    }
};

TEST_F(ReactorCoreTest, SteadyStateIsStable) {
    // Rods fully withdrawn → rho_rod = 0, fuel at ref temp → rho_doppler = 0
    // Total rho = 0 → critical
    read.rod_position = 100.0;

    core.tick(0.1, read, write);

    // dn/dt = (0 - β)/Λ * n + λ*C = -0.08125 + 0.08125 = 0
    EXPECT_NEAR(write.reactivity, 0.0, 1e-10);
    EXPECT_NEAR(write.neutron_population, 1.0, 1e-6);
}

TEST_F(ReactorCoreTest, PositiveReactivityIncreasesPower) {
    // Cool fuel → positive Doppler feedback (rho_doppler > 0 when T < T_ref)
    read.rod_position = 100.0;
    read.fuel_temperature = fuel.ref_temperature - 200.0;

    core.tick(0.1, read, write);

    EXPECT_GT(write.reactivity, 0.0);
    EXPECT_GT(write.neutron_population, 1.0);
    EXPECT_GT(write.thermal_power, neutronics.nominal_power);
}

TEST_F(ReactorCoreTest, NegativeReactivityDecreasesPower) {
    // Rods partially inserted → negative rod reactivity
    read.rod_position = 50.0;
    // rho_rod = -0.08 * (1 - 0.5) = -0.04

    core.tick(0.1, read, write);

    EXPECT_LT(write.reactivity, 0.0);
    EXPECT_LT(write.neutron_population, 1.0);
}

TEST_F(ReactorCoreTest, FuelTemperatureChangesWithPower) {
    // If thermal power exceeds heat removal to coolant, fuel heats up
    // With very high neutron population and fuel near coolant temp (small Q_to_coolant)
    read.rod_position = 100.0;
    read.neutron_population = 2.0;
    read.precursor_concentration =
        (neutronics.beta / neutronics.generation_time) / neutronics.lambda * 2.0;
    // Set fuel temperature low so Q_to_coolant is small relative to 6 GW
    read.fuel_temperature = 350.0;

    core.tick(0.1, read, write);

    // P_fission = 6 GW, Q_to_coolant = 1e7 * (350 - 314) = 3.6e8 W
    // dT/dt = (6e9 - 3.6e8) / (1e5 * 300) > 0 → fuel heats up
    EXPECT_GT(write.fuel_temperature, 350.0);
}

TEST_F(ReactorCoreTest, NeutronPopulationNeverNegative) {
    read.rod_position = 0.0;
    read.neutron_population = 1e-15;
    read.precursor_concentration = 0.0;
    read.fuel_temperature = fuel.ref_temperature + 5000.0;

    core.tick(0.1, read, write);

    EXPECT_GE(write.neutron_population, 0.0);
}
