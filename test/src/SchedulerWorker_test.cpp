#include <gtest/gtest.h>

#include <tinycoro/tinycoro_all.h>

TEST(SchedulerWorkerTest, SchedulerWorkerTest_PushTask)
{
    std::stop_source ss;
    tinycoro::detail::AtomicQueue<size_t, 2> queue;

    EXPECT_TRUE(tinycoro::detail::helper::PushTask(1, queue, ss));

    ss.request_stop();
    EXPECT_FALSE(tinycoro::detail::helper::PushTask(2, queue, ss));
}