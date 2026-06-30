#include "renderer/vab/VehicleAssembly.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace openorbit::vab {

static constexpr double G0 = 9.80665; // m/s²

void VehicleAssembly::addPart(const parts::PartDef& part, bool atBottom) {
    StackPart sp; sp.def = part;
    if (atBottom)
        m_stack.insert(m_stack.begin(), sp);
    else
        m_stack.push_back(sp);
}

void VehicleAssembly::removePart(size_t index) {
    if (index < m_stack.size())
        m_stack.erase(m_stack.begin() + static_cast<std::ptrdiff_t>(index));
}

void VehicleAssembly::moveUp(size_t index) {
    if (index + 1 < m_stack.size())
        std::swap(m_stack[index], m_stack[index + 1]);
}

void VehicleAssembly::moveDown(size_t index) {
    if (index > 0)
        std::swap(m_stack[index], m_stack[index - 1]);
}

void VehicleAssembly::markStageBase(size_t index, bool value) {
    if (index < m_stack.size())
        m_stack[index].isStageBase = value;
}

// ── Delta-V physics ───────────────────────────────────────────────────────────

StageStats VehicleAssembly::computeStage(size_t bottomIdx, size_t topIdx,
                                          double payloadMass_kg) const
{
    StageStats s;

    double wetMass  = payloadMass_kg;
    double dryMass  = payloadMass_kg;
    double thrustVac = 0.0;
    double thrustSL  = 0.0;
    double totalMdotIsp = 0.0;   // for mass-flow-weighted Isp
    double totalMdot    = 0.0;

    for (size_t i = bottomIdx; i <= topIdx && i < m_stack.size(); ++i) {
        const auto& p = m_stack[i].def;
        wetMass  += p.dry_mass_kg + p.fuel_mass_kg + p.oxidizer_kg;
        dryMass  += p.dry_mass_kg;

        // Engine contribution
        if (p.category == parts::PartCategory::Engine) {
            double Tv = p.thrust_vac_N * p.engine_count;
            double Ts = p.thrust_sl_N  * p.engine_count;
            double isp = p.isp_vac_s > 0 ? p.isp_vac_s : p.isp_sl_s;
            thrustVac    += Tv;
            thrustSL     += Ts;
            if (isp > 0.0) {
                double mdot = Tv / (isp * G0);
                totalMdotIsp += mdot * isp;
                totalMdot    += mdot;
            }
        }
    }

    s.totalMass_kg = wetMass;
    s.dryMass_kg   = dryMass;
    s.thrustVac_N  = thrustVac;
    s.thrustSL_N   = thrustSL;
    s.isp_s        = totalMdot > 0.0 ? totalMdotIsp / totalMdot : 0.0;

    if (s.isp_s > 0.0 && wetMass > 0.0 && dryMass > 0.0 && dryMass < wetMass)
        s.deltaV_ms = s.isp_s * G0 * std::log(wetMass / dryMass);

    if (wetMass > 0.0) {
        s.twr_vac = thrustVac / (wetMass * G0);
        s.twr_sl  = thrustSL  / (wetMass * G0);
    }

    if (totalMdot > 0.0)
        s.burnTime_s = (wetMass - dryMass) / totalMdot;

    return s;
}

void VehicleAssembly::recalculate() {
    m_stages.clear();
    m_totalStats = {};

    if (m_stack.empty()) return;

    // Split into stages at stageBase markers
    std::vector<std::pair<size_t,size_t>> stageBounds; // [bottom, top] inclusive
    size_t stageStart = 0;
    for (size_t i = 0; i < m_stack.size(); ++i) {
        if (m_stack[i].isStageBase && i > stageStart) {
            stageBounds.emplace_back(stageStart, i - 1);
            stageStart = i;
        }
    }
    stageBounds.emplace_back(stageStart, m_stack.size() - 1);

    // Compute stages top-down (payload for each stage is everything above it)
    double payload = 0.0;
    m_stages.resize(stageBounds.size());

    for (int s = static_cast<int>(stageBounds.size()) - 1; s >= 0; --s) {
        m_stages[s] = computeStage(stageBounds[s].first, stageBounds[s].second, payload);
        // Next (lower) stage sees this stage's total wet mass as payload
        payload += m_stages[s].totalMass_kg;
    }

    // Total vehicle stats
    double totalDV = 0.0;
    for (const auto& st : m_stages) totalDV += st.deltaV_ms;

    if (!m_stages.empty()) {
        m_totalStats = m_stages.front(); // first stage carries everything
        m_totalStats.deltaV_ms = totalDV;
    }
}

} // namespace openorbit::vab
