#include "reactor/turbine.h"

#include "reactor/constants.h"

#include <gtest/gtest.h>

#include <algorithm>

using namespace reactor;

namespace {
// Heat extracted by the secondary loop, mirroring Turbine::tick's model.
double
expected_Q_extracted(double T_out, double flow, double effectiveness)
{
	double Q_max = std::max(
		flow * constants::coolant::specific_heat * (T_out - constants::turbine::secondary_saturation_temperature), 0.0);
	return (effectiveness / 100.0) * Q_max;
}
} // namespace

class TurbineTest : public ::testing::Test
{
protected:
	Turbine turbine;
	ReactorState read;
	ReactorState write;

	void SetUp() override
	{
		read.coolant_outlet_temp = 338.0;
		read.pump_flow_rate = constants::coolant::nominal_flow_rate;
		read.steam_generator_effectiveness = constants::turbine::steam_generator_effectiveness;
	}
};

TEST_F(TurbineTest, ConvertsHeatToElectricalPower)
{
	turbine.tick(std::chrono::duration<double>{0.1}, read, write);

	double Q_extracted =
		expected_Q_extracted(read.coolant_outlet_temp, read.pump_flow_rate, read.steam_generator_effectiveness);
	double T_hot = read.coolant_outlet_temp + 273.15;
	double T_cold = constants::turbine::condenser_temperature + 273.15;
	double expected = constants::turbine::carnot_fraction * (1.0 - T_cold / T_hot) * Q_extracted;
	EXPECT_DOUBLE_EQ(write.electrical_power, expected);
}

TEST_F(TurbineTest, OutletAtCondenserTempGivesZeroPower)
{
	read.coolant_outlet_temp = constants::turbine::condenser_temperature;

	turbine.tick(std::chrono::duration<double>{0.1}, read, write);

	EXPECT_DOUBLE_EQ(write.electrical_power, 0.0);
}

TEST_F(TurbineTest, HotterOutletGivesMorePower)
{
	read.coolant_outlet_temp = 320.0;
	turbine.tick(std::chrono::duration<double>{0.1}, read, write);
	double cooler = write.electrical_power;

	read.coolant_outlet_temp = 360.0;
	turbine.tick(std::chrono::duration<double>{0.1}, read, write);
	double hotter = write.electrical_power;

	EXPECT_GT(hotter, cooler);
}

TEST_F(TurbineTest, HigherEffectivenessGivesMorePowerAndMoreCooling)
{
	read.steam_generator_effectiveness = 10.0;
	turbine.tick(std::chrono::duration<double>{0.1}, read, write);
	double low_power = write.electrical_power;
	double low_T_in = write.coolant_inlet_temp;

	read.steam_generator_effectiveness = 50.0;
	turbine.tick(std::chrono::duration<double>{0.1}, read, write);
	double high_power = write.electrical_power;
	double high_T_in = write.coolant_inlet_temp;

	EXPECT_GT(high_power, low_power);
	// More effective extraction cools the returning coolant more.
	EXPECT_LT(high_T_in, low_T_in);
}

TEST_F(TurbineTest, ZeroEffectivenessLeavesCoolantUncooled)
{
	read.steam_generator_effectiveness = 0.0;

	turbine.tick(std::chrono::duration<double>{0.1}, read, write);

	EXPECT_DOUBLE_EQ(write.coolant_inlet_temp, read.coolant_outlet_temp);
	EXPECT_DOUBLE_EQ(write.electrical_power, 0.0);
}

TEST_F(TurbineTest, FullEffectivenessCoolsToSecondarySaturationTemp)
{
	read.steam_generator_effectiveness = 100.0;

	turbine.tick(std::chrono::duration<double>{0.1}, read, write);

	EXPECT_NEAR(write.coolant_inlet_temp, constants::turbine::secondary_saturation_temperature, 1e-9);
}

TEST_F(TurbineTest, InletTempNeverGoesBelowPinchPointLimit)
{
	read.steam_generator_effectiveness = 100.0;

	turbine.tick(std::chrono::duration<double>{0.1}, read, write);

	EXPECT_GE(write.coolant_inlet_temp, constants::turbine::secondary_saturation_temperature);
}

TEST_F(TurbineTest, InletTempNeverExceedsOutletTemp)
{
	turbine.tick(std::chrono::duration<double>{0.1}, read, write);

	EXPECT_LE(write.coolant_inlet_temp, read.coolant_outlet_temp);
}
