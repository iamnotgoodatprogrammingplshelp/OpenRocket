#pragma once
#include "core/physics/StateVector.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <memory>

namespace openorbit::parts {

/// Versioned API for part plugin modules
static constexpr int OPENORBIT_API_VERSION = 1;

/**
 * @brief Base interface for all loadable part modules.
 *
 * Parts are dynamically loaded from shared libraries (.so / .dll) and must
 * implement this interface. The versioned OPENORBIT_API_VERSION guard ensures
 * ABI compatibility.
 */
class IPartModule {
public:
    virtual ~IPartModule() = default;

    virtual std::string name() const = 0;
    virtual std::string category() const = 0;

    /// Called every physics step
    virtual void onUpdate(double dt, physics::StateVector& state) = 0;

    /// Serialise/deserialise module state (save/load game)
    virtual nlohmann::json serialize() const = 0;
    virtual void deserialize(const nlohmann::json& j) = 0;

    /// Returns API version this module was compiled against
    virtual int apiVersion() const { return OPENORBIT_API_VERSION; }
};

} // namespace openorbit::parts
