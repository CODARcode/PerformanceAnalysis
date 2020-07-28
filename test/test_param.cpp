#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/pserver/global_anomaly_stats.hpp"
#include "chimbuko/message.hpp"
#include <gtest/gtest.h>
#include <random>
#include <nlohmann/json.hpp>
#include <unordered_map>

class ParamTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {

    }

    virtual void TearDown()
    {

    }
};

TEST_F(ParamTest, SstdUpdateTest)
{
    using namespace chimbuko;

    const int n_functions = 1000;
    const int n_rolls = 10000;

    SstdParam g_param;
    SstdParam l_param_a;
    SstdParam l_param_b;

    for (int i = 0; i < n_functions; i++) {
        unsigned long id = (unsigned long)i;
        for (int j = 0; j < n_rolls; j++) {
            double num = j % 10;
            l_param_a[id].push(num);
        }
    }

    g_param.update(l_param_a.serialize());
    for (int i = 0; i < n_functions; i++) {
        unsigned long id = (unsigned long)i;
        EXPECT_DOUBLE_EQ(l_param_a[id].mean(), g_param[id].mean());
        EXPECT_DOUBLE_EQ(l_param_a[id].stddev(), g_param[id].stddev());
        for (int j = 0; j < n_rolls; j++) {
            l_param_b[id].push(j % 20);
        }
    }
    EXPECT_STREQ(g_param.serialize().c_str(), l_param_a.serialize().c_str());

    g_param.update(l_param_b.serialize());
    l_param_a.update(l_param_b);
    for (int i = 0; i < n_functions; i++) {
        unsigned long id = (unsigned long)i;
        EXPECT_DOUBLE_EQ(l_param_a[id].mean(), g_param[id].mean());
        EXPECT_DOUBLE_EQ(l_param_a[id].stddev(), g_param[id].stddev());
    }

    EXPECT_STREQ(g_param.serialize().c_str(), l_param_a.serialize().c_str());
}

TEST_F(ParamTest, MessageTest)
{
    using namespace chimbuko;

    Message msg, c_msg;
    Message::Header c_head;
    std::string dummy{"Hello World!"}, dummy2;

    msg.set_info(0, 1, MessageType::REQ_ADD, MessageKind::PARAMETERS, 10);
    msg.set_msg(dummy, false);

    EXPECT_EQ(0, msg.src());
    EXPECT_EQ(1, msg.dst());
    EXPECT_EQ(MessageType::REQ_ADD, msg.type());
    EXPECT_EQ(MessageKind::PARAMETERS, msg.kind());
    EXPECT_EQ(10, msg.frame());
    EXPECT_EQ((int)dummy.size(), msg.size());
 
    c_msg = msg.createReply();

    EXPECT_EQ(1, c_msg.src());
    EXPECT_EQ(0, c_msg.dst());
    EXPECT_EQ(MessageType::REP_ADD, c_msg.type());
    EXPECT_EQ(MessageKind::PARAMETERS, c_msg.kind());
    EXPECT_EQ(10, c_msg.frame());

    dummy2 = msg.buf();
    EXPECT_STREQ(dummy.c_str(), dummy2.c_str());
    EXPECT_STREQ(msg.buf().c_str(), dummy2.c_str());
}

TEST_F(ParamTest, SstdMessageTest)
{
    using namespace chimbuko;

    const int n_functions = 4;
    const double mean[n_functions] = {1.0, 10.0, 100.0, 1000.0};
    const double std[n_functions] = {0.1, 1.0, 10.0, 100.0};
    const int n_rolls = 1000;
    const int n_frames = 1;
    SstdParam g_param, l_param, c_param;
    Message msg, c_msg;
    std::default_random_engine generator;
    std::string dummy;

    for (int iFrame = 0; iFrame < n_frames; iFrame++) {
        l_param.clear();
        for (int i = 0; i < n_functions; i++)
        {
            std::normal_distribution<double> dist(mean[i], std[i]);
            unsigned long funcid = (unsigned long)i;
            for (int j = 0; j < n_rolls; j++) 
            {
                double num = dist(generator);
                l_param[funcid].push(num);
            }
        }
        // set message
        msg.set_info(1, 2, 
            (int)chimbuko::MessageType::REQ_ADD, 
            (int)chimbuko::MessageKind::PARAMETERS, iFrame);
        msg.set_msg(l_param.serialize(), false);
        
        EXPECT_EQ(1, msg.src());
        EXPECT_EQ(2, msg.dst());
        EXPECT_EQ(chimbuko::MessageType::REQ_ADD, msg.type());
        EXPECT_EQ(chimbuko::MessageKind::PARAMETERS, msg.kind());
        EXPECT_EQ((int)l_param.serialize().size(), msg.size());
        EXPECT_EQ(iFrame, msg.frame());

        dummy = msg.data();
        c_msg.set_msg(dummy, true);

        EXPECT_EQ(msg.src(), c_msg.src());
        EXPECT_EQ(msg.dst(), c_msg.dst());
        EXPECT_EQ(msg.type(), c_msg.type());
        EXPECT_EQ(msg.kind(), c_msg.kind());
        EXPECT_EQ(msg.size(), c_msg.size());
        EXPECT_EQ(msg.frame(), c_msg.frame());
        EXPECT_STREQ(msg.data().c_str(), c_msg.data().c_str());
        EXPECT_STREQ(msg.buf().c_str(), c_msg.buf().c_str());

        c_param.update(l_param);
        dummy = g_param.update(msg.buf(), true);
        EXPECT_STREQ(c_param.serialize().c_str(), dummy.c_str());
    }
}

TEST(GlobalAnomalyStatsTest, AnomalyStatTest1)
{
    using namespace chimbuko;

    const std::vector<int> N_RANKS = {2, 1};

    GlobalAnomalyStats param(N_RANKS);

    // empty case
    EXPECT_EQ(0, param.collect_stat_data().size());

    param.add_anomaly_data(nlohmann::json::object({{"anomaly", AnomalyData(0, 0, 0,  0, 10, 10).get_json()}}).dump());
    param.add_anomaly_data(nlohmann::json::object({{"anomaly", AnomalyData(0, 0, 1, 11, 20, 20).get_json()}}).dump());
    param.add_anomaly_data(nlohmann::json::object({{"anomaly", AnomalyData(0, 0, 2, 21, 30, 30).get_json()}}).dump());

    nlohmann::json j = param.collect_stat_data();
    ASSERT_EQ(1, j.size());
    
    // std::cout << j.dump(2) << std::endl;

    j = j[0];
    ASSERT_EQ(3 , j["data"].size());

    EXPECT_EQ(0 , j["data"][0]["step"]);
    EXPECT_EQ(0 , j["data"][0]["min_timestamp"]);
    EXPECT_EQ(10, j["data"][0]["max_timestamp"]);
    EXPECT_EQ(10, j["data"][0]["n_anomalies"]);
    EXPECT_STREQ("0:0" , j["data"][0]["stat_id"].get<std::string>().c_str());

    EXPECT_EQ(1 , j["data"][1]["step"]);
    EXPECT_EQ(11 , j["data"][1]["min_timestamp"]);
    EXPECT_EQ(20, j["data"][1]["max_timestamp"]);
    EXPECT_EQ(20, j["data"][1]["n_anomalies"]);
    EXPECT_STREQ("0:0" , j["data"][1]["stat_id"].get<std::string>().c_str());

    EXPECT_EQ(2 , j["data"][2]["step"]);
    EXPECT_EQ(21 , j["data"][2]["min_timestamp"]);
    EXPECT_EQ(30, j["data"][2]["max_timestamp"]);
    EXPECT_EQ(30, j["data"][2]["n_anomalies"]);
    EXPECT_STREQ("0:0" , j["data"][2]["stat_id"].get<std::string>().c_str());

    EXPECT_STREQ("0:0", j["key"].get<std::string>().c_str());

    EXPECT_NEAR(-1.5, j["stats"]["kurtosis"], 1e-3);
    EXPECT_NEAR(20.0, j["stats"]["mean"], 1e-3);
    EXPECT_NEAR(60.0, j["stats"]["accumulate"], 1e-3);
    EXPECT_NEAR(30.0, j["stats"]["maximum"], 1e-3);
    EXPECT_NEAR(10.0, j["stats"]["minimum"], 1e-3);
    EXPECT_NEAR(3.0, j["stats"]["count"], 1e-3);
    EXPECT_NEAR(0.0, j["stats"]["skewness"], 1e-3);
    EXPECT_NEAR(10.0, j["stats"]["stddev"], 1e-3);

    param.add_anomaly_data(nlohmann::json::object({{"anomaly", AnomalyData(0, 0, 3, 31, 40, 40).get_json()}}).dump());
    param.add_anomaly_data(nlohmann::json::object({{"anomaly", AnomalyData(0, 1, 0,  0, 10, 10).get_json()}}).dump());
    param.add_anomaly_data(nlohmann::json::object({{"anomaly", AnomalyData(1, 0, 1, 11, 20, 20).get_json()}}).dump());

    j = param.collect_stat_data();
    EXPECT_EQ(3, j.size());
    for (auto& jj: j)
    {
        if (jj["key"].get<std::string>() == "1:0")
        {
            ASSERT_EQ(1, jj["data"].size());

            EXPECT_EQ(1 , jj["data"][0]["step"]);
            EXPECT_EQ(11 , jj["data"][0]["min_timestamp"]);
            EXPECT_EQ(20, jj["data"][0]["max_timestamp"]);
            EXPECT_EQ(20, jj["data"][0]["n_anomalies"]);
            EXPECT_STREQ("1:0" , jj["data"][0]["stat_id"].get<std::string>().c_str());

            EXPECT_NEAR(0.0, jj["stats"]["kurtosis"], 1e-3);
            EXPECT_NEAR(20.0, jj["stats"]["mean"], 1e-3);
            EXPECT_NEAR(20.0, jj["stats"]["accumulate"], 1e-3);
            EXPECT_NEAR(20.0, jj["stats"]["maximum"], 1e-3);
            EXPECT_NEAR(20.0, jj["stats"]["minimum"], 1e-3);
            EXPECT_NEAR(1.0, jj["stats"]["count"], 1e-3);
            EXPECT_NEAR(0.0, jj["stats"]["skewness"], 1e-3);
            EXPECT_NEAR(0.0, jj["stats"]["stddev"], 1e-3);
        }
        else if (jj["key"].get<std::string>() == "0:1")
        {
            ASSERT_EQ(1, jj["data"].size());

            EXPECT_EQ(0 , jj["data"][0]["step"]);
            EXPECT_EQ(0 , jj["data"][0]["min_timestamp"]);
            EXPECT_EQ(10, jj["data"][0]["max_timestamp"]);
            EXPECT_EQ(10, jj["data"][0]["n_anomalies"]);
            EXPECT_STREQ("0:1" , jj["data"][0]["stat_id"].get<std::string>().c_str());

            EXPECT_NEAR(0.0, jj["stats"]["kurtosis"], 1e-3);
            EXPECT_NEAR(10.0, jj["stats"]["mean"], 1e-3);
            EXPECT_NEAR(10.0, jj["stats"]["accumulate"], 1e-3);
            EXPECT_NEAR(10.0, jj["stats"]["maximum"], 1e-3);
            EXPECT_NEAR(10.0, jj["stats"]["minimum"], 1e-3);
            EXPECT_NEAR(1.0, jj["stats"]["count"], 1e-3);
            EXPECT_NEAR(0.0, jj["stats"]["skewness"], 1e-3);
            EXPECT_NEAR(0.0, jj["stats"]["stddev"], 1e-3);
        }
        else if (jj["key"].get<std::string>() == "0:0")
        {
            ASSERT_EQ(1, jj["data"].size());

            EXPECT_EQ(3 , jj["data"][0]["step"]);
            EXPECT_EQ(31 , jj["data"][0]["min_timestamp"]);
            EXPECT_EQ(40, jj["data"][0]["max_timestamp"]);
            EXPECT_EQ(40, jj["data"][0]["n_anomalies"]);
            EXPECT_STREQ("0:0" , jj["data"][0]["stat_id"].get<std::string>().c_str());

            EXPECT_NEAR(-1.36, jj["stats"]["kurtosis"], 1e-3);
            EXPECT_NEAR(25.0, jj["stats"]["mean"], 1e-3);
            EXPECT_NEAR(100.0, jj["stats"]["accumulate"], 1e-3);
            EXPECT_NEAR(40.0, jj["stats"]["maximum"], 1e-3);
            EXPECT_NEAR(10.0, jj["stats"]["minimum"], 1e-3);
            EXPECT_NEAR(4.0, jj["stats"]["count"], 1e-3);
            EXPECT_NEAR(0.0, jj["stats"]["skewness"], 1e-3);
            EXPECT_NEAR(12.9099, jj["stats"]["stddev"], 1e-3);
        }
        else
        {
            FAIL();
        }
    }
}

TEST(GlobalAnomalyStatsTest, AnomalyStatTest2)
{
    using namespace chimbuko;

    const std::vector<int> N_RANKS = {2};

    GlobalAnomalyStats param(N_RANKS);

    // empty case
    EXPECT_EQ(0, param.collect_stat_data().size());
    EXPECT_EQ(0, param.collect_func_data().size());

    std::unordered_map<unsigned long, RunStats> g_inclusive, g_exclusive;
    RunStats l_inclusive, l_exclusive;
    nlohmann::json j;

    l_inclusive.clear();
    l_inclusive.push(10); l_inclusive.push(10.5); l_inclusive.push(9.5);
    l_inclusive.push(10); l_inclusive.push(10.5); l_inclusive.push(9.5);
    l_inclusive.push(15); l_inclusive.push(15.5); l_inclusive.push(14.5); l_inclusive.push(6.5);
    l_exclusive.clear();
    l_exclusive.push(7); l_exclusive.push(7.5); l_exclusive.push(6.5);
    l_exclusive.push(7); l_exclusive.push(7.5); l_exclusive.push(6.5);
    l_exclusive.push(10); l_exclusive.push(10.5); l_exclusive.push(11.5); l_exclusive.push(4.5);
    g_inclusive[0] += l_inclusive; g_exclusive[0] += l_exclusive;
    param.add_anomaly_data(
        nlohmann::json::object({
            {"func", nlohmann::json::array({
                {
                    {"id", 0}, {"name", "func 0"}, {"n_anomaly", 4},
                    {"inclusive", l_inclusive.get_json_state()}, 
                    {"exclusive", l_exclusive.get_json_state()}
                }
            })}
        }).dump()
    );
    
    // func 1: 10 calls = 4 nomal + 6 abnormal
    l_inclusive.clear();
    l_inclusive.push(10); l_inclusive.push(10.5); l_inclusive.push(9.5);
    l_inclusive.push(10); l_inclusive.push(10.5); l_inclusive.push(9.5);
    l_inclusive.push(15); l_inclusive.push(15.5); l_inclusive.push(14.5); l_inclusive.push(6.5);
    l_exclusive.clear();
    l_exclusive.push(7); l_exclusive.push(7.5); l_exclusive.push(6.5);
    l_exclusive.push(7); l_exclusive.push(7.5); l_exclusive.push(6.5);
    l_exclusive.push(10); l_exclusive.push(10.5); l_exclusive.push(11.5); l_exclusive.push(4.5);
    g_inclusive[1] += l_inclusive; g_exclusive[1] += l_exclusive;
    param.add_anomaly_data(
        nlohmann::json::object({
            {"func", nlohmann::json::array({
                {
                    {"id", 1}, {"name", "func 1"}, {"n_anomaly", 4},
                    {"inclusive", l_inclusive.get_json_state()}, 
                    {"exclusive", l_exclusive.get_json_state()}
                }
            })}
        }).dump()
    );
    // func 0: 10 calls = 6 nomal + 4 abnormal
    l_inclusive.clear();
    l_inclusive.push(10); l_inclusive.push(10.5); l_inclusive.push(9.5);
    l_inclusive.push(10); l_inclusive.push(10.5); l_inclusive.push(9.5);
    l_inclusive.push(15); l_inclusive.push(15.5); l_inclusive.push(14.5); l_inclusive.push(6.5);
    l_exclusive.clear();
    l_exclusive.push(7); l_exclusive.push(7.5); l_exclusive.push(6.5);
    l_exclusive.push(7); l_exclusive.push(7.5); l_exclusive.push(6.5);
    l_exclusive.push(10); l_exclusive.push(10.5); l_exclusive.push(11.5); l_exclusive.push(4.5);
    g_inclusive[0] += l_inclusive; g_exclusive[0] += l_exclusive;
    param.add_anomaly_data(
        nlohmann::json::object({
            {"func", nlohmann::json::array({
                {
                    {"id", 0}, {"name", "func 0"}, {"n_anomaly", 4},
                    {"inclusive", l_inclusive.get_json_state()}, 
                    {"exclusive", l_exclusive.get_json_state()}
                }
            })}
        }).dump()
    );    
    // func 1: 10 calls = 4 nomal + 6 abnormal
    l_inclusive.clear();
    l_inclusive.push(10); l_inclusive.push(10.5); l_inclusive.push(9.5);
    l_inclusive.push(10); l_inclusive.push(10.5); l_inclusive.push(9.5);
    l_inclusive.push(15); l_inclusive.push(15.5); l_inclusive.push(14.5); l_inclusive.push(6.5);
    l_exclusive.clear();
    l_exclusive.push(7); l_exclusive.push(7.5); l_exclusive.push(6.5);
    l_exclusive.push(7); l_exclusive.push(7.5); l_exclusive.push(6.5);
    l_exclusive.push(10); l_exclusive.push(10.5); l_exclusive.push(11.5); l_exclusive.push(4.5);
    g_inclusive[1] += l_inclusive; g_exclusive[1] += l_exclusive;
    param.add_anomaly_data(
        nlohmann::json::object({
            {"func", nlohmann::json::array({
                {
                    {"id", 1}, {"name", "func 1"}, {"n_anomaly", 4},
                    {"inclusive", l_inclusive.get_json_state()}, 
                    {"exclusive", l_exclusive.get_json_state()}
                }
            })}
        }).dump()
    );    

    j = param.collect_func_data();
    
    EXPECT_EQ(2, j.size());

    for (auto f: j)
    {
        unsigned long fid = f["fid"];

        EXPECT_NEAR(g_exclusive[fid].kurtosis(), f["exclusive"]["kurtosis"], 1e-3);
        EXPECT_NEAR(g_exclusive[fid].mean(), f["exclusive"]["mean"], 1e-3);
        EXPECT_NEAR(g_exclusive[fid].accumulate(), f["exclusive"]["accumulate"], 1e-3);
        EXPECT_NEAR(g_exclusive[fid].maximum(), f["exclusive"]["maximum"], 1e-3);
        EXPECT_NEAR(g_exclusive[fid].minimum(), f["exclusive"]["minimum"], 1e-3);
        EXPECT_NEAR(g_exclusive[fid].count(), f["exclusive"]["count"], 1e-3);
        EXPECT_NEAR(g_exclusive[fid].skewness(), f["exclusive"]["skewness"], 1e-3);
        EXPECT_NEAR(g_exclusive[fid].stddev(), f["exclusive"]["stddev"], 1e-3);

        EXPECT_NEAR(g_inclusive[fid].kurtosis(), f["inclusive"]["kurtosis"], 1e-3);
        EXPECT_NEAR(g_inclusive[fid].mean(), f["inclusive"]["mean"], 1e-3);
        EXPECT_NEAR(g_inclusive[fid].accumulate(), f["inclusive"]["accumulate"], 1e-3);
        EXPECT_NEAR(g_inclusive[fid].maximum(), f["inclusive"]["maximum"], 1e-3);
        EXPECT_NEAR(g_inclusive[fid].minimum(), f["inclusive"]["minimum"], 1e-3);
        EXPECT_NEAR(g_inclusive[fid].count(), f["inclusive"]["count"], 1e-3);
        EXPECT_NEAR(g_inclusive[fid].skewness(), f["inclusive"]["skewness"], 1e-3);
        EXPECT_NEAR(g_inclusive[fid].stddev(), f["inclusive"]["stddev"], 1e-3);      
        
        EXPECT_EQ(8.0, f["stats"]["accumulate"]);
        EXPECT_EQ(2.0, f["stats"]["count"]);
        EXPECT_EQ(0.0, f["stats"]["kurtosis"]);
        EXPECT_EQ(4.0, f["stats"]["maximum"]);
        EXPECT_EQ(4.0, f["stats"]["mean"]);
        EXPECT_EQ(4.0, f["stats"]["minimum"]);
        EXPECT_EQ(0.0, f["stats"]["skewness"]);
        EXPECT_EQ(0.0, f["stats"]["stddev"]);
    }
}

