#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/propulsion/Engine.hpp"

using namespace openorbit::propulsion;
using namespace Catch::Matchers;

TEST_CASE("atmosphericIsp: vacuum returns isp_vac", "[propulsion]") {
    double isp = atmosphericIsp(348.0, 311.0, 0.0);
    REQUIRE_THAT(isp, WithinRel(348.0, 1e-10));
}

TEST_CASE("atmosphericIsp: sea level returns isp_sl", "[propulsion]") {
    double isp = atmosphericIsp(348.0, 311.0, 101325.0);
    REQUIRE_THAT(isp, WithinRel(311.0, 1e-10));
}

TEST_CASE("atmosphericIsp: interpolates linearly", "[propulsion]") {
    double isp = atmosphericIsp(348.0, 311.0, 50662.5); // half pressure
    REQUIRE_THAT(isp, WithinRel(329.5, 1e-6));
}

TEST_CASE("massFlowRate: Merlin at vacuum thrust", "[propulsion]") {
    auto e = makeMerlin1DVac();
    e.throttle = 1.0;
    e.ignited  = true;
    double mdot = massFlowRate(e.thrust_vac_N, e.isp_vac_s);
    // ṁ = F / (Isp * g0) = 934000 / (348 * 9.80665) ≈ 273.5 kg/s
    REQUIRE_THAT(mdot, WithinRel(934000.0 / (348.0 * 9.80665), 1e-6));
}

TEST_CASE("EngineModel::compute: zero throttle gives zero force", "[propulsion]") {
    auto e = makeMerlin1DVac();
    e.ignited  = true;
    e.throttle = 0.0;
    auto out = e.compute(0.0, Eigen::Vector3d::Zero());
    REQUIRE_THAT(out.force_N.norm(), WithinAbs(0.0, 1e-10));
    REQUIRE_THAT(out.torque_Nm.norm(), WithinAbs(0.0, 1e-10));
    REQUIRE_THAT(out.mass_flow_kgs, WithinAbs(0.0, 1e-10));
}

TEST_CASE("EngineModel::compute: unignited gives zero force", "[propulsion]") {
    auto e = makeMerlin1DVac();
    e.ignited  = false;
    e.throttle = 1.0;
    auto out = e.compute(0.0, Eigen::Vector3d::Zero());
    REQUIRE_THAT(out.force_N.norm(), WithinAbs(0.0, 1e-10));
}

TEST_CASE("EngineModel::compute: full throttle vacuum thrust matches spec", "[propulsion]") {
    auto e = makeMerlin1DVac();
    e.ignited  = true;
    e.throttle = 1.0;
    auto out = e.compute(0.0, Eigen::Vector3d::Zero());
    // Thrust direction is {0,0,-1}, magnitude ≈ thrust_vac_N
    REQUIRE_THAT(out.force_N.norm(), WithinRel(e.thrust_vac_N, 0.05));
    REQUIRE(out.mass_flow_kgs < 0.0); // consumption
}

TEST_CASE("EngineModel::compute: gimbal deflection produces torque", "[propulsion]") {
    auto e = makeMerlin1DVac();
    e.ignited   = true;
    e.throttle  = 1.0;
    e.position  = {0.0, 0.0, -5.0}; // 5 m below CoM
    e.gimbal_range_deg = 5.0;
    e.gimbal_cmd = {1.0, 0.0}; // full pitch gimbal

    auto out = e.compute(0.0, Eigen::Vector3d::Zero());
    // Non-zero gimbal should produce a torque
    REQUIRE(out.torque_Nm.norm() > 1.0);
}
