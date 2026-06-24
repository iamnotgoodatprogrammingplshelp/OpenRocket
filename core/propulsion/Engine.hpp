#pragma once
#include "core/physics/StateVector.hpp"
#include <Eigen/Dense>
#include <string>
#include <vector>
#include <functional>

namespace openorbit::propulsion {

/**
 * @brief Engine type selector.
 */
enum class EngineType {
    LiquidBipropellant,
    SolidRocket,
    Ion,
    HallEffect,
    NuclearThermal,
    RCS,
};

/**
 * @brief Atmosphere-adjusted Isp using the rocket nozzle equation.
 *
 *   Isp(alt) = Isp_vac - (Isp_vac - Isp_sl) * (P_amb / P_sl)
 *
 * @param isp_vac     Vacuum Isp [s]
 * @param isp_sl      Sea-level Isp [s]
 * @param p_ambient   Ambient pressure at current altitude [Pa]
 * @param p_sl        Sea-level reference pressure [Pa] (default 101325 Pa)
 */
[[nodiscard]] double atmosphericIsp(double isp_vac, double isp_sl,
                                     double p_ambient, double p_sl = 101325.0) noexcept;

/**
 * @brief Mass flow rate from thrust and Isp.
 *
 *   ṁ = F / (Isp × g₀)
 */
[[nodiscard]] double massFlowRate(double thrust_N, double isp_s) noexcept;

/**
 * @brief Model of a single engine or thruster.
 */
struct EngineModel {
    std::string  name;
    EngineType   type          = EngineType::LiquidBipropellant;

    // Performance parameters
    double thrust_vac_N        = 0.0;    ///< vacuum thrust [N]
    double thrust_sl_N         = 0.0;    ///< sea-level thrust [N]
    double isp_vac_s           = 300.0;  ///< vacuum Isp [s]
    double isp_sl_s            = 280.0;  ///< sea-level Isp [s]

    // Physical placement (body frame)
    Eigen::Vector3d position{0, 0, 0};   ///< nozzle exit position [m]
    Eigen::Vector3d direction{0, 0, -1}; ///< thrust direction (unit vector, body frame)

    // Gimbal
    double gimbal_range_deg    = 0.0;    ///< maximum gimbal deflection [°]
    Eigen::Vector2d gimbal_cmd{0, 0};    ///< pitch/yaw command [-1, 1]

    // Throttle
    double throttle            = 0.0;    ///< [0, 1]
    double min_throttle        = 0.0;    ///< minimum stable throttle [0, 1]

    // State
    bool   ignited             = false;

    /**
     * @brief Compute thrust force and torque in the body frame.
     *
     * @param ambient_pressure  Atmospheric pressure at current altitude [Pa]
     * @param vehicle_com       Vehicle centre-of-mass in body frame [m]
     * @return                  Force [N] and torque [N·m] in body frame
     */
    struct ThrustOutput {
        Eigen::Vector3d force_N;
        Eigen::Vector3d torque_Nm;
        double          mass_flow_kgs; ///< negative (consumption)
    };

    [[nodiscard]] ThrustOutput compute(double ambient_pressure,
                                        const Eigen::Vector3d& vehicle_com) const noexcept;
};

/**
 * @brief Merlin 1D vacuum (Falcon 9 upper stage) — reference parameters.
 */
[[nodiscard]] EngineModel makeMerlin1DVac();

/**
 * @brief SpaceX Raptor 2 (vacuum variant) — reference parameters.
 */
[[nodiscard]] EngineModel makeRaptor2Vac();

/**
 * @brief Generic solid rocket motor (SRB-style).
 * @param thrust_kN  Average thrust [kN]
 * @param burn_time  Total burn duration [s]
 * @param isp        Specific impulse [s]
 */
[[nodiscard]] EngineModel makeSolidRocket(double thrust_kN, double burn_time, double isp = 270.0);

} // namespace openorbit::propulsion
