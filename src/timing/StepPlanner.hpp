#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>

namespace toasty::timing {
    constexpr uint32_t MaximumStepsPerUpdate = 100000;
    constexpr uint64_t VanillaTickRate = 240;

    inline uint32_t tickQuantum(int64_t rate) {
        if (rate <= 0) {
            return 1;
        }
        auto target = static_cast<uint64_t>(rate);
        auto quantum = target / std::gcd(target, VanillaTickRate);
        if (quantum == 0 || quantum > MaximumStepsPerUpdate) {
            return 1;
        }
        return static_cast<uint32_t>(quantum);
    }

    struct StepPlan {
        uint32_t steps = 0;
        double delta = 0.0;
    };

    class StepPlanner {
      public:
        StepPlan advance(double delta, double timestep, uint32_t quantum) {
            if (!std::isfinite(delta) || !std::isfinite(timestep) || delta < 0.0 ||
                timestep <= 0.0 || quantum == 0) {
                reset();
                return {};
            }

            m_remainder += delta;
            auto grid = static_cast<double>(quantum);
            auto ceiling = static_cast<double>(
                std::max(quantum, MaximumStepsPerUpdate - MaximumStepsPerUpdate % quantum));
            auto rounded = std::round(m_remainder / (timestep * grid)) * grid;
            auto bounded = std::clamp(rounded, 0.0, ceiling);
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
