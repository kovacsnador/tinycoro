#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <random>

#include <tinycoro/tinycoro_all.h>

struct CancellationTokenMock
{
    CancellationTokenMock() { }

    CancellationTokenMock(auto) { }
};

TEST(SoftClockTest, SoftClockTest_registration_token_type)
{
    tinycoro::detail::SoftClock<CancellationTokenMock, std::chrono::milliseconds> clock{};

    using clock_t = tinycoro::detail::SoftClock<CancellationTokenMock, std::chrono::milliseconds>;

    using tokenType1 = decltype(std::declval<clock_t>().RegisterWithCancellation([]() noexcept { }, 50ms));
    using tokenType2 = decltype(std::declval<clock_t>().RegisterWithCancellation([]() noexcept { }, std::declval<clock_t>().Now() + 50ms));

    EXPECT_TRUE((std::same_as<CancellationTokenMock, tokenType1>));
    EXPECT_TRUE((std::same_as<CancellationTokenMock, tokenType2>));
}

TEST(SoftClockTest, SoftClockTest_simple)
{
    tinycoro::SoftClock clock{};

    bool called{false};

    auto token = clock.RegisterWithCancellation([&called]() noexcept { called = true; }, 1ms);
    std::this_thread::sleep_for(100ms);

    EXPECT_TRUE(called);
}

TEST(SoftClockTest, SoftClockTest_move_token)
{
    tinycoro::SoftClock clock;

    bool called{false};
    auto token = clock.RegisterWithCancellation([&called]() noexcept { called = true; }, 300s);

    tinycoro::SoftClockCancelToken token2{std::move(token)};

    // this should cancel nothing
    EXPECT_FALSE(token.TryCancel());
    EXPECT_TRUE(token2.TryCancel());

    // for the second time this should return false
    EXPECT_FALSE(token2.TryCancel());
}

TEST(SoftClockTest, SoftClockTest_move_token_destroy_clock)
{
    tinycoro::SoftClockCancelToken token1;
    tinycoro::SoftClockCancelToken token2;
    tinycoro::SoftClockCancelToken token3;

    {
        tinycoro::SoftClock clock;

        token1 = clock.RegisterWithCancellation([]() noexcept { }, 300s);
        token2 = clock.RegisterWithCancellation([]() noexcept { }, 300s);
        token3 = clock.RegisterWithCancellation([]() noexcept { }, 300s);
    }

    EXPECT_FALSE(token1.TryCancel());
    EXPECT_FALSE(token2.TryCancel());
    EXPECT_FALSE(token3.TryCancel());
}

TEST(SoftClockTest, SoftClockTest_ignore_token)
{
    size_t count{};

    tinycoro::SoftClock clock;

    std::ignore = clock.RegisterWithCancellation([&count]() noexcept { ++count; }, 100s);
    std::ignore = clock.RegisterWithCancellation([&count]() noexcept { ++count; }, clock.Now() + 100s);
    std::ignore = clock.RegisterWithCancellation([&count]() noexcept { ++count; }, 100s);

    // all the tokes are destoyed
    // all the events are cancelled

    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(count, 0);
}

TEST(SoftClockTest, SoftClockTest_destroy_clock)
{
    size_t count{0};

    {
        tinycoro::SoftClock clock;

        auto token = clock.RegisterWithCancellation([&count]() noexcept { count++; }, 300s);
        token      = clock.RegisterWithCancellation([&count]() noexcept { count++; }, 300s);
        token      = clock.RegisterWithCancellation([&count]() noexcept { count++; }, 300s);

        // everything should be cancelled here...
    }

    EXPECT_EQ(count, 0);
}

TEST(SoftClockTest, SoftClockTest_move_token_timed_out)
{
    tinycoro::SoftClockCancelToken token1;
    tinycoro::SoftClockCancelToken token2;
    tinycoro::SoftClockCancelToken token3;
    tinycoro::SoftClockCancelToken token4;
    tinycoro::SoftClockCancelToken token5;

    tinycoro::SoftClock clock{};

    bool t1{false};
    bool t2{false};
    bool t3{false};
    bool t4{false};
    bool t5{false};

    token1 = clock.RegisterWithCancellation([&t1]() noexcept { t1 = true; }, 10ms);
    token2 = clock.RegisterWithCancellation([&t2]() noexcept { t2 = true; }, 20ms);
    token3 = clock.RegisterWithCancellation([&t3]() noexcept { t3 = true; }, 10ms);
    token4 = clock.RegisterWithCancellation([&t4]() noexcept { t4 = true; }, 10ms);
    token5 = clock.RegisterWithCancellation([&t5]() noexcept { t5 = true; }, 10ms);

    std::this_thread::sleep_for(100ms);

    // those should timed out
    EXPECT_FALSE(token1.TryCancel());
    EXPECT_FALSE(token2.TryCancel());
    EXPECT_FALSE(token3.TryCancel());
    EXPECT_FALSE(token4.TryCancel());
    EXPECT_FALSE(token5.TryCancel());

    // all callback need to be called
    EXPECT_TRUE(t1);
    EXPECT_TRUE(t2);
    EXPECT_TRUE(t3);
    EXPECT_TRUE(t4);
    EXPECT_TRUE(t5);
}

TEST(SoftClockTest, SoftClockTest_timed_out)
{
    tinycoro::SoftClock clock{};

    bool t1{false};
    bool t2{false};
    bool t3{false};
    bool t4{false};
    bool t5{false};

    clock.Register([&t1]() noexcept { t1 = true; }, 10ms);
    clock.Register([&t2]() noexcept { t2 = true; }, 20ms);
    clock.Register([&t3]() noexcept { t3 = true; }, 10ms);
    clock.Register([&t4]() noexcept { t4 = true; }, 10ms);
    clock.Register([&t5]() noexcept { t5 = true; }, 10ms);

    std::this_thread::sleep_for(100ms);

    // all callback need to be called
    EXPECT_TRUE(t1);
    EXPECT_TRUE(t2);
    EXPECT_TRUE(t3);
    EXPECT_TRUE(t4);
    EXPECT_TRUE(t5);
}

TEST(SoftClockTest, SoftClockTest_token_mixed)
{
    tinycoro::SoftClock clock{};

    bool t1{false};
    bool t2{false};
    bool t3{false};
    bool t4{false};
    bool t5{false};

    auto token1 = clock.RegisterWithCancellation([&t1]() noexcept { t1 = true; }, 300ms);
    auto token2 = clock.RegisterWithCancellation([&t2]() noexcept { t2 = true; }, 10ms);
    auto token3 = clock.RegisterWithCancellation([&t3]() noexcept { t3 = true; }, 300ms);
    auto token4 = clock.RegisterWithCancellation([&t4]() noexcept { t4 = true; }, 10ms);
    auto token5 = clock.RegisterWithCancellation([&t5]() noexcept { t5 = true; }, 300ms);

    EXPECT_TRUE(token1.TryCancel());
    EXPECT_TRUE(token3.TryCancel());
    EXPECT_TRUE(token5.TryCancel());

    std::this_thread::sleep_for(100ms);

    // all callback need to be called
    EXPECT_FALSE(t1);
    EXPECT_TRUE(t2);
    EXPECT_FALSE(t3);
    EXPECT_TRUE(t4);
    EXPECT_FALSE(t5);
}

TEST(SoftClockTest, SoftClockTest_stop_token)
{
    std::stop_source stopSource;

    tinycoro::SoftClock clock{stopSource.get_token()};

    EXPECT_FALSE(clock.StopRequested());

    auto token = clock.RegisterWithCancellation([&stopSource]() noexcept { stopSource.request_stop(); }, 10ms);

    // make sure the stop_source is called
    std::this_thread::sleep_for(200ms);

    EXPECT_TRUE(clock.StopRequested());

    // those should timed out
    EXPECT_FALSE(token.TryCancel());
}

TEST(SoftClockTest, SoftClockTest_stop_and_start)
{
    tinycoro::SoftClock clock{};

    std::this_thread::sleep_for(100ms);

    // now the clock should be empty and waiting
    auto token = clock.RegisterWithCancellation([]() noexcept { }, 10ms);

    // event is called for sure
    std::this_thread::sleep_for(100ms);

    // those should timed out
    EXPECT_FALSE(token.TryCancel());
}

struct SoftClockTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(SoftClockTest, SoftClockTest, testing::Values(2, 10, 100, 1000, 10000));

TEST_P(SoftClockTest, SoftClockFunctionalTest_ignore_tokens)
{
    const auto count = GetParam();

    tinycoro::SoftClock clock{};

    for (size_t i = 0; i < count; ++i)
    {
        std::ignore = clock.RegisterWithCancellation([]() noexcept { }, 10ms);
    }
}

TEST_P(SoftClockTest, SoftClockFunctionalTest_wait_completion_token)
{
    const auto count = GetParam();

    tinycoro::Latch latch{count};
    std::atomic<bool> fff{false};

    tinycoro::SoftClock clock{};

    std::vector<tinycoro::SoftClockCancelToken> tokens;

    for (size_t i = 0; i < count; ++i)
    {
        tokens.push_back(clock.RegisterWithCancellation([&]() noexcept { 
            
            fff = true;
            
            latch.CountDown(); }, 50ms));
    }

    auto waiter = [&latch]() -> tinycoro::Task<void> { co_await latch; };

    // waits for all the events
    tinycoro::AllOfInline(waiter());
}

TEST_P(SoftClockTest, SoftClockFunctionalTest_requestStop_test)
{
    const auto count = GetParam();

    size_t c{};

    tinycoro::SoftClock clock{};

    for (size_t i = 0; i < count; ++i)
    {
        clock.Register([&c]() noexcept { c++; }, 2000ms);
    }

    clock.RequestStop();

    EXPECT_EQ(c, 0);
}

TEST_P(SoftClockTest, SoftClockFunctionalTest_destructor_test)
{
    const auto count = GetParam();

    size_t c{};

    auto onExit = tinycoro::Finally([&c] { EXPECT_EQ(c, 0); });

    tinycoro::SoftClock clock{};

    for (size_t i = 0; i < count; ++i)
    {
        clock.Register([&c]() noexcept { c++; }, 2000ms);
    }
}

TEST_P(SoftClockTest, SoftClockFunctionalTest_wait_complition)
{
    const auto count = GetParam();

    tinycoro::Latch latch{count};

    tinycoro::SoftClock clock{};

    for (size_t i = 0; i < count; ++i)
    {
        clock.Register([&latch]() noexcept { latch.CountDown(); }, 50ms);
    }

    auto waiter = [&latch]() -> tinycoro::Task<void> { co_await latch; };

    // waits for all the events
    tinycoro::AllOfInline(waiter());
}

TEST_P(SoftClockTest, SoftClockFunctionalTest_wait_complition_token_cancel)
{
    const auto count = GetParam();

    tinycoro::Latch latch{count / 2};

    tinycoro::SoftClock clock{};

    std::vector<tinycoro::SoftClockCancelToken> tokens;

    for (size_t i = 0; i < count; ++i)
    {
        if (i % 2)
        {
            auto token = clock.RegisterWithCancellation([]() noexcept { }, 2000ms);
            token.TryCancel();
        }
        else
        {
            tokens.push_back(clock.RegisterWithCancellation([&latch]() noexcept { latch.CountDown(); }, 50ms));
        }
    }

    auto waiter = [&latch]() -> tinycoro::Task<void> { co_await latch; };

    // waits for all the events
    tinycoro::AllOfInline(waiter());
}

TEST_P(SoftClockTest, SoftClockFunctionalTest_multiThreaded)
{
    const auto count = GetParam();

    tinycoro::Barrier   barrier{count};
    tinycoro::SoftClock clock{};

    tinycoro::Scheduler scheduler;

    std::atomic<size_t> c{0};

    auto setTimer = [&]() -> tinycoro::Task<void> {
        clock.Register(
            [&]() noexcept {
                c++;
                barrier.Arrive();
            },
            20ms);
        co_await barrier;
    };

    std::vector<tinycoro::Task<void>> tasks;

    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(setTimer());
    }

    tinycoro::AllOf(scheduler, std::move(tasks));

    EXPECT_EQ(count, c);
}

TEST_P(SoftClockTest, SoftClockFunctionalTest_multiThreaded_measure_1)
{
    const auto count = GetParam();

    tinycoro::SoftClock clock{};

    tinycoro::Scheduler scheduler;

    auto duration = 200ms;

    auto measure = [&]() -> tinycoro::Task<void> {
        auto start = clock.Now();
        co_await tinycoro::SleepFor(clock, duration);
        EXPECT_TRUE(start + duration <= clock.Now());
    };

    std::vector<tinycoro::Task<void>> tasks;

    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(measure());
    }

    tinycoro::AllOf(scheduler, std::move(tasks));
}

TEST_P(SoftClockTest, SoftClockFunctionalTest_multiThreaded_measure_timedOut)
{
    const auto count = GetParam();

    tinycoro::SoftClock clock{};

    tinycoro::Scheduler scheduler;

    auto duration = 200ms;

    auto measure = [&](auto duration) -> tinycoro::Task<void> {
        auto start = clock.Now();
        co_await tinycoro::SleepFor(clock, duration);
        EXPECT_TRUE(start + duration <= clock.Now());
    };

    std::vector<tinycoro::Task<void>> tasks;

    for (size_t i = 0; i < count; ++i)
    {
        if(i % 2 == 0)
        {
            tasks.push_back(measure(duration));
        }
        else
        {
            // should be timed out
            tasks.push_back(measure(-duration));
        }
        
    }

    tinycoro::AllOf(scheduler, std::move(tasks));
}

TEST_P(SoftClockTest, SoftClockFunctionalTest_multiThreaded_measure_1_random)
{
    const auto count = GetParam();

    tinycoro::SoftClock clock{};

    tinycoro::Scheduler scheduler;

    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_real_distribution<double> dist(40, 400);

    auto measure = [&]() -> tinycoro::Task<void> {
        std::chrono::milliseconds duration(static_cast<int32_t>(dist(mt)));

        auto start = clock.Now();
        co_await tinycoro::SleepFor(clock, duration);
        EXPECT_TRUE(start + duration <= clock.Now());
    };

    std::vector<tinycoro::Task<void>> tasks;

    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(measure());
    }

    tinycoro::AllOf(scheduler, std::move(tasks));
}

TEST_P(SoftClockTest, SoftClockFunctionalTest_multiThreaded_measure_2)
{
    const auto count = GetParam();

    tinycoro::SoftClock clock{};

    tinycoro::Scheduler scheduler;

    auto duration = 200ms;

    auto measure = [&]() -> tinycoro::Task<void> {
        auto tp = clock.Now() + duration;
        co_await tinycoro::SleepUntil(clock, tp);
        EXPECT_TRUE(tp <= clock.Now());
    };

    std::vector<tinycoro::Task<void>> tasks;

    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(measure());
    }

    tinycoro::AllOf(scheduler, std::move(tasks));
}

TEST_P(SoftClockTest, SoftClockFunctionalTest_multiThreaded_measure_2_random)
{
    const auto count = GetParam();

    tinycoro::SoftClock clock{};

    tinycoro::Scheduler scheduler;

    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_real_distribution<double> dist(40, 400);

    auto measure = [&]() -> tinycoro::Task<void> {
        std::chrono::milliseconds duration(static_cast<int32_t>(dist(mt)));

        auto tp = clock.Now() + duration;
        co_await tinycoro::SleepUntil(clock, tp);
        EXPECT_TRUE(tp <= clock.Now());
    };

    std::vector<tinycoro::Task<void>> tasks;

    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(measure());
    }

    tinycoro::AllOf(scheduler, std::move(tasks));
}

TEST_P(SoftClockTest, SoftClockFunctionalTest_multiThreaded_with_token)
{
    const auto count = GetParam();

    tinycoro::Barrier   barrier{count};
    tinycoro::SoftClock clock{};

    tinycoro::Scheduler scheduler;

    std::atomic<size_t> c{0};

    auto setTimer = [&]() -> tinycoro::Task<void> {
        auto token = clock.RegisterWithCancellation(
            [&]() noexcept {
                c++;
                barrier.Arrive();
            },
            20ms);
        co_await barrier;
    };

    std::vector<tinycoro::Task<void>> tasks;

    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(setTimer());
    }

    tinycoro::AllOf(scheduler, std::move(tasks));

    EXPECT_EQ(count, c);
}

TEST_P(SoftClockTest, SoftClockFunctionalTest_multiThreaded_with_token_cancel)
{
    const auto count = GetParam();

    tinycoro::SoftClock clock{};

    tinycoro::Scheduler scheduler;

    std::atomic<size_t> c{0};

    auto setTimer = [&]() -> tinycoro::Task<void> {
        auto token = clock.RegisterWithCancellation([&]() noexcept { c++; }, 2000ms);

        // here should be cancelled
        co_return;
    };

    std::vector<tinycoro::Task<void>> tasks;

    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(setTimer());
    }

    tinycoro::AllOf(scheduler, std::move(tasks));

    // should be 0
    EXPECT_EQ(0, c);
}

TEST_P(SoftClockTest, SoftClockFunctionalTest_multiThreaded_with_token_cancel_stop_token)
{
    const auto count = GetParam();

    std::stop_source ss;

    tinycoro::Latch     latch{1};
    tinycoro::SoftClock clock{ss.get_token()};

    tinycoro::Scheduler scheduler;

    std::atomic<size_t> c{0};

    auto setTimer = [&]() -> tinycoro::Task<void> {
        auto token = clock.RegisterWithCancellation([&]() noexcept { c++; }, 20s);

        // here should be cancelled
        co_await latch;
    };

    auto canceller = [&]() -> tinycoro::Task<void> {
        ss.request_stop();
        latch.CountDown();
        co_return;
    };

    std::vector<tinycoro::Task<void>> tasks;

    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(setTimer());
    }

    tasks.push_back(canceller());

    tinycoro::AllOf(scheduler, std::move(tasks));

    // should be 0
    EXPECT_EQ(0, c);
}

TEST_P(SoftClockTest, SoftClockFunctionalTest_multiThreaded_close)
{
    const auto count = GetParam();

    tinycoro::SoftClock clock{};

    tinycoro::Scheduler scheduler;

    std::atomic<size_t> c{0};

    auto setTimer = [&]() -> tinycoro::Task<void> {
        auto token = clock.RegisterWithCancellation([&]() noexcept { c++; }, 2000ms);

        // here should be cancelled
        co_return;
    };

    auto closer = [&clock]() -> tinycoro::Task<void> {
        clock.RequestStop();
        co_return;
    };

    std::vector<tinycoro::Task<void>> tasks;

    for (size_t i = 0; i < count; ++i)
    {
        tasks.push_back(setTimer());

        if (i == count / 2)
        {
            // push the closer task in the middle somewhere
            tasks.push_back(closer());
        }
    }

    tinycoro::AllOf(scheduler, std::move(tasks));

    // should be 0
    EXPECT_EQ(0, c);
}