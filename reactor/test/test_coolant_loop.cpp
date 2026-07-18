#include "reactor/coolant_loop.h"

#include "reactor/constants.h"

#include <gtest/gtest.h>

#include <cmath>

using namespace reactor;

class CoolantLoopTest : public ::testing::Test
{
protected:
	CoolantLoop loop;
	ReactorState read;
	ReactorState write;

	void SetUp() override
	{
		read.fuel_temperature = constants::fuel::ref_temperature;        // ~614 °C
		read.coolant_inlet_temp = constants::coolant::inlet_temperature; // 290 °C
		// Steady-state outlet: T_out = T_in + P / (m_dot * c_p)
		read.coolant_outlet_temp = constants::coolant::inlet_temperature +
		                           3.0e9 / (constants::coolant::nominal_flow_rate * constants::coolant::specific_heat);
		read.pump_flow_rate = constants::coolant::nominal_flow_rate;
		read.thermal_power = 3.0e9;
	}
};

TEST_F(CoolantLoopTest, SteadyStateIsStable)
{
	// At steady state, Q_core ≈ Q_removed, so T_out should not change much
	loop.tick(std::chrono::duration<double>{0.1}, read, write);

	EXPECT_NEAR(write.coolant_outlet_temp, read.coolant_outlet_temp, 1.0);
}

TEST_F(CoolantLoopTest, ReducedFlowIncreasesOutletTemp)
{
	// Halving flow rate should cause outlet temp to rise
	read.pump_flow_rate = constants::coolant::nominal_flow_rate / 2.0;

	loop.tick(std::chrono::duration<double>{0.1}, read, write);

	// Q_removed drops because flow drops, so T_out increases
	EXPECT_GT(write.coolant_outlet_temp, read.coolant_outlet_temp);
}

TEST_F(CoolantLoopTest, IncreasedFlowDecreasesOutletTemp)
{
	// Doubling flow rate should cause outlet temp to fall
	read.pump_flow_rate = constants::coolant::nominal_flow_rate * 2.0;

	loop.tick(std::chrono::duration<double>{0.1}, read, write);

	EXPECT_LT(write.coolant_outlet_temp, read.coolant_outlet_temp);
}

TEST_F(CoolantLoopTest, HotterFuelIncreasesOutletTemp)
{
	// If fuel is much hotter, more heat flows to coolant
	read.fuel_temperature = 1000.0;

	loop.tick(std::chrono::duration<double>{0.1}, read, write);

	EXPECT_GT(write.coolant_outlet_temp, read.coolant_outlet_temp);
}

TEST_F(CoolantLoopTest, DoesNotWriteInletTemp)
{
	// coolant_inlet_temp is owned by the Turbine module (secondary loop);
	// CoolantLoop must not touch it.
	write.coolant_inlet_temp = -1.0;

	loop.tick(std::chrono::duration<double>{0.1}, read, write);

	EXPECT_DOUBLE_EQ(write.coolant_inlet_temp, -1.0);
}

TEST_F(CoolantLoopTest, FlowRateIsPreserved)
{
	read.pump_flow_rate = 12345.0;

	loop.tick(std::chrono::duration<double>{0.1}, read, write);

	EXPECT_DOUBLE_EQ(write.pump_flow_rate, 12345.0);
}
