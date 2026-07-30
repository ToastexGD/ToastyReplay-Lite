#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace toasty::timing {
    struct StepPlan {
        uint32_t steps = 0;
        double delta = 0.0;
    };

    class StepPlanner {
      public:
        StepPlan advance(double delta, double timestep) {
            if (!std::isfinite(delta) || !std::isfinite(timestep) || delta < 0.0 ||
                timestep <= 0.0) {
                reset();
                return {};
            }

            m_remainder += delta;
            auto rounded = std::round(m_remainder / timestep);
            auto bounded =
                std::clamp(rounded, 0.0, static_cast<double>(std::numeric_limits<uint32_t>::max()));
            auto steps = static_cast<uint32_t>(bounded);
            auto planned = static_cast<double>(steps) * timestep;
            m_remainder -= planned;

            if (!std::isfinite(m_remainder) || std::abs(m_remainder) < timestep * 1e-9) {
                m_remainder = 0.0;
            }

            return {steps, planned};
        }

        void reset() {
            m_remainder = 0.0;
        }

        double remainder() const {
            return m_remainder;
        }

      private:
        double m_remainder = 0.0;
    };
} // namespace toasty::timing
