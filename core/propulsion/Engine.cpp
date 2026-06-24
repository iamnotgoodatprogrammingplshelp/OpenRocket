#include "core/propulsion/Engine.hpp"
#include <cmath>
#include <numbers>
#include <algorithm>

namespace openorbit::propulsion {

static constexpr double kG0 = 9.80665; // standard gravity [m/s²]

double atmosphericIsp(double isp_vac, double isp_sl,
                       double p_ambient, double p_sl) noexcept
{
    double pressure_ratio = std::clamp(p_ambient / p_sl, 0.0, 1.0);
    return isp_vac - (isp_vac - isp_sl) * pressure_ratio;
}

double massFlowRate(double thrust_N, double isp_s) noexcept {
    if (isp_s <= 0.0) return 0.0;
    return thrust_N / (isp_s * kG0);
}

EngineModel::ThrustOutput EngineModel::compute(
        double ambient_pressure,
        const Eigen::Vector3d& vehicle_com) const noexcept
{
    ThrustOutput out{};

    if (!ignited || throttle <= 0.0) return out;

    // Atmosphere-adjusted Isp and thrust
    double isp = atmosphericIsp(isp_vac_s, isp_sl_s, ambient_pressure);

    // Linear interpolation of vac/sl thrust with pressure
    double p_ratio = std::clamp(ambient_pressure / 101325.0, 0.0, 1.0);
    double raw_thrust = std::lerp(thrust_vac_N, thrust_sl_N, p_ratio);
    double actual_thrust = raw_thrust * throttle;

    // Apply gimbal deflection to direction
    // pitch = gimbal_cmd.x (rotation around Y), yaw = gimbal_cmd.y (around Z)
    double pitch = gimbal_cmd.x() * gimbal_range_deg * std::numbers::pi / 180.0;
    double yaw   = gimbal_cmd.y() * gimbal_range_deg * std::numbers::pi / 180.0;

    // Rotate the nominal direction by gimbal angles (small angle approximation)
    Eigen::Vector3d gimballed_dir = direction;
    // Rotation matrix around Y then Z
    Eigen::AngleAxisd rot_pitch(pitch, Eigen::Vector3d::UnitY());
    Eigen::AngleAxisd rot_yaw(yaw,   Eigen::Vector3d::UnitZ());
    gimballed_dir = (rot_yaw * rot_pitch) * direction;
    gimballed_dir.normalize();

    out.force_N        = gimballed_dir * actual_thrust;
    out.torque_Nm      = (position - vehicle_com).cross(out.force_N);
    out.mass_flow_kgs  = -massFlowRate(actual_thrust, isp); // negative = consumption

    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Reference engine definitions
// ─────────────────────────────────────────────────────────────────────────────

EngineModel makeMerlin1DVac() {
    EngineModel e;
    e.name          = "Merlin 1D Vacuum";
    e.type          = EngineType::LiquidBipropellant;
    e.thrust_vac_N  = 934e3;   // 934 kN vacuum
    e.thrust_sl_N   = 845e3;   // ~845 kN SL (not used in vacuum variant)
    e.isp_vac_s     = 348.0;
    e.isp_sl_s      = 311.0;
    e.gimbal_range_deg = 5.0;
    e.min_throttle  = 0.40;    // 40% min throttle
    e.direction     = {0, 0, -1};
    return e;
}

EngineModel makeRaptor2Vac() {
    EngineModel e;
    e.name          = "Raptor 2 Vacuum";
    e.type          = EngineType::LiquidBipropellant;
    e.thrust_vac_N  = 2200e3;  // 2.2 MN
    e.thrust_sl_N   = 1960e3;
    e.isp_vac_s     = 380.0;
    e.isp_sl_s      = 350.0;
    e.gimbal_range_deg = 6.0;
    e.min_throttle  = 0.20;
    e.direction     = {0, 0, -1};
    return e;
}

EngineModel makeSolidRocket(double thrust_kN, double burn_time, double isp) {
    EngineModel e;
    e.name          = "Solid Rocket Motor";
    e.type          = EngineType::SolidRocket;
    e.thrust_vac_N  = thrust_kN * 1000.0;
    e.thrust_sl_N   = thrust_kN * 1000.0 * 0.92; // ~8% lower at sea level
    e.isp_vac_s     = isp;
    e.isp_sl_s      = isp * 0.90;
    e.min_throttle  = 1.0;  // solid: all or nothing
    e.gimbal_range_deg = 0.0;
    e.direction     = {0, 0, -1};
    return e;
}

} // namespace openorbit::propulsion
