#include "chimbuko/util/threadPool.hpp"
#include "chimbuko/util/RunStats.hpp"
#include "chimbuko/ad/AnomalyStat.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <sstream>

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

    RunStats stat(true, false);
    double sum = 0.0;
    for (int i = 0; i < nrolls; i++)
    {
        double num = dist(generator);
        sum += num;
        data.push_back(num);
        stat.push(num);
    }

    double static_mean = chimbuko::static_mean(data);
    double static_std = chimbuko::static_std(data);
    double run_mean = stat.mean();
    double run_std = stat.stddev();
    double run_sum = stat.accumulate();

    EXPECT_NEAR(static_mean, run_mean, 0.001);
    EXPECT_NEAR(static_std, run_std, 0.001);
    EXPECT_NEAR(sum, run_sum, 0.001);
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
    double run_std = merged.stddev();

    EXPECT_NEAR(static_mean, run_mean, 0.001);
    EXPECT_NEAR(static_std, run_std, 0.001);
}

TEST_F(UtilTest, RunStatSerializeTest)
{
    using namespace chimbuko;

    RunStats stat, c_stat;
    std::vector<double> data;
    for (int i = 0; i < 100; i++) {
        double num = i % 10;
        stat.push(num);
        data.push_back(num);
    }

    std::stringstream ss(std::stringstream::in | std::stringstream::out | std::stringstream::binary);
    stat.set_stream(true);
    ss << stat;
    stat.set_stream(false);

    c_stat.set_stream(true);
    ss >> c_stat;
    c_stat.set_stream(false);

    double static_mean = chimbuko::static_mean(data, 0.0);
    double static_std = chimbuko::static_std(data);
    double run_mean = stat.mean();
    double run_std = stat.stddev();  
    EXPECT_NEAR(run_mean, static_mean, 0.001);
    EXPECT_NEAR(run_std, static_std, 0.001);
    EXPECT_EQ(stat.count(), c_stat.count());
    EXPECT_DOUBLE_EQ(run_mean, c_stat.mean());
    EXPECT_DOUBLE_EQ(run_std, c_stat.stddev());
    EXPECT_EQ(stat, c_stat);
}

TEST_F(UtilTest, AnomalyDataSerializeTest)
{
    using namespace chimbuko;

    AnomalyData d, c_d, d2;

    d.set(1, 1000, 2000, 1234567890, 912345678, 123);
    d2.set(1, 1001, 2000, 1234567890, 912345678, 123);

    c_d.set_binary(d.get_binary());

    EXPECT_EQ(d, c_d);
    EXPECT_TRUE(d==c_d);
    EXPECT_TRUE(d!=d2);
}
