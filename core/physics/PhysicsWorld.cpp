#include "core/physics/PhysicsWorld.hpp"
#include <stdexcept>
#include <format>
#include <algorithm>

namespace openorbit::physics {

PhysicsWorld::PhysicsWorld(IntegratorType integrator)
    : m_integratorType(integrator)
{}

// ─────────────────────────────────────────────────────────────────────────────
// Entity management
// ─────────────────────────────────────────────────────────────────────────────

EntityID PhysicsWorld::addEntity(const StateVector& initialState,
                                  const InertiaProperties& inertia)
{
    EntityID id = nextID();
    m_entities[id] = EntityData{initialState, inertia, {}};
    m_rk45NextDt[id] = 1.0; // default trial step
    return id;
}

void PhysicsWorld::removeEntity(EntityID id) {
    m_entities.erase(id);
    m_rk45NextDt.erase(id);
}

bool PhysicsWorld::hasEntity(EntityID id) const noexcept {
    return m_entities.contains(id);
}

// ─────────────────────────────────────────────────────────────────────────────
// Gravitational bodies
// ─────────────────────────────────────────────────────────────────────────────

void PhysicsWorld::addGravBody(const GravBody& body) {
    m_gravBodies.push_back(body);
}

void PhysicsWorld::updateGravBodyPosition(const std::string& name,
                                           const Eigen::Vector3d& pos)
{
    for (auto& b : m_gravBodies) {
        if (b.name == name) { b.position = pos; return; }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Derivative computation
// ─────────────────────────────────────────────────────────────────────────────

StateDerivative PhysicsWorld::computeDerivative(
        double t,
        const StateVector& s,
        const ExternalForce& forces,
        const InertiaProperties& inertia) const
{
    StateDerivative d;

    // Translational
    d.dposition = s.velocity;
    d.dvelocity  = computeGravity(s.position, std::span<const GravBody>(m_gravBodies));

    // External force (thrust + aero) converted to inertial frame
    Eigen::Matrix3d R = s.attitude.toRotationMatrix(); // body→inertial
    d.dvelocity += R * forces.force_N / s.mass_kg;

    // Rotational  α = I⁻¹ (τ - ω × I·ω)
    Eigen::Vector3d Iomega = inertia.inertia * s.omega;
    Eigen::Vector3d gyro   = s.omega.cross(Iomega);
    d.domega = inertia.inertia.inverse() * (forces.torque_Nm - gyro);

    // Quaternion kinematics
    d.dattitude = quaternionRate(s.attitude, s.omega);

    // Mass depletion
    d.dmass = forces.mass_flow; // kg/s (negative = consumption)

    return d;
}

// ─────────────────────────────────────────────────────────────────────────────
// Simulation step
// ─────────────────────────────────────────────────────────────────────────────

void PhysicsWorld::step(double dt, int time_warp) {
    // Sub-divide warped steps to maintain integrator accuracy.
    // For RK4 at 1 s nominal step, warp of 100 → 100 sub-steps of 1 s each.
    // For very high warps (1000x) we allow larger sub-steps but cap at 10 s.
    const double warp_dt = std::min(dt * time_warp, 10.0);
    const int    n_steps = std::max(1, static_cast<int>(std::ceil(dt * time_warp / warp_dt)));
    const double sub_dt  = dt * time_warp / static_cast<double>(n_steps);

    for (int sub = 0; sub < n_steps; ++sub) {
        for (auto& [id, data] : m_entities) {
            // Capture accumulated external forces (zeroed after use)
            ExternalForce forces = data.accumulated;
            data.accumulated     = {};

            // Build derivative function that uses the accumulated forces
            auto derivFn = [this, &forces, &data](double t, const StateVector& s) {
                return computeDerivative(t, s, forces, data.inertia);
            };

            switch (m_integratorType) {
                case IntegratorType::RK4:
                    data.state = stepRK4(derivFn, data.state, m_simTime + sub * sub_dt, sub_dt);
                    break;

                case IntegratorType::RK45: {
                    double trial = m_rk45NextDt.at(id);
                    auto result  = stepRK45(derivFn, data.state,
                                            m_simTime + sub * sub_dt, trial);
                    data.state          = result.state;
                    m_rk45NextDt[id]    = result.dt_next;
                    break;
                }

                case IntegratorType::Verlet:
                    data.state = stepVerlet(derivFn, data.state, m_simTime + sub * sub_dt, sub_dt);
                    break;
            }

            // Clamp fuel to zero
            if (data.state.fuel_kg < 0.0) data.state.fuel_kg = 0.0;
            if (data.state.mass_kg < data.inertia.mass_kg - data.state.fuel_kg)
                data.state.mass_kg = data.inertia.mass_kg;
        }
        m_simTime += sub_dt;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// State accessors
// ─────────────────────────────────────────────────────────────────────────────

StateVector PhysicsWorld::getState(EntityID id) const {
    auto it = m_entities.find(id);
    if (it == m_entities.end())
        throw std::out_of_range(std::format("PhysicsWorld::getState: unknown entity {}", id));
    return it->second.state;
}

double PhysicsWorld::getAltitude(EntityID id) const {
    const auto& s = getState(id);
    for (const auto& b : m_gravBodies) {
        if (b.name == m_primaryBodyName)
            return s.position.norm() - b.radius_m;
    }
    return s.position.norm();
}

OrbitalElements PhysicsWorld::getOrbitalElements(EntityID id) const {
    const auto& s = getState(id);
    double mu = 3.986004418e14; // Earth GM, fallback
    for (const auto& b : m_gravBodies) {
        if (b.name == m_primaryBodyName) { mu = b.mu; break; }
    }
    return stateToElements(s.position, s.velocity, mu);
}

// ─────────────────────────────────────────────────────────────────────────────
// Force application
// ─────────────────────────────────────────────────────────────────────────────

void PhysicsWorld::applyThrust(EntityID id,
                                const Eigen::Vector3d& force_N,
                                const Eigen::Vector3d& torque_Nm,
                                double mass_flow_kgs)
{
    auto it = m_entities.find(id);
    if (it == m_entities.end()) return;
    it->second.accumulated.force_N   += force_N;
    it->second.accumulated.torque_Nm += torque_Nm;
    it->second.accumulated.mass_flow += mass_flow_kgs;
}

void PhysicsWorld::applyAero(EntityID id,
                              const Eigen::Vector3d& force_N,
                              const Eigen::Vector3d& torque_Nm)
{
    auto it = m_entities.find(id);
    if (it == m_entities.end()) return;
    it->second.accumulated.force_N   += force_N;
    it->second.accumulated.torque_Nm += torque_Nm;
}

} // namespace openorbit::physics
