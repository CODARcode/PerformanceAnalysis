#include "chimbuko/param/sstd_param.hpp"
#include "chimbuko/message.hpp"
#include <gtest/gtest.h>
#include <random>

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

    const int n_functions = 100;
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

    msg.set_info(0, 1, MessageType::REQ_ADD, MessageKind::SSTD, 10);
    msg.set_msg(dummy, false);

    EXPECT_EQ(0, msg.src());
    EXPECT_EQ(1, msg.dst());
    EXPECT_EQ(MessageType::REQ_ADD, msg.type());
    EXPECT_EQ(MessageKind::SSTD, msg.kind());
    EXPECT_EQ(10, msg.frame());
    EXPECT_EQ((int)dummy.size(), msg.size());
    EXPECT_EQ((int)(dummy.size() + Message::Header::headerSize), msg.count());

    c_msg = msg.createReply();

    EXPECT_EQ(1, c_msg.src());
    EXPECT_EQ(0, c_msg.dst());
    EXPECT_EQ(MessageType::REP_ADD, c_msg.type());
    EXPECT_EQ(MessageKind::SSTD, c_msg.kind());
    EXPECT_EQ(10, c_msg.frame());

    dummy2 = msg.data().substr(Message::Header::headerSize);
    EXPECT_STREQ(dummy.c_str(), dummy2.c_str());
    EXPECT_STREQ(msg.data_buffer().c_str(), dummy2.c_str());

    std::stringstream ss(msg.data(), std::stringstream::in | std::stringstream::binary);
    ss >> c_head;
    EXPECT_EQ(0, c_head.src());
    EXPECT_EQ(1, c_head.dst());
    EXPECT_EQ(MessageType::REQ_ADD, c_head.type());
    EXPECT_EQ(MessageKind::SSTD, c_head.kind());
    EXPECT_EQ(10, c_head.frame());
}

TEST_F(ParamTest, SstdMessageTest)
{
    using namespace chimbuko;

    const int n_functions = 4;
    const double mean[n_functions] = {1.0, 10.0, 100.0, 1000.0};
    const double std[n_functions] = {0.1, 1.0, 10.0, 100.0};
    const int n_rolls = 1000;
    const int n_frames = 100;
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
        msg.set_info(1, 2, (int)chimbuko::MessageType::REQ_ADD, (int)chimbuko::MessageKind::SSTD, iFrame);
        msg.set_msg(l_param.serialize(), false);
        
        EXPECT_EQ(1, msg.src());
        EXPECT_EQ(2, msg.dst());
        EXPECT_EQ(chimbuko::MessageType::REQ_ADD, msg.type());
        EXPECT_EQ(chimbuko::MessageKind::SSTD, msg.kind());
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
        EXPECT_STREQ(msg.data_buffer().c_str(), c_msg.data_buffer().c_str());

        c_param.update(l_param);
        dummy = g_param.update(msg.data_buffer(), true);
        EXPECT_STREQ(c_param.serialize().c_str(), dummy.c_str());
    }
}

