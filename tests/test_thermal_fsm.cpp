#include <gtest/gtest.h>
#include "devices/ThermalFsm.h"

TEST(ThermalFsmTest, StartsNormal) {
    ThermalFsm fsm;
    EXPECT_EQ(fsm.state(), ThermalState::NORMAL);
}

TEST(ThermalFsmTest, NormalToWarnAtThreshold) {
    ThermalFsm fsm;
    fsm.update(ThermalFsm::WARN_C);
    EXPECT_EQ(fsm.state(), ThermalState::WARN);
}

TEST(ThermalFsmTest, NormalToCriticalSkipsWarn) {
    ThermalFsm fsm;
    fsm.update(ThermalFsm::CRITICAL_C + 1.0);
    EXPECT_EQ(fsm.state(), ThermalState::CRITICAL);
}

TEST(ThermalFsmTest, NormalToShutdownDirectly) {
    ThermalFsm fsm;
    fsm.update(ThermalFsm::SHUTDOWN_C + 1.0);
    EXPECT_EQ(fsm.state(), ThermalState::SHUTDOWN);
}

TEST(ThermalFsmTest, WarnToCritical) {
    ThermalFsm fsm;
    fsm.update(ThermalFsm::WARN_C);
    fsm.update(ThermalFsm::CRITICAL_C);
    EXPECT_EQ(fsm.state(), ThermalState::CRITICAL);
}

TEST(ThermalFsmTest, WarnBackToNormalWithHysteresis) {
    ThermalFsm fsm;
    fsm.update(ThermalFsm::WARN_C);
    // Just below threshold but within hysteresis — stays WARN
    fsm.update(ThermalFsm::WARN_C - 1.0);
    EXPECT_EQ(fsm.state(), ThermalState::WARN);
    // Below hysteresis — returns to NORMAL
    fsm.update(ThermalFsm::WARN_C - ThermalFsm::HYSTERESIS - 0.1);
    EXPECT_EQ(fsm.state(), ThermalState::NORMAL);
}

TEST(ThermalFsmTest, CriticalBackToWarnWithHysteresis) {
    ThermalFsm fsm;
    fsm.update(ThermalFsm::CRITICAL_C);
    fsm.update(ThermalFsm::CRITICAL_C - ThermalFsm::HYSTERESIS - 0.1);
    EXPECT_EQ(fsm.state(), ThermalState::WARN);
}

TEST(ThermalFsmTest, ShutdownLatches) {
    ThermalFsm fsm;
    fsm.update(ThermalFsm::SHUTDOWN_C);
    EXPECT_EQ(fsm.state(), ThermalState::SHUTDOWN);
    // Even at low temp — stays latched
    fsm.update(20.0);
    EXPECT_EQ(fsm.state(), ThermalState::SHUTDOWN);
}

TEST(ThermalFsmTest, ResetFromShutdown) {
    ThermalFsm fsm;
    fsm.update(ThermalFsm::SHUTDOWN_C);
    fsm.reset();
    EXPECT_EQ(fsm.state(), ThermalState::NORMAL);
}

TEST(ThermalFsmTest, StateNameStrings) {
    EXPECT_STREQ(thermalStateName(ThermalState::NORMAL),   "NORMAL");
    EXPECT_STREQ(thermalStateName(ThermalState::WARN),     "WARN");
    EXPECT_STREQ(thermalStateName(ThermalState::CRITICAL), "CRITICAL");
    EXPECT_STREQ(thermalStateName(ThermalState::SHUTDOWN), "SHUTDOWN");
}
