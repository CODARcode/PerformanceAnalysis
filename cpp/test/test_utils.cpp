#include "chimbuko/util/threadPool.hpp"
#include "chimbuko/util/RunStats.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <random>

class UtilTest : public ::testing::Test
{
protected:
    virtual void SetUp() 
    {

    }

    virtual void TearDown()
    {

    }
};

TEST_F(UtilTest, ThreadPoolScaleTest)
{
    using namespace std::chrono;

    high_resolution_clock::time_point t1 = high_resolution_clock::now();

    threadPool tpool(7);
    std::vector<threadPool::TaskFuture<void>> v;
    for (std::uint32_t i = 0u; i < 21u; ++i)
    {
        v.push_back(tpool.sumit([](){
            std::this_thread::sleep_for(seconds(1));
        }));
    }
    for (auto& item: v)
        item.get();
    
    high_resolution_clock::time_point t2 = high_resolution_clock::now();
    duration<double> time_span = duration_cast<duration<double>>(t2 - t1);

    EXPECT_NEAR(3.0, time_span.count(), 0.1);
}

TEST_F(UtilTest, RunStatSimpleTest)
{
    using namespace chimbuko;

    const int nrolls = 10000;
    const double mean = 5.0;
    const double std = 2.0;

    std::default_random_engine generator;
    std::normal_distribution<double> dist(mean, std);
    std::vector<double> data;

    RunStats stat;

    for (int i = 0; i < nrolls; i++)
    {
        double num = dist(generator);
        data.push_back(num);
        stat.push(num);
    }

    double static_mean = chimbuko::static_mean(data);
    double static_std = chimbuko::static_std(data);
    double run_mean = stat.mean();
    double run_std = stat.std();

    EXPECT_NEAR(static_mean, run_mean, 0.001);
    EXPECT_NEAR(static_std, run_std, 0.001);
}

TEST_F(UtilTest, RunStatMergeTest)
{
    using namespace chimbuko;

    const int nrolls = 10000;
    const double mean[2] = {5.0, 10.0};
    const double std[2] = {2.0, 4.0};

    std::default_random_engine generator;
    std::vector<double> data;

    RunStats stat[2];

    for (int j = 0; j < 2; j++) 
    {
        std::normal_distribution<double> dist(mean[j], std[j]);
        for (int i = 0; i < nrolls; i++)
        {
            double num = dist(generator);
            data.push_back(num);
            stat[j].push(num);
        }
    }

    RunStats merged = stat[0] + stat[1];

    double static_mean = chimbuko::static_mean(data);
    double static_std = chimbuko::static_std(data);
    double run_mean = merged.mean();
    double run_std = merged.std();

    EXPECT_NEAR(static_mean, run_mean, 0.001);
    EXPECT_NEAR(static_std, run_std, 0.001);
}