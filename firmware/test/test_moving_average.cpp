#include "test_framework.h"
#include "../src/core/MovingAverageFilter.h"

TEST_CASE(MovingAverage, InitialState) {
    VoltWatch::MovingAverageFilter<4> filter;
    ASSERT_EQ(static_cast<size_t>(0), filter.getSampleCount());
    ASSERT_FLOAT_NEAR(0.0f, filter.getAverage(), 0.0001f);
    ASSERT_FALSE(filter.isFull());
    ASSERT_EQ(static_cast<size_t>(4), filter.getCapacity());
}

TEST_CASE(MovingAverage, AccumulatePartial) {
    VoltWatch::MovingAverageFilter<4> filter;
    filter.addSample(10.0f);
    ASSERT_EQ(static_cast<size_t>(1), filter.getSampleCount());
    ASSERT_FLOAT_NEAR(10.0f, filter.getAverage(), 0.001f);

    filter.addSample(20.0f);
    ASSERT_EQ(static_cast<size_t>(2), filter.getSampleCount());
    ASSERT_FLOAT_NEAR(15.0f, filter.getAverage(), 0.001f);
    ASSERT_FALSE(filter.isFull());
}

TEST_CASE(MovingAverage, FillAndSlideWindow) {
    VoltWatch::MovingAverageFilter<4> filter;
    filter.addSample(10.0f);
    filter.addSample(10.0f);
    filter.addSample(10.0f);
    filter.addSample(10.0f);

    ASSERT_TRUE(filter.isFull());
    ASSERT_FLOAT_NEAR(10.0f, filter.getAverage(), 0.001f);

    // Overwrite the first 10.0f with 20.0f -> (10 + 10 + 10 + 20) / 4 = 12.5f
    float avg = filter.addSample(20.0f);
    ASSERT_FLOAT_NEAR(12.5f, avg, 0.001f);
    ASSERT_FLOAT_NEAR(12.5f, filter.getAverage(), 0.001f);
}

TEST_CASE(MovingAverage, ResetClearsBuffer) {
    VoltWatch::MovingAverageFilter<4> filter;
    filter.addSample(12.0f);
    filter.addSample(14.0f);
    filter.reset();

    ASSERT_EQ(static_cast<size_t>(0), filter.getSampleCount());
    ASSERT_FLOAT_NEAR(0.0f, filter.getAverage(), 0.0001f);
    ASSERT_FALSE(filter.isFull());
}
