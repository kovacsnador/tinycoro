#include <gtest/gtest.h>

#include <tinycoro/DetachedTask.hpp>
#include <mock/TaskMock.hpp>

#include <tinycoro/tinycoro_all.h>

template<typename T>
struct DetachedTaskTest : testing::Test
{   
    using value_type = T;
};

using DetachedTaskTestTypes = testing::Types<void, int32_t, bool, char>;

TYPED_TEST_SUITE(DetachedTaskTest, DetachedTaskTestTypes);

TYPED_TEST(DetachedTaskTest, DetachedTaskTest)
{ 
    using value_type = TestFixture::value_type;

    using task_t = tinycoro::test::TaskMock<value_type>;

    // always return as value_type DetachedTask 
    EXPECT_TRUE((std::same_as<typename tinycoro::Detach<task_t>::value_type, value_type>));
}

TEST(DetachedTaskTest, DetachedTaskTest_address)
{
    using task_t = tinycoro::test::TaskMock<void>;

    task_t task;

    tinycoro::Detach detach{std::move(task)};

    EXPECT_CALL(*task.mock, Address).Times(1).WillOnce(testing::Return(nullptr));

    auto address = detach.Address();
    EXPECT_EQ(address, nullptr);
}

TYPED_TEST(DetachedTaskTest, DetachedTaskTest_functional)
{
    using value_type = TestFixture::value_type;

    tinycoro::Scheduler scheduler;
    tinycoro::ManualEvent event;

    auto task = [&event]() -> tinycoro::Task<value_type> {
        co_await event;

        if constexpr (!std::same_as<void, value_type>)
        {
            co_return value_type{};
        }
    };

    auto intTask = []()->tinycoro::Task<int32_t> {
        co_return 42;
    };

    auto setter = [&event]() -> tinycoro::Task<> {
        event.Set();
        co_return;
    };

    // start a detached task
    auto [first, second] = tinycoro::AllOf(scheduler, tinycoro::Detach{task()}, intTask());

    EXPECT_TRUE((std::same_as<typename tinycoro::detail::ReturnT<value_type>::value_type, std::remove_reference_t<decltype(first.value())>>));
    EXPECT_TRUE((std::same_as<int32_t, std::remove_reference_t<decltype(second.value())>>));

    // wait until the detached task is done
    tinycoro::AllOf(scheduler, setter());

    EXPECT_TRUE(event.IsSet());
}

struct DetachedTaskFunctionalTest : testing::TestWithParam<size_t>
{
};

INSTANTIATE_TEST_SUITE_P(DetachedTaskFunctionalTest, DetachedTaskFunctionalTest, testing::Values(
    1, 10, 100, 1000, 5000
));

TEST_P(DetachedTaskFunctionalTest, DetachedTaskFunctionalTest_functional_01)
{
    auto count = GetParam();

    tinycoro::Scheduler scheduler;
    tinycoro::ManualEvent event;
    tinycoro::Latch latch{count};
    std::atomic<size_t> c;

    auto task = [&event, &c, &latch]()->tinycoro::Task<>
    {
        co_await event;
        c++;
        latch.CountDown();
    };

    for(size_t i = 0; i < count; ++i)
        tinycoro::AllOf(scheduler, tinycoro::Detach{task()});

    event.Set();

    auto waiter = [&latch]()->tinycoro::Task<>
    {
        co_await latch;
    };

    tinycoro::AllOf(waiter());

    EXPECT_EQ(count, c);
}

TEST_P(DetachedTaskFunctionalTest, DetachedTaskFunctionalTest_functional_02)
{
    auto count = GetParam();

    tinycoro::Scheduler scheduler;
    tinycoro::ManualEvent event;

    auto task = [&event]()->tinycoro::Task<>
    {
        co_await event;
    };

    for(size_t i = 0; i < count; ++i)
        tinycoro::AllOf(scheduler, tinycoro::Detach{task()});

    event.Set();

    // test for sanitizers
}