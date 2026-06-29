#include "reactor/control_rods.h"

#include <gtest/gtest.h>

using namespace reactor;

class ControlRodsTest : public ::testing::Test {
protected:
    ReactivityParams params;
    ControlRods rods{params};
    ReactorState read;
    ReactorState write;
};

TEST_F(ControlRodsTest, NoMovementWhenAtTarget) {
    read.rod_position = 50.0;
    read.rod_target = 50.0;

    rods.tick(std::chrono::duration<double>{0.1}, read, write);

    EXPECT_DOUBLE_EQ(write.rod_position, 50.0);
}

TEST_F(ControlRodsTest, MovesUpTowardTarget) {
    read.rod_position = 50.0;
    read.rod_target = 60.0;

    rods.tick(std::chrono::duration<double>{1.0}, read, write);

    // max_rod_speed = 2%/s, dt = 1.0s → moves 2%
    EXPECT_DOUBLE_EQ(write.rod_position, 52.0);
}

TEST_F(ControlRodsTest, MovesDownTowardTarget) {
    read.rod_position = 50.0;
    read.rod_target = 40.0;

    rods.tick(std::chrono::duration<double>{1.0}, read, write);

    EXPECT_DOUBLE_EQ(write.rod_position, 48.0);
}

TEST_F(ControlRodsTest, ClampsToMaxSpeed) {
    read.rod_position = 0.0;
    read.rod_target = 100.0;

    rods.tick(std::chrono::duration<double>{0.5}, read, write);

    // max_rod_speed = 2%/s, dt = 0.5s → moves 1%
    EXPECT_DOUBLE_EQ(write.rod_position, 1.0);
}

TEST_F(ControlRodsTest, ClampsToRange) {
    read.rod_position = 99.5;
    read.rod_target = 100.0;

    rods.tick(std::chrono::duration<double>{1.0}, read, write);

    // Would move 0.5%, which is within max_step of 2%
    EXPECT_DOUBLE_EQ(write.rod_position, 100.0);
}

TEST_F(ControlRodsTest, DoesNotGoBelowZero) {
    read.rod_position = 0.5;
    read.rod_target = 0.0;

    rods.tick(std::chrono::duration<double>{1.0}, read, write);

    EXPECT_DOUBLE_EQ(write.rod_position, 0.0);
}

TEST_F(ControlRodsTest, SmallStepWhenCloseToTarget) {
    read.rod_position = 50.0;
    read.rod_target = 50.1;

    rods.tick(std::chrono::duration<double>{1.0}, read, write);

    // Error = 0.1 < max_step = 2.0, so moves exactly 0.1
    EXPECT_DOUBLE_EQ(write.rod_position, 50.1);
}
