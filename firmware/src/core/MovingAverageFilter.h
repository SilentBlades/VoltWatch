#ifndef VOLTWATCH_MOVING_AVERAGE_FILTER_H
#define VOLTWATCH_MOVING_AVERAGE_FILTER_H

#include <stddef.h>
#include <stdint.h>

namespace VoltWatch {

/**
 * @brief Fixed-size circular buffer moving average filter.
 * 
 * Provides O(1) time-complexity filtering without heap allocations.
 * Computes mean over valid accumulated samples during initialization.
 * 
 * @tparam Capacity Maximum number of samples in the moving window.
 */
template <size_t Capacity>
class MovingAverageFilter {
    static_assert(Capacity > 0, "Capacity must be greater than zero");

public:
    MovingAverageFilter() {
        reset();
    }

    /**
     * @brief Reset filter buffer and accumulated metrics.
     */
    void reset() {
        for (size_t i = 0; i < Capacity; ++i) {
            m_buffer[i] = 0.0f;
        }
        m_head = 0;
        m_count = 0;
        m_sum = 0.0f;
    }

    /**
     * @brief Add a new sample to the window and update moving average.
     * @param sample The new numerical sample.
     * @return Current moving average after insertion.
     */
    float addSample(float sample) {
        if (m_count < Capacity) {
            m_buffer[m_head] = sample;
            m_sum += sample;
            m_count++;
            m_head = (m_head + 1) % Capacity;
        } else {
            // Subtract the sample being overwritten
            m_sum -= m_buffer[m_head];
            m_buffer[m_head] = sample;
            m_sum += sample;
            m_head = (m_head + 1) % Capacity;
        }
        return getAverage();
    }

    /**
     * @brief Get current moving average value.
     */
    float getAverage() const {
        if (m_count == 0) {
            return 0.0f;
        }
        return m_sum / static_cast<float>(m_count);
    }

    /**
     * @brief Check whether the moving window is completely filled.
     */
    bool isFull() const {
        return m_count == Capacity;
    }

    /**
     * @brief Return number of samples currently recorded.
     */
    size_t getSampleCount() const {
        return m_count;
    }

    /**
     * @brief Return maximum capacity of the filter.
     */
    static constexpr size_t getCapacity() {
        return Capacity;
    }

private:
    float m_buffer[Capacity];
    size_t m_head{0};
    size_t m_count{0};
    float m_sum{0.0f};
};

} // namespace VoltWatch

#endif // VOLTWATCH_MOVING_AVERAGE_FILTER_H
