#include <GameCore/GameMath.h>

#include <cmath>

#include "Utils.h"

#include "gtest/gtest.h"

TEST(GameMathTests, FastPowTest_Basic)
{
    float result = FastPow(0.1f, 2.0f);
    float expectedResult = 0.01f;

    EXPECT_TRUE(ApproxEquals(result, expectedResult, 0.0001f));
}

class FastPowTest : public testing::TestWithParam<std::tuple<float, float, float>>
{
public:
    virtual void SetUp() {}
    virtual void TearDown() {}
};

INSTANTIATE_TEST_CASE_P(
	GameMathTests,
    FastPowTest,
    ::testing::Values(
        std::make_tuple(0.0f, 0.0f, 0.001f),
        std::make_tuple(0.0f, 0.1f, 0.001f),
        std::make_tuple(0.0f, 0.5f, 0.001f),
        std::make_tuple(0.0f, 1.0f, 0.001f),
        std::make_tuple(0.0f, 2.0f, 0.001f),

        std::make_tuple(1.0f, 0.0f, 0.001f),
        std::make_tuple(1.0f, 0.1f, 0.001f),
        std::make_tuple(1.0f, 0.5f, 0.001f),
        std::make_tuple(1.0f, 1.0f, 0.001f),
        std::make_tuple(1.0f, 2.0f, 0.001f),

        std::make_tuple(1.5f, 0.0f, 0.001f),
        std::make_tuple(1.5f, 0.1f, 0.001f),
        std::make_tuple(1.5f, 0.5f, 0.001f),
        std::make_tuple(1.5f, 1.0f, 0.001f),
        std::make_tuple(1.5f, 2.0f, 0.001f),
        std::make_tuple(1.5f, 2.1f, 0.001f),
        std::make_tuple(1.5f, 4.1f, 0.001f),

        std::make_tuple(2.0f, 0.0f, 0.001f),
        std::make_tuple(2.0f, 0.1f, 0.001f),
        std::make_tuple(2.0f, 0.5f, 0.001f),
        std::make_tuple(2.0f, 1.0f, 0.001f),
        std::make_tuple(2.0f, 2.0f, 0.001f),

        std::make_tuple(10.0f, 0.0f, 0.001f),
        std::make_tuple(10.0f, 0.001f, 0.001f),
        std::make_tuple(10.0f, 0.1f, 0.001f),
        std::make_tuple(10.0f, 0.99f, 0.001f),
        std::make_tuple(10.0f, 1.0f, 0.001f),
        std::make_tuple(10.0f, 1.001f, 0.001f),
        std::make_tuple(10.0f, 1.1f, 0.001f),
        std::make_tuple(10.0f, 1.5f, 0.1f),
        std::make_tuple(10.0f, 1.99f, 0.1f),
        std::make_tuple(10.0f, 2.0f, 1.0f),
        std::make_tuple(10.0f, 3.0f, 1.0f)
    ));

TEST_P(FastPowTest, FastPowTest)
{
    float result = FastPow(std::get<0>(GetParam()), std::get<1>(GetParam()));
    float expectedResult = pow(std::get<0>(GetParam()), std::get<1>(GetParam()));

    EXPECT_TRUE(ApproxEquals(result, expectedResult, std::get<2>(GetParam())));
}

class FastExpTest : public testing::TestWithParam<std::tuple<float, float>>
{
public:
    virtual void SetUp() {}
    virtual void TearDown() {}
};

INSTANTIATE_TEST_CASE_P(
    GameMathTests,
    FastExpTest,
    ::testing::Values(
        std::make_tuple(-5.0f, 0.001f),
        std::make_tuple(-4.0f, 0.001f),
        std::make_tuple(-1.001f, 0.001f),
        std::make_tuple(-1.0f, 0.001f),
        std::make_tuple(-0.9f, 0.001f),
        std::make_tuple(-0.1f, 0.001f),
        std::make_tuple(-0.001f, 0.001f),
        std::make_tuple(0.0f, 0.001f),
        std::make_tuple(0.001f, 0.001f),
        std::make_tuple(0.1f, 0.001f),
        std::make_tuple(0.9f, 0.001f),
        std::make_tuple(1.0f, 0.001f),
        std::make_tuple(1.001f, 0.001f),
        std::make_tuple(1.1f, 0.001f),
        std::make_tuple(4.0f, 0.01f),
        std::make_tuple(5.0f, 0.01f)
    ));

TEST_P(FastExpTest, FastExpTest)
{
    float result = FastExp(std::get<0>(GetParam()));
    float expectedResult = exp(std::get<0>(GetParam()));

    EXPECT_TRUE(ApproxEquals(result, expectedResult, std::get<1>(GetParam())));
}

// COMPARISON STICK
#include <cmath>
float FastFastLog2_1(float x)
{
    return logb(x);
}
TEST(GameMathTests, FastFastLog2_1_Basic)
{
    EXPECT_EQ(FastFastLog2_1(0.1f), -4.0f);
    EXPECT_EQ(FastFastLog2_1(0.5f), -1.0f);
    EXPECT_EQ(FastFastLog2_1(1.0f), 0.0f);
    EXPECT_EQ(FastFastLog2_1(2.0f), 1.0f);
    EXPECT_EQ(FastFastLog2_1(1024.0f), 10.0f);
    EXPECT_EQ(FastFastLog2_1(1700.0f), 10.0f);
    EXPECT_EQ(FastFastLog2_1(65536.0f), 16.0f);
    EXPECT_EQ(FastFastLog2_1(1000000.0f), 19.0f);
}

TEST(GameMathTests, DiscreteLog2_Basic)
{
    EXPECT_EQ(DiscreteLog2(0.1f), -4.0f);
    EXPECT_EQ(DiscreteLog2(0.5f), -1.0f);
    EXPECT_EQ(DiscreteLog2(1.0f), 0.0f);
    EXPECT_EQ(DiscreteLog2(1.5f), 0.0f);
    EXPECT_EQ(DiscreteLog2(2.0f), 1.0f);
    EXPECT_EQ(DiscreteLog2(1024.0f), 10.0f);
    EXPECT_EQ(DiscreteLog2(1700.0f), 10.0f);
    EXPECT_EQ(DiscreteLog2(65536.0f), 16.0f);
    EXPECT_EQ(DiscreteLog2(1000000.0f), 19.0f);
}

TEST(GameMathTests, Clamp_Basic)
{
    EXPECT_EQ(2.0f, Clamp(-1.0f, 2.0f, 4.0f));
    EXPECT_EQ(4.0f, Clamp(12.0f, 2.0f, 4.0f));
    EXPECT_EQ(3.0f, Clamp(3.0f, 2.0f, 4.0f));
}

TEST(GameMathTests, LinearStep_Basic)
{
    EXPECT_EQ(0.0f, LinearStep(1.0f, 2.0f, 0.5f));
    EXPECT_EQ(1.0f, LinearStep(1.0f, 2.0f, 2.5f));

    EXPECT_TRUE(ApproxEquals(LinearStep(1.0f, 2.0f, 1.25f), 0.25f, 0.05f));
    EXPECT_TRUE(ApproxEquals(LinearStep(1.0f, 2.0f, 1.5f), 0.5f, 0.05f));
    EXPECT_TRUE(ApproxEquals(LinearStep(1.0f, 2.0f, 1.75f), 0.75f, 0.05f));
}

TEST(GameMathTests, SmoothStep_Basic)
{
    EXPECT_EQ(0.0f, SmoothStep(1.0f, 2.0f, 0.5f));
    EXPECT_EQ(1.0f, SmoothStep(1.0f, 2.0f, 2.5f));

    EXPECT_TRUE(ApproxEquals(SmoothStep(1.0f, 2.0f, 1.25f), 0.15f, 0.05f));
    EXPECT_TRUE(ApproxEquals(SmoothStep(1.0f, 2.0f, 1.5f), 0.5f, 0.05f));
    EXPECT_TRUE(ApproxEquals(SmoothStep(1.0f, 2.0f, 1.75f), 0.85f, 0.05f));
}

TEST(GameMathTests, MixPiecewiseLinear_Basic)
{
	EXPECT_TRUE(ApproxEquals(MixPiecewiseLinear(0.3f, 10.0f, 20.0f, 0.1f, 100.0f, 0.1f), 0.3f, 0.05f));

	EXPECT_TRUE(ApproxEquals(MixPiecewiseLinear(0.3f, 10.0f, 20.0f, 0.1f, 100.0f, 0.1f + (1.0f - 0.1f) / 2.0f), 0.3f + (10.0f - 0.3f) / 2.0f, 0.05f));

	EXPECT_TRUE(ApproxEquals(MixPiecewiseLinear(0.3f, 10.0f, 20.0f, 0.1f, 100.0f, 1.0f), 10.0f, 0.05f));

	EXPECT_TRUE(ApproxEquals(MixPiecewiseLinear(0.3f, 10.0f, 20.0f, 0.1f, 100.0f, 1.0f + (100.0f - 1.0f) / 2.0f), 10.0f + (20.0f - 10.0f) / 2.0f, 0.05f));

	EXPECT_TRUE(ApproxEquals(MixPiecewiseLinear(0.3f, 10.0f, 20.0f, 0.1f, 100.0f, 100.0f), 20.0f, 0.05f));
}