#pragma once
#include "StateVector.hpp"
#include "OrbitalMechanics.hpp"
#include "Integrators.hpp"
#include <unordered_map>
#include <vector>
#include <functional>
#include <string>

namespace openorbit::physics {

/**
 * @brief External force/torque applied to an entity each step.
 *
 * Force models (aerodynamics, propulsion, solar pressure) register callbacks
 * here. The world sums all contributions before integration.
 */
struct ExternalForce {
    Eigen::Vector3d force_N{0, 0, 0};   ///< body-frame force  [N]
    Eigen::Vector3d torque_Nm{0, 0, 0}; ///< body-frame torque [N·m]
    double          mass_flow{0};        ///< propellant consumption rate [kg/s] (negative)
};

/**
 * @brief Per-entity inertia properties (updated each step as fuel depletes).
 */
struct InertiaProperties {
    double          mass_kg   = 1.0;    ///< current total mass [kg]
    Eigen::Matrix3d inertia   = Eigen::Matrix3d::Identity() * 1.0; ///< MOI tensor [kg·m²]
    Eigen::Vector3d com_offset{0, 0, 0}; ///< CoM offset from reference point [m]
};

/**
 * @brief The central simulation world.
 *
 * Owns all entity states, gravitational body positions, and the integration
 * loop. Thread-safe for single-writer (physics thread) / multiple-reader
 * (renderer, AI) access via lock-free state snapshots.
 */
class PhysicsWorld {
public:
    explicit PhysicsWorld(IntegratorType integrator = IntegratorType::RK4);
    ~PhysicsWorld() = default;

    // ── Entity management ────────────────────────────────────────────────────

    /// Add a spacecraft/entity to the simulation. Returns its EntityID.
    EntityID addEntity(const StateVector& initialState,
                       const InertiaProperties& inertia);

    /// Remove an entity (no further updates).
    void removeEntity(EntityID id);

    /// Returns true if the entity exists.
    [[nodiscard]] bool hasEntity(EntityID id) const noexcept;

    // ── Gravitational bodies ─────────────────────────────────────────────────

    /// Register a gravitational body (planet, moon, sun).
    void addGravBody(const GravBody& body);

    /// Update a gravitational body's position (called each step for ephemeris).
    void updateGravBodyPosition(const std::string& name, const Eigen::Vector3d& pos);

    // ── Simulation step ──────────────────────────────────────────────────────

    /**
     * @brief Advance simulation by dt seconds.
     *
     * Steps all entities through the integrator, applying gravity + any
     * registered external forces. For time warp > 1 the step is subdivided
     * into sub-steps to maintain accuracy.
     *
     * @param dt         Nominal time step [s]
     * @param time_warp  Multiplier (1 = real-time, 100 = 100× warp)
     */
    void step(double dt, int time_warp = 1);

    // ── State accessors ──────────────────────────────────────────────────────

    [[nodiscard]] StateVector         getState(EntityID id) const;
    [[nodiscard]] double              getAltitude(EntityID id) const;
    [[nodiscard]] OrbitalElements     getOrbitalElements(EntityID id) const;
    [[nodiscard]] double              getSimTime() const noexcept { return m_simTime; }

    // ── Force application (external modules call these) ──────────────────────

    /// Apply thrust force and torque in the body frame (propulsion module).
    void applyThrust(EntityID id, const Eigen::Vector3d& force_N,
                     const Eigen::Vector3d& torque_Nm, double mass_flow_kgs = 0.0);

    /// Apply aerodynamic force and torque in the body frame.
    void applyAero(EntityID id, const Eigen::Vector3d& force_N,
                   const Eigen::Vector3d& torque_Nm);

    // ── Configuration ─────────────────────────────────────────────────────────

    void setIntegrator(IntegratorType type) noexcept { m_integratorType = type; }
    void setPrimaryBodyName(const std::string& name) { m_primaryBodyName = name; }

    /// Maximum sub-steps per time-warped step (prevents spiral-of-death)
    static constexpr int kMaxSubSteps = 10000;

private:
    struct EntityData {
        StateVector       state;
        InertiaProperties inertia;
        ExternalForce     accumulated;  // zeroed each step
    };

    EntityID nextID() noexcept { return ++m_nextID; }

    StateDerivative computeDerivative(double t, const StateVector& s,
                                      const ExternalForce& forces,
                                      const InertiaProperties& inertia) const;

    std::unordered_map<EntityID, EntityData> m_entities;
    std::vector<GravBody>                    m_gravBodies;
    IntegratorType                           m_integratorType;
    std::string                              m_primaryBodyName = "Earth";
    double                                   m_simTime = 0.0;
    EntityID                                 m_nextID  = 0;

    // RK45 remembers suggested next step per entity
    std::unordered_map<EntityID, double>     m_rk45NextDt;
};

} // namespace openorbit::physics
