#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/aero/Aerodynamics.hpp"
#include "core/world/CelestialBody.hpp"

using namespace openorbit::aero;
using namespace openorbit::world;
using namespace Catch::Matchers;

TEST_CASE("flow conditions: stationary vehicle gives zero Mach", "[aero]") {
    physics::StateVector s;
    s.position = {6.778e6, 0.0, 0.0}; // 400 km altitude
    s.velocity = Eigen::Vector3d::Zero();
    s.attitude = Eigen::Quaterniond::Identity();
    s.mass_kg  = 1000.0;

    auto earth = makeEarth();
    auto atm   = earth.atmosphere.sample(400e3);

    FlowConditions fc = computeFlowConditions(s, atm);
    REQUIRE_THAT(fc.mach, WithinAbs(0.0, 1e-10));
    REQUIRE_THAT(fc.v_body_ms, WithinAbs(0.0, 1e-10));
}

TEST_CASE("flow conditions: supersonic at Mach 2", "[aero]") {
    auto earth = makeEarth();
    auto atm   = earth.atmosphere.sample(20000.0); // 20 km

    physics::StateVector s;
    s.position = {6.391e6, 0.0, 0.0}; // ~20 km altitude
    s.velocity = {2.0 * atm.speed_of_sound_ms, 0.0, 0.0}; // Mach 2 along x
    s.attitude = Eigen::Quaterniond::Identity();
    s.mass_kg  = 1000.0;

    FlowConditions fc = computeFlowConditions(s, atm);
    REQUIRE_THAT(fc.mach, WithinRel(2.0, 0.01));
}

TEST_CASE("aerodynamics: drag force opposes velocity", "[aero]") {
    auto earth = makeEarth();
    auto atm   = earth.atmosphere.sample(10000.0);

    physics::StateVector s;
    s.position = {kREarth + 10000.0, 0.0, 0.0};
    s.velocity = {100.0, 0.0, 0.0}; // moving in +x
    s.attitude = Eigen::Quaterniond::Identity();
    s.mass_kg  = 1000.0;

    VehicleAero aero;
    aero.reference_area_m2 = 10.0;
    aero.table = makeSlenderRocketAeroTable();

    auto out = computeAero(s, aero, atm);
    // Drag should oppose velocity (negative x in body frame)
    REQUIRE(out.force_N.x() < 0.0);
}

TEST_CASE("aerodynamics: zero velocity gives zero forces", "[aero]") {
    auto earth = makeEarth();
    auto atm   = earth.atmosphere.sample(0.0);

    physics::StateVector s;
    s.position = {kREarth, 0.0, 0.0};
    s.velocity = Eigen::Vector3d::Zero();
    s.attitude = Eigen::Quaterniond::Identity();
    s.mass_kg  = 1000.0;

    VehicleAero aero;
    aero.reference_area_m2 = 10.0;
    aero.table = makeCapsuleAeroTable();

    auto out = computeAero(s, aero, atm);
    REQUIRE_THAT(out.force_N.norm(), WithinAbs(0.0, 1e-10));
}

TEST_CASE("aerodynamics: heat flux is zero in vacuum", "[aero]") {
    AtmosphereState vacuum{};
    vacuum.density_kgm3      = 0.0;
    vacuum.pressure_Pa       = 0.0;
    vacuum.temperature_K     = 3.0;
    vacuum.speed_of_sound_ms = 0.0;

    physics::StateVector s;
    s.position = {6.778e6, 0.0, 0.0};
    s.velocity = {7800.0, 0.0, 0.0};
    s.attitude = Eigen::Quaterniond::Identity();
    s.mass_kg  = 1000.0;

    VehicleAero aero;
    aero.reference_area_m2 = 10.0;
    aero.table = makeCapsuleAeroTable();

    auto out = computeAero(s, aero, vacuum);
    REQUIRE_THAT(out.heat_flux_Wm2, WithinAbs(0.0, 1e-10));
    REQUIRE_THAT(out.force_N.norm(), WithinAbs(0.0, 1e-10));
}

TEST_CASE("US Standard Atmosphere: sea-level values match ISA", "[atmosphere]") {
    auto earth = makeEarth();
    auto atm   = earth.atmosphere.sample(0.0);

    REQUIRE_THAT(atm.pressure_Pa,      WithinRel(101325.0, 0.001));
    REQUIRE_THAT(atm.temperature_K,    WithinRel(288.15,   0.001));
    REQUIRE_THAT(atm.density_kgm3,     WithinRel(1.225,    0.01));
    REQUIRE(atm.speed_of_sound_ms > 340.0);
    REQUIRE(atm.speed_of_sound_ms < 341.0);
}

TEST_CASE("US Standard Atmosphere: density decreases with altitude", "[atmosphere]") {
    auto earth = makeEarth();
    double rho0  = earth.atmosphere.sample(0.0).density_kgm3;
    double rho10 = earth.atmosphere.sample(10000.0).density_kgm3;
    double rho80 = earth.atmosphere.sample(80000.0).density_kgm3;

    REQUIRE(rho0  > rho10);
    REQUIRE(rho10 > rho80);
    REQUIRE(rho80 < 0.001);
}

static constexpr double kREarth = 6.3781e6;
