#include "core/physics/OrbitalMechanics.hpp"
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace openorbit::physics {

// ─────────────────────────────────────────────────────────────────────────────
// N-body gravity
// ─────────────────────────────────────────────────────────────────────────────

Eigen::Vector3d computeGravity(
        const Eigen::Vector3d&    position,
        std::span<const GravBody> bodies,
        double                    softening)
{
    Eigen::Vector3d accel = Eigen::Vector3d::Zero();
    const double eps2 = softening * softening;

    for (const auto& body : bodies) {
        Eigen::Vector3d r = body.position - position;
        double dist2 = r.squaredNorm() + eps2;
        double dist  = std::sqrt(dist2);
        // a_i = GM * r̂ / |r|²  →  GM * r / |r|³
        accel += (body.mu / (dist2 * dist)) * r;
    }
    return accel;
}

// ─────────────────────────────────────────────────────────────────────────────
// Orbital element conversions
// ─────────────────────────────────────────────────────────────────────────────

OrbitalElements stateToElements(
        const Eigen::Vector3d& r,
        const Eigen::Vector3d& v,
        double                 mu)
{
    const double r_mag = r.norm();
    const double v_mag = v.norm();

    // Specific angular momentum vector  h = r × v
    Eigen::Vector3d h = r.cross(v);
    double h_mag = h.norm();

    // Node vector  N = ẑ × h
    Eigen::Vector3d z_hat{0, 0, 1};
    Eigen::Vector3d N = z_hat.cross(h);
    double N_mag = N.norm();

    // Eccentricity vector  e_vec = (v×h)/mu - r̂
    Eigen::Vector3d e_vec = (v.cross(h) / mu) - (r / r_mag);
    double e = e_vec.norm();

    // Specific orbital energy
    double energy = 0.5 * v_mag * v_mag - mu / r_mag;

    // Semi-major axis (negative for hyperbola)
    double a = (std::abs(energy) > 1e-10) ? -mu / (2.0 * energy) : 1e30;

    // Inclination  i = arccos(h_z / |h|)
    double i = std::acos(std::clamp(h.z() / h_mag, -1.0, 1.0));

    // RAAN  Ω = arccos(N_x / |N|)
    double raan = 0.0;
    if (N_mag > 1e-10) {
        raan = std::acos(std::clamp(N.x() / N_mag, -1.0, 1.0));
        if (N.y() < 0.0) raan = 2.0 * std::numbers::pi - raan;
    }

    // Argument of periapsis  ω = arccos(N·e_vec / (|N||e|))
    double aop = 0.0;
    if (N_mag > 1e-10 && e > 1e-10) {
        aop = std::acos(std::clamp(N.dot(e_vec) / (N_mag * e), -1.0, 1.0));
        if (e_vec.z() < 0.0) aop = 2.0 * std::numbers::pi - aop;
    }

    // True anomaly  ν = arccos(e_vec·r / (|e||r|))
    double ta = 0.0;
    if (e > 1e-10) {
        ta = std::acos(std::clamp(e_vec.dot(r) / (e * r_mag), -1.0, 1.0));
        if (r.dot(v) < 0.0) ta = 2.0 * std::numbers::pi - ta;
    } else {
        // Circular orbit: use argument of latitude
        ta = std::acos(std::clamp(N.dot(r) / (N_mag * r_mag), -1.0, 1.0));
        if (r.z() < 0.0) ta = 2.0 * std::numbers::pi - ta;
    }

    return OrbitalElements{a, e, i, raan, aop, ta};
}

void elementsToState(
        const OrbitalElements& el,
        double                 mu,
        Eigen::Vector3d&       position,
        Eigen::Vector3d&       velocity)
{
    const double e = el.e;
    const double a = el.a;
    const double nu = el.ta;

    // Semi-latus rectum
    double p = a * (1.0 - e * e);

    // Position in perifocal frame
    double r_mag = p / (1.0 + e * std::cos(nu));
    Eigen::Vector3d r_peri{r_mag * std::cos(nu), r_mag * std::sin(nu), 0.0};

    // Velocity in perifocal frame
    double v_scale = std::sqrt(mu / p);
    Eigen::Vector3d v_peri{
        -v_scale * std::sin(nu),
         v_scale * (e + std::cos(nu)),
         0.0
    };

    // Rotation matrices  Rz(-Ω) · Rx(-i) · Rz(-ω)
    using M3 = Eigen::Matrix3d;
    auto Rz = [](double ang) -> M3 {
        return (M3() << std::cos(ang), -std::sin(ang), 0,
                        std::sin(ang),  std::cos(ang), 0,
                        0,              0,             1).finished();
    };
    auto Rx = [](double ang) -> M3 {
        return (M3() << 1, 0,            0,
                        0, std::cos(ang), -std::sin(ang),
                        0, std::sin(ang),  std::cos(ang)).finished();
    };

    M3 Q = Rz(-el.raan) * Rx(-el.i) * Rz(-el.aop);
    position = Q * r_peri;
    velocity = Q * v_peri;
}

// ─────────────────────────────────────────────────────────────────────────────
// Utility functions
// ─────────────────────────────────────────────────────────────────────────────

double orbitalPeriod(double a, double mu) noexcept {
    return 2.0 * std::numbers::pi * std::sqrt(a * a * a / mu);
}

double circularOrbitSpeed(double r, double mu) noexcept {
    return std::sqrt(mu / r);
}

double solveKepler(double M, double e, double tol) {
    // Starting guess — Mikkola's method for better convergence at high e
    double E = M + e * std::sin(M) * (1.0 + e * std::cos(M));
    for (int iter = 0; iter < 100; ++iter) {
        double dE = (M - E + e * std::sin(E)) / (1.0 - e * std::cos(E));
        E += dE;
        if (std::abs(dE) < tol) return E;
    }
    return E; // Should not reach here for e < 1
}

double trueFromEccentric(double E, double e) noexcept {
    // tan(ν/2) = sqrt((1+e)/(1-e)) * tan(E/2)
    double half_ta = std::atan2(
        std::sqrt(1.0 + e) * std::sin(E / 2.0),
        std::sqrt(1.0 - e) * std::cos(E / 2.0)
    );
    return 2.0 * half_ta;
}

// ─────────────────────────────────────────────────────────────────────────────
// Quaternion kinematics
// ─────────────────────────────────────────────────────────────────────────────

Eigen::Quaterniond quaternionRate(
        const Eigen::Quaterniond& q,
        const Eigen::Vector3d&    omega) noexcept
{
    // Represent ω as a pure quaternion [0, ωx, ωy, ωz]
    Eigen::Quaterniond omega_quat{0.0, omega.x(), omega.y(), omega.z()};
    // dq/dt = 0.5 * q ⊗ omega_quat
    Eigen::Quaterniond dq = Eigen::Quaterniond{
        (q * omega_quat).coeffs() * 0.5
    };
    return dq;
}

} // namespace openorbit::physics
