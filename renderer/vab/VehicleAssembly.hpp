#pragma once
#include "core/parts/PartCatalog.hpp"
#include <vector>
#include <string>

namespace openorbit::vab {

/**
 * @brief One part instance in the rocket stack.
 *
 * Parts stack vertically bottom-to-top. The decoupler flag marks the
 * boundary between stages.
 */
struct StackPart {
    parts::PartDef def;
    bool           isStageBase = false; ///< stage separation below this part
};

/**
 * @brief Stats for a single stage or the full stack.
 */
struct StageStats {
    double totalMass_kg  = 0.0;  ///< at ignition (wet)
    double dryMass_kg    = 0.0;  ///< after burn (no propellant)
    double thrustVac_N   = 0.0;
    double thrustSL_N    = 0.0;
    double isp_s         = 0.0;  ///< mass-flow-weighted average
    double deltaV_ms     = 0.0;  ///< Tsiolkovsky (vacuum)
    double twr_vac       = 0.0;
    double twr_sl        = 0.0;
    double burnTime_s    = 0.0;
};

/**
 * @brief Ordered list of parts forming a rocket, with delta-V analysis.
 *
 * Parts are stored bottom-to-top (index 0 = bottom = first to ignite).
 * Call recalculate() after any modification to refresh all stats.
 */
class VehicleAssembly {
public:
    VehicleAssembly() = default;

    // ── Stack editing ─────────────────────────────────────────────────────────

    void addPart(const parts::PartDef& part, bool atBottom = false);
    void removePart(size_t index);
    void moveUp(size_t index);
    void moveDown(size_t index);
    void markStageBase(size_t index, bool value);
    void clear() { m_stack.clear(); m_stages.clear(); m_totalStats = {}; }

    // ── Queries ───────────────────────────────────────────────────────────────

    [[nodiscard]] const std::vector<StackPart>& stack() const { return m_stack; }
    [[nodiscard]] size_t size() const { return m_stack.size(); }

    [[nodiscard]] const std::vector<StageStats>& stages() const { return m_stages; }
    [[nodiscard]] const StageStats& total() const { return m_totalStats; }

    /**
     * @brief Recompute all stage stats from current stack.
     *
     * Must be called after any stack modification before reading stats.
     */
    void recalculate();

private:
    StageStats computeStage(size_t bottomIdx, size_t topIdx,
                             double payloadMass_kg) const;

    std::vector<StackPart>  m_stack;
    std::vector<StageStats> m_stages;
    StageStats              m_totalStats;
};

} // namespace openorbit::vab
