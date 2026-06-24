#include "core/aero/Aerodynamics.hpp"
#include <cmath>
#include <algorithm>
#include <numbers>

namespace openorbit::aero {

// ─────────────────────────────────────────────────────────────────────────────
// AeroTable bilinear lookup
// ─────────────────────────────────────────────────────────────────────────────

static std::pair<int,double> findInterval(const std::vector<double>& xs, double x) {
    if (xs.empty()) return {0, 0.0};
    if (x <= xs.front()) return {0, 0.0};
    if (x >= xs.back())  return {static_cast<int>(xs.size())-2, 1.0};

    auto it = std::lower_bound(xs.begin(), xs.end(), x);
    int i = static_cast<int>(it - xs.begin()) - 1;
    double t = (x - xs[i]) / (xs[i+1] - xs[i]);
    return {i, t};
}

double AeroTable::bilinear(const std::vector<std::vector<double>>& table,
                            double mach, double alpha) const noexcept
{
    if (table.empty()) return 0.0;
    auto [mi, tm] = findInterval(mach_points,  mach);
    auto [ai, ta] = findInterval(alpha_points, alpha);

    int mi1 = std::min(mi+1, static_cast<int>(mach_points.size())-1);
    int ai1 = std::min(ai+1, static_cast<int>(alpha_points.size())-1);

    double v00 = table[mi ][ai ];
    double v10 = table[mi1][ai ];
    double v01 = table[mi ][ai1];
    double v11 = table[mi1][ai1];

    return v00*(1-tm)*(1-ta) + v10*tm*(1-ta) + v01*(1-tm)*ta + v11*tm*ta;
}

double AeroTable::lookupCd(double mach, double alpha) const noexcept {
    return bilinear(Cd, mach, alpha);
}
double AeroTable::lookupCl(double mach, double alpha) const noexcept {
    return bilinear(Cl, mach, alpha);
}
double AeroTable::lookupCm(double mach, double alpha) const noexcept {
    return bilinear(Cm, mach, alpha);
}

// ─────────────────────────────────────────────────────────────────────────────
// Flow conditions
// ─────────────────────────────────────────────────────────────────────────────

FlowConditions computeFlowConditions(
        const physics::StateVector&   state,
        const world::AtmosphereState& atm) noexcept
{
    FlowConditions fc{};

    // Relative velocity (ignoring Earth rotation for now)
    Eigen::Vector3d v_inertial = state.velocity;
    Eigen::Matrix3d R_inertial_to_body = state.attitude.toRotationMatrix().transpose();
    fc.v_wind_body = R_inertial_to_body * v_inertial;
    fc.v_body_ms   = fc.v_wind_body.norm();

    if (fc.v_body_ms < 0.01) {
        fc.mach = fc.alpha_rad = fc.beta_rad = fc.q_dyn_Pa = 0.0;
        return fc;
    }

    fc.mach      = (atm.speed_of_sound_ms > 0) ? fc.v_body_ms / atm.speed_of_sound_ms : 0.0;
    fc.q_dyn_Pa  = 0.5 * atm.density_kgm3 * fc.v_body_ms * fc.v_body_ms;

    // Angle of attack α: angle between velocity and body x-axis (nose direction)
    // Convention: body frame x-axis = nose. Wind from below → positive AoA.
    Eigen::Vector3d v_hat = fc.v_wind_body.normalized();
    fc.alpha_rad = std::atan2(-v_hat.z(), v_hat.x());  // pitch AoA
    fc.beta_rad  = std::asin(std::clamp(v_hat.y(), -1.0, 1.0));  // sideslip

    return fc;
}

// ─────────────────────────────────────────────────────────────────────────────
// Aerodynamic force computation
// ─────────────────────────────────────────────────────────────────────────────

AeroOutput computeAero(
        const physics::StateVector&   state,
        const VehicleAero&            aero,
        const world::AtmosphereState& atm) noexcept
{
    AeroOutput out{};

    FlowConditions fc = computeFlowConditions(state, atm);
    if (fc.v_body_ms < 0.01 || atm.density_kgm3 < 1e-12) return out;

    double Cd = aero.table.lookupCd(fc.mach, std::abs(fc.alpha_rad));
    double Cl = aero.table.lookupCl(fc.mach, std::abs(fc.alpha_rad));
    double Cm = aero.table.lookupCm(fc.mach, fc.alpha_rad);

    double q  = fc.q_dyn_Pa;
    double S  = aero.reference_area_m2;
    double L  = aero.reference_length_m;

    // Drag force: opposite to velocity direction in body frame
    Eigen::Vector3d v_hat = fc.v_wind_body.normalized();
    double drag_mag = q * S * Cd;
    out.force_N = -drag_mag * v_hat;

    // Lift force: perpendicular to velocity, in the plane of α
    if (std::abs(fc.alpha_rad) > 1e-4) {
        // Lift direction: perpendicular to v_hat in the pitch plane (x-z)
        Eigen::Vector3d lift_dir{-v_hat.z(), 0.0, v_hat.x()};
        if (lift_dir.norm() > 1e-6) {
            lift_dir.normalize();
            double lift_sign = (fc.alpha_rad > 0) ? 1.0 : -1.0;
            out.force_N += lift_sign * q * S * Cl * lift_dir;
        }
    }

    // Pitching moment
    double moment_mag = q * S * L * Cm;
    // About body Y-axis (pitch)
    out.torque_Nm = Eigen::Vector3d{0, moment_mag, 0};
    // Add moment arm from CoP offset
    out.torque_Nm += aero.cp_offset.cross(out.force_N);

    // Re-entry heat flux: q_heat = 1.7415e-4 * sqrt(ρ/r_nose) * v³
    // Simplified: q = 0.5 * ρ * v³ * Cd_heat (Cd_heat ≈ 1.0 for blunt bodies)
    out.heat_flux_Wm2 = 0.5 * atm.density_kgm3
                      * fc.v_body_ms * fc.v_body_ms * fc.v_body_ms
                      * 0.1; // Cd_heat = 0.1 (sharp nose)

    return out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Default aerotables
// ─────────────────────────────────────────────────────────────────────────────

AeroTable makeCapsuleAeroTable() {
    AeroTable t;
    t.mach_points  = {0.0, 0.5, 0.8, 1.0, 1.2, 1.5, 2.0, 3.0, 5.0, 8.0, 15.0, 25.0};
    t.alpha_points = {0.0, 5.0*std::numbers::pi/180.0,
                      10.0*std::numbers::pi/180.0,
                      20.0*std::numbers::pi/180.0,
                      45.0*std::numbers::pi/180.0};

    // Cd values (blunt body — high drag)
    size_t nm = t.mach_points.size();
    size_t na = t.alpha_points.size();
    t.Cd.assign(nm, std::vector<double>(na, 0.0));
    t.Cl.assign(nm, std::vector<double>(na, 0.0));
    t.Cm.assign(nm, std::vector<double>(na, 0.0));

    // Approximate Cd for a blunt capsule (1.5 at subsonic, peak ~1.6 at Mach 1, reduces at hypersonic)
    std::vector<double> cd_mach = {1.50, 1.52, 1.55, 1.60, 1.58, 1.52, 1.45, 1.35, 1.25, 1.20, 1.15, 1.10};
    for (size_t mi = 0; mi < nm; ++mi)
        for (size_t ai = 0; ai < na; ++ai)
            t.Cd[mi][ai] = cd_mach[mi] * (1.0 + 0.1 * ai / static_cast<double>(na));

    return t;
}

AeroTable makeSlenderRocketAeroTable() {
    AeroTable t;
    t.mach_points  = {0.0, 0.3, 0.6, 0.8, 0.9, 1.0, 1.1, 1.2, 1.5, 2.0, 3.0, 5.0, 10.0};
    t.alpha_points = {0.0, 2.0*std::numbers::pi/180.0, 5.0*std::numbers::pi/180.0,
                      10.0*std::numbers::pi/180.0, 20.0*std::numbers::pi/180.0};

    size_t nm = t.mach_points.size();
    size_t na = t.alpha_points.size();
    t.Cd.assign(nm, std::vector<double>(na, 0.0));
    t.Cl.assign(nm, std::vector<double>(na, 0.0));
    t.Cm.assign(nm, std::vector<double>(na, 0.0));

    // Subsonic base Cd ~0.3, transonic drag rise to ~0.6, supersonic reduction
    std::vector<double> cd_mach = {0.30, 0.30, 0.32, 0.38, 0.50, 0.60, 0.55, 0.50, 0.42, 0.36, 0.30, 0.24, 0.18};
    // Cl increases with AoA
    for (size_t mi = 0; mi < nm; ++mi)
        for (size_t ai = 0; ai < na; ++ai) {
            t.Cd[mi][ai] = cd_mach[mi] + 0.05 * ai / static_cast<double>(na);
            t.Cl[mi][ai] = 2.0 * std::sin(t.alpha_points[ai]) * std::cos(t.alpha_points[ai]);
            t.Cm[mi][ai] = -0.1 * t.alpha_points[ai]; // restoring moment
        }

    return t;
}

} // namespace openorbit::aero
