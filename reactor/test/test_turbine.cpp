#include "reactor/turbine.h"

#include <gtest/gtest.h>

using namespace reactor;

class TurbineTest : public ::testing::Test {
protected:
    TurbineParams params;
    Turbine turbine{params};
    ReactorState read;
    ReactorState write;
};

TEST_F(TurbineTest, ConvertsHeatToElectricalPower) {
    read.heat_removal_rate = 3.0e9;

    turbine.tick(std::chrono::duration<double>{0.1}, read, write);

    EXPECT_DOUBLE_EQ(write.electrical_power, params.thermal_efficiency * 3.0e9);
}

TEST_F(TurbineTest, ZeroHeatGivesZeroPower) {
    read.heat_removal_rate = 0.0;

    turbine.tick(std::chrono::duration<double>{0.1}, read, write);

    EXPECT_DOUBLE_EQ(write.electrical_power, 0.0);
}

TEST_F(TurbineTest, MoreHeatGivesMorePower) {
    read.heat_removal_rate = 1.0e9;
    turbine.tick(std::chrono::duration<double>{0.1}, read, write);
    double low = write.electrical_power;

    read.heat_removal_rate = 2.0e9;
    turbine.tick(std::chrono::duration<double>{0.1}, read, write);
    double high = write.electrical_power;

    EXPECT_GT(high, low);
}

TEST_F(TurbineTest, OutputNeverExceedsHeatInput) {
    read.heat_removal_rate = 3.0e9;

    turbine.tick(std::chrono::duration<double>{0.1}, read, write);

    EXPECT_LT(write.electrical_power, read.heat_removal_rate);
}
