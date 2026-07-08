#include "reactor/turbine.h"

#include <gtest/gtest.h>

using namespace reactor;

class TurbineTest : public ::testing::Test
{
protected:
	TurbineParams params;
	Turbine turbine{params};
	ReactorState read;
	ReactorState write;
};

TEST_F(TurbineTest, ConvertsHeatToElectricalPower)
{
	read.heat_removal_rate = 3.0e9;

	turbine.tick(std::chrono::duration<double>{0.1}, read, write);

	double T_hot = read.coolant_outlet_temp + 273.15;
	double T_cold = params.condenser_temperature + 273.15;
	double expected = params.carnot_fraction * (1.0 - T_cold / T_hot) * 3.0e9;
	EXPECT_DOUBLE_EQ(write.electrical_power, expected);
}

TEST_F(TurbineTest, ZeroHeatGivesZeroPower)
{
	read.heat_removal_rate = 0.0;

	turbine.tick(std::chrono::duration<double>{0.1}, read, write);

	EXPECT_DOUBLE_EQ(write.electrical_power, 0.0);
}

TEST_F(TurbineTest, MoreHeatGivesMorePower)
{
	read.heat_removal_rate = 1.0e9;
	turbine.tick(std::chrono::duration<double>{0.1}, read, write);
	double low = write.electrical_power;

	read.heat_removal_rate = 2.0e9;
	turbine.tick(std::chrono::duration<double>{0.1}, read, write);
	double high = write.electrical_power;

	EXPECT_GT(high, low);
}

TEST_F(TurbineTest, OutputNeverExceedsHeatInput)
{
	read.heat_removal_rate = 3.0e9;

	turbine.tick(std::chrono::duration<double>{0.1}, read, write);

	EXPECT_LT(write.electrical_power, read.heat_removal_rate);
}

TEST_F(TurbineTest, HotterOutletGivesHigherEfficiency)
{
	read.heat_removal_rate = 2.0e9;

	read.coolant_outlet_temp = 320.0;
	turbine.tick(std::chrono::duration<double>{0.1}, read, write);
	double cooler = write.electrical_power;

	read.coolant_outlet_temp = 360.0;
	turbine.tick(std::chrono::duration<double>{0.1}, read, write);
	double hotter = write.electrical_power;

	EXPECT_GT(hotter, cooler);
}
