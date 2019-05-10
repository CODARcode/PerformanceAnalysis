#include "chimbuko/param/sstd_param.hpp"
#include <gtest/gtest.h>

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
            l_param_a[id].push(j % 10);
        }
    }

    g_param.update(l_param_a.serialize());
    for (int i = 0; i < n_functions; i++) {
        unsigned long id = (unsigned long)i;
        EXPECT_DOUBLE_EQ(l_param_a[id].mean(), g_param[id].mean());
        EXPECT_DOUBLE_EQ(l_param_a[id].std(), g_param[id].std());
        for (int j = 0; j < n_rolls; j++) {
            l_param_b[id].push(j % 20);
        }
    }

    g_param.update(l_param_b.serialize());
    l_param_a.update(l_param_b);
    for (int i = 0; i < n_functions; i++) {
        unsigned long id = (unsigned long)i;
        EXPECT_DOUBLE_EQ(l_param_a[id].mean(), g_param[id].mean());
        EXPECT_DOUBLE_EQ(l_param_a[id].std(), g_param[id].std());
    }

    EXPECT_STREQ(g_param.serialize().c_str(), l_param_a.serialize().c_str());
}

