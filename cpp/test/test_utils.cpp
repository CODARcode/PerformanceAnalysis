#include "chimbuko/util/threadPool.hpp"
#include "chimbuko/util/RunStats.hpp"
#include "chimbuko/ad/AnomalyStat.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <sstream>
#include <unordered_map>

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

TEST_F(UtilTest, AnomalyStatMultiThreadsTest)
{
    using namespace chimbuko;

    const std::vector<int> N_RANKS = {20, 30, 50};
    const int MIN_STEPS = 1000;
    const int MAX_STEPS = 2000;
    std::default_random_engine generator;

    std::vector<std::string> anomaly_data;
    std::unordered_map<std::string, AnomalyStat*> anomaly_stats, c_anomaly_stats;
    std::unordered_map<std::string, int> n_anomaly_data;

    // generate pseudo data
    for (int app_id = 0; app_id < (int)N_RANKS.size(); app_id++)
    {
        for (int rank_id = 0; rank_id < N_RANKS[app_id]; rank_id++)
        {
            const int max_steps = rand() % (MAX_STEPS - MIN_STEPS + 1) + MIN_STEPS;
            const double mean = (double)rand() / (double)RAND_MAX * 100.0;
            const double stddev = mean * (double)rand() / (double)RAND_MAX;
            // std::cout << "App: " << app_id << ", rank: " << rank_id 
            //     << ", mean: " << mean << ", stddev: " << stddev << std::endl;
            std::normal_distribution<double> dist(mean, stddev);

            for (int step = 0; step < max_steps; step++)
            {
                const int n_anomalies = std::max((int)dist(generator), 0);
                // std::cout << "Step: " << step << ", n: " << n_anomalies << std::endl;
                AnomalyData d(app_id, rank_id, step, 0, 0, n_anomalies);
                anomaly_data.push_back(d.get_binary());

                if (step == 0)
                {
                    anomaly_stats[d.get_stat_id()] = new AnomalyStat();
                    c_anomaly_stats[d.get_stat_id()] = new AnomalyStat();
                    n_anomaly_data[d.get_stat_id()] = max_steps;
                }

                c_anomaly_stats[d.get_stat_id()]->add(d);
            }
        }
    }

    // simulated parameter server
    threadPool tpool(10);
    std::vector<threadPool::TaskFuture<void>> v;
    for (int i = 0; i < (int)anomaly_data.size(); ++i)
    {
        v.push_back(tpool.sumit([&anomaly_data, &anomaly_stats, i](){
            AnomalyData d;
            d.set_binary(anomaly_data[i]);
            anomaly_stats[d.get_stat_id()]->add(anomaly_data[i]);
        }));
    }
    for (auto& item: v)
        item.get();


    // check the result
    for (auto pair: anomaly_stats) {
        auto stats = pair.second->get();
        RunStats c_stats = c_anomaly_stats[pair.first]->get_stats();
        
        EXPECT_EQ(c_stats, stats.first);
        EXPECT_EQ(n_anomaly_data[pair.first], stats.second->size());
        delete pair.second;
        delete stats.second;
        delete c_anomaly_stats[pair.first];
    }
}


