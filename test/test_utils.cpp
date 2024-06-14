#include "chimbuko/core/util/threadPool.hpp"
#include "chimbuko/core/util/RunStats.hpp"
#include "chimbuko/modules/performance_analysis/pserver/AggregateAnomalyData.hpp"
#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <sstream>
#include <unordered_map>

using namespace std::chrono;
using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

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
    const int nrolls = 10000;
    const double mean = 5.0;
    const double std = 2.0;

    std::default_random_engine generator;
    std::normal_distribution<double> dist(mean, std);
    std::vector<double> data;

    RunStats stat(true);
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
    RunStats stat, c_stat;
    std::vector<double> data;
    for (int i = 0; i < 100; i++) {
        double num = i % 10;
        stat.push(num);
        data.push_back(num);
    }

    c_stat.net_deserialize(stat.net_serialize());

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
    AnomalyData d, d2;

    d.set(1, 1000, 2000, 1234567890, 912345678, 123);
    d2.set(1, 1001, 2000, 1234567890, 912345678, 123);

    std::string ser = d.net_serialize();

    AnomalyData c_d;
    c_d.net_deserialize(ser);

    EXPECT_EQ(d, c_d);
    EXPECT_TRUE(d==c_d);
    EXPECT_TRUE(d!=d2);
}

TEST_F(UtilTest, AggregateAnomalyDataMultiThreadsTest)
{
    const std::vector<int> N_RANKS = {20, 30, 50};
    const int MIN_STEPS = 1000;
    const int MAX_STEPS = 2000;
    std::default_random_engine generator;

    std::vector<std::string> anomaly_data;
    std::unordered_map<int, std::unordered_map<unsigned long, AggregateAnomalyData*> > anomaly_stats, c_anomaly_stats;
    std::unordered_map<int, std::unordered_map<unsigned long, int> > n_anomaly_data;
    std::mutex mtx;

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
                anomaly_data.push_back(d.net_serialize());

                if (step == 0)
                {
                    anomaly_stats[app_id][rank_id] = new AggregateAnomalyData();
                    c_anomaly_stats[app_id][rank_id] = new AggregateAnomalyData();
                    n_anomaly_data[app_id][rank_id] = max_steps;
                }

                c_anomaly_stats[app_id][rank_id]->add(d);
            }
        }
    }

    // simulated parameter server
    threadPool tpool(10);
    std::vector<threadPool::TaskFuture<void>> v;
    for (int i = 0; i < (int)anomaly_data.size(); ++i)
    {
      v.push_back(tpool.sumit([&anomaly_data, &anomaly_stats, &mtx, i](){
	      AnomalyData d;
	      std::lock_guard<std::mutex> _(mtx);
	      d.net_deserialize(anomaly_data[i]);
	      anomaly_stats[d.get_app()][d.get_rank()]->add(d);
        }));
    }
    for (auto& item: v)
        item.get();


    // check the result
    for(auto &pp: anomaly_stats) {
      int pid = pp.first;
      for(auto &rp : pp.second){
	unsigned long rid = rp.first;

	const RunStats &stats = rp.second->get_stats();
        RunStats c_stats = c_anomaly_stats[pid][rid]->get_stats();
        
        EXPECT_TRUE(c_stats.equiv(stats));
        EXPECT_EQ(n_anomaly_data[pid][rid], rp.second->get_data()->size());
        delete rp.second;
        delete c_anomaly_stats[pid][rid];
      }
    }
}
