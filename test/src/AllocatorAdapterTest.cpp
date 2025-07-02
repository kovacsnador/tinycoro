#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <tinycoro/tinycoro_all.h>

struct MockAllocatorTracer
{
    MOCK_METHOD(void*, allocate_bytes, (size_t));
    MOCK_METHOD(void, deallocate_bytes, (void*, size_t), (noexcept));
    MOCK_METHOD(void*, allocate_bytes_noexcept, (size_t), (noexcept));
    MOCK_METHOD(void, get_return_object_on_allocation_failure, ());
};

static MockAllocatorTracer* g_currentMockPtr{nullptr};

template <typename PromiseT>
struct AllocatorAdapterNoExcept
{
    // ensure the use of non-throwing operator-new
    [[noreturn]] static std::coroutine_handle<PromiseT> get_return_object_on_allocation_failure()
    {
        g_currentMockPtr->get_return_object_on_allocation_failure();

        // as backup always throw
        throw std::bad_alloc{};
    }

    [[nodiscard]] static void* operator new(size_t nbytes) noexcept { return g_currentMockPtr->allocate_bytes_noexcept(nbytes); }

    static void operator delete(void* ptr, [[maybe_unused]] size_t nbytes) noexcept { g_currentMockPtr->deallocate_bytes(ptr, nbytes); }
};

template <typename PromiseT>
struct AllocatorAdapter
{
    [[nodiscard]] static void* operator new(size_t nbytes) { return g_currentMockPtr->allocate_bytes(nbytes); }

    static void operator delete(void* ptr, [[maybe_unused]] size_t nbytes) noexcept { g_currentMockPtr->deallocate_bytes(ptr, nbytes); }
};

struct AllocatorAdapterTest : testing::Test
{
    void SetUp() override { g_currentMockPtr = std::addressof(mock); }

    void TearDown() override { g_currentMockPtr = nullptr; }

    MockAllocatorTracer mock;
};

TEST_F(AllocatorAdapterTest, AllocatorAdapterTest_RunInline)
{
    EXPECT_CALL(mock, allocate_bytes_noexcept).Times(1).WillOnce([](size_t nbytes) { return std::malloc(nbytes); });
    EXPECT_CALL(mock, deallocate_bytes).Times(1).WillOnce([](void* p, [[maybe_unused]] size_t nbytes) { return std::free(p); });

    auto task = []() -> tinycoro::Task<int32_t, AllocatorAdapterNoExcept> {
        int32_t i{};
        i++;
        co_return i;
    };

    auto ret = tinycoro::AllOfInline(task());
    EXPECT_EQ(ret, 1);
}

TEST_F(AllocatorAdapterTest, AllocatorAdapterTest_Async)
{
    tinycoro::Scheduler scheduler;

    EXPECT_CALL(mock, allocate_bytes_noexcept).Times(3).WillRepeatedly([](size_t nbytes) { return std::malloc(nbytes); });
    EXPECT_CALL(mock, deallocate_bytes).Times(3).WillRepeatedly([](void* p, [[maybe_unused]] size_t nbytes) { return std::free(p); });

    auto task = [](int32_t i) -> tinycoro::Task<int32_t, AllocatorAdapterNoExcept> {
        i++;
        co_return i;
    };

    auto [t1, t2, t3] = tinycoro::AllOf(scheduler, task(0), task(1), task(2));

    EXPECT_EQ(t1, 1);
    EXPECT_EQ(t2, 2);
    EXPECT_EQ(t3, 3);
}

void bad_alloc(auto& mock, auto task)
{
    EXPECT_CALL(mock, allocate_bytes_noexcept).Times(1).WillRepeatedly([]([[maybe_unused]] size_t nbytes) { return nullptr; });
    EXPECT_CALL(mock, get_return_object_on_allocation_failure).Times(1).WillRepeatedly([] { throw std::bad_alloc{}; });

    auto func = [&] { std::ignore = tinycoro::AllOfInline(task(0)); };

    EXPECT_THROW(func(), std::bad_alloc);
}

TEST_F(AllocatorAdapterTest, AllocatorAdapterTest_bad_alloc)
{
    auto task = [](int32_t i) -> tinycoro::Task<int32_t, AllocatorAdapterNoExcept> {
        i++;
        co_return i;
    };
    bad_alloc(mock, std::move(task));
}

TEST_F(AllocatorAdapterTest, AllocatorAdapterTest_bad_alloc_inlineTask)
{
    auto task = [](int32_t i) -> tinycoro::InlineTask<int32_t, AllocatorAdapterNoExcept> {
        i++;
        co_return i;
    };
    bad_alloc(mock, std::move(task));
}

TEST_F(AllocatorAdapterTest, AllocatorAdapterTest_bad_alloc_multi)
{
    EXPECT_CALL(mock, allocate_bytes_noexcept).Times(1).WillRepeatedly([]([[maybe_unused]] size_t nbytes) { return nullptr; });
    EXPECT_CALL(mock, get_return_object_on_allocation_failure).Times(1).WillRepeatedly([] { throw std::bad_alloc{}; });

    auto task = [](int32_t i) -> tinycoro::Task<int32_t, AllocatorAdapterNoExcept> {
        i++;
        co_return i;
    };
    auto func = [&] { std::ignore = tinycoro::AllOfInline(task(0), task(1), task(2)); };

    EXPECT_THROW(func(), std::bad_alloc);
}

void second_bad_alloc_multi(auto& mock, auto task)
{
    EXPECT_CALL(mock, allocate_bytes_noexcept)
        .Times(2)
        .WillOnce([](size_t nbytes) { return std::malloc(nbytes); })
        .WillRepeatedly([]([[maybe_unused]] size_t nbytes) { return nullptr; });

    EXPECT_CALL(mock, deallocate_bytes).Times(1).WillOnce([](void* p, [[maybe_unused]] size_t nbytes) { return std::free(p); });

    EXPECT_CALL(mock, get_return_object_on_allocation_failure).Times(1).WillRepeatedly([] { throw std::bad_alloc{}; });

    auto func = [&] { std::ignore = tinycoro::AllOfInline(task(0), task(1), task(2)); };

    EXPECT_THROW(func(), std::bad_alloc);
}

TEST_F(AllocatorAdapterTest, AllocatorAdapterTest_second_bad_alloc_multi)
{
    auto task = [](int32_t i) -> tinycoro::Task<int32_t, AllocatorAdapterNoExcept> {
        i++;
        co_return i;
    };
    second_bad_alloc_multi(mock, std::move(task));
}

TEST_F(AllocatorAdapterTest, AllocatorAdapterTest_second_bad_alloc_multi_inline)
{
    auto task = [](int32_t i) -> tinycoro::InlineTask<int32_t, AllocatorAdapterNoExcept> {
        i++;
        co_return i;
    };
    second_bad_alloc_multi(mock, std::move(task));
}

void third_bad_alloc_multi_syncwait(auto& mock, auto task)
{
    EXPECT_CALL(mock, allocate_bytes_noexcept)
        .Times(3)
        .WillOnce([](size_t nbytes) { return std::malloc(nbytes); })
        .WillOnce([](size_t nbytes) { return std::malloc(nbytes); })
        .WillRepeatedly([]([[maybe_unused]] size_t nbytes) { return nullptr; });

    EXPECT_CALL(mock, deallocate_bytes)
        .Times(2)
        .WillOnce([](void* p, [[maybe_unused]] size_t nbytes) { return std::free(p); })
        .WillOnce([](void* p, [[maybe_unused]] size_t nbytes) { return std::free(p); });

    EXPECT_CALL(mock, get_return_object_on_allocation_failure).Times(1).WillRepeatedly([] { throw std::bad_alloc{}; });

    auto func = [&] { tinycoro::AllOfInline(task()); };
    EXPECT_THROW(func(), std::bad_alloc);
}

TEST_F(AllocatorAdapterTest, AllocatorAdapterTest_second_bad_alloc_multi_sync_await)
{
    tinycoro::Scheduler scheduler;

    auto task = []() -> tinycoro::Task<int32_t, AllocatorAdapterNoExcept> { co_return 42; };

    auto wrapperTask = [&]() -> tinycoro::Task<void, AllocatorAdapterNoExcept> {
        std::ignore = co_await tinycoro::AllOfAwait(scheduler, task(), task(), task());
    };

    third_bad_alloc_multi_syncwait(mock, wrapperTask);
}

TEST_F(AllocatorAdapterTest, AllocatorAdapterTest_second_bad_alloc_multi_sync_await_inline)
{
    tinycoro::Scheduler scheduler;

    auto task = []() -> tinycoro::Task<int32_t, AllocatorAdapterNoExcept> { co_return 42; };

    auto wrapperTask = [&]() -> tinycoro::InlineTask<void, AllocatorAdapterNoExcept> {
        std::ignore = co_await tinycoro::AllOfAwait(scheduler, task(), task(), task());
    };

    third_bad_alloc_multi_syncwait(mock, wrapperTask);
}

TEST_F(AllocatorAdapterTest, AllocatorAdapterTest_second_bad_alloc_multi_async)
{
    tinycoro::Scheduler scheduler;

    EXPECT_CALL(mock, allocate_bytes_noexcept)
        .Times(2)
        .WillOnce([](size_t nbytes) { return std::malloc(nbytes); })
        .WillRepeatedly([]([[maybe_unused]] size_t nbytes) { return nullptr; });

    EXPECT_CALL(mock, deallocate_bytes).Times(1).WillOnce([](void* p, [[maybe_unused]] size_t nbytes) { return std::free(p); });

    EXPECT_CALL(mock, get_return_object_on_allocation_failure).Times(1).WillRepeatedly([] { throw std::bad_alloc{}; });

    auto task = [](int32_t i) -> tinycoro::Task<int32_t, AllocatorAdapterNoExcept> {
        i++;
        co_return i;
    };
    auto func = [&] { std::ignore = tinycoro::AllOf(scheduler, task(0), task(1), task(2)); };

    EXPECT_THROW(func(), std::bad_alloc);
}

void throw_in_operator_new(auto& mock, auto task)
{
    EXPECT_CALL(mock, allocate_bytes)
        .Times(2)
        .WillOnce([](size_t nbytes) { return std::malloc(nbytes); })
        .WillRepeatedly([]([[maybe_unused]] size_t nbytes) {
            throw std::bad_alloc{};
            return nullptr;
        });

    EXPECT_CALL(mock, deallocate_bytes).Times(1).WillOnce([](void* p, [[maybe_unused]] size_t nbytes) { return std::free(p); });

    auto func = [&] { std::ignore = tinycoro::AllOfInline(task(0), task(1), task(2)); };

    EXPECT_THROW(func(), std::bad_alloc);
}

TEST_F(AllocatorAdapterTest, AllocatorAdapterTest_throw_in_operator_new)
{
    auto task = [](int32_t i) -> tinycoro::Task<int32_t, AllocatorAdapter> {
        i++;
        co_return i;
    };
    throw_in_operator_new(mock, task);
}

TEST_F(AllocatorAdapterTest, AllocatorAdapterTest_throw_in_operator_new_InlineTask)
{
    auto task = [](int32_t i) -> tinycoro::InlineTask<int32_t, AllocatorAdapter> {
        i++;
        co_return i;
    };
    throw_in_operator_new(mock, std::move(task));
}

TEST_F(AllocatorAdapterTest, AllocatorAdapterTest_throw_in_operator_new_async)
{
    tinycoro::Scheduler scheduler;

    EXPECT_CALL(mock, allocate_bytes)
        .Times(2)
        .WillOnce([](size_t nbytes) { return std::malloc(nbytes); })
        .WillRepeatedly([]([[maybe_unused]] size_t nbytes) {
            throw std::bad_alloc{};
            return nullptr;
        });

    EXPECT_CALL(mock, deallocate_bytes).Times(1).WillOnce([](void* p, [[maybe_unused]] size_t nbytes) { return std::free(p); });

    auto task = [](int32_t i) -> tinycoro::Task<int32_t, AllocatorAdapter> {
        i++;
        co_return i;
    };
    auto func = [&] { std::ignore = tinycoro::AllOf(scheduler, task(0), task(1), task(2)); };

    EXPECT_THROW(func(), std::bad_alloc);
}

TEST_F(AllocatorAdapterTest, AllocatorAdapterTest_sync_await_throw)
{
    tinycoro::Scheduler scheduler;

    EXPECT_CALL(mock, allocate_bytes)
        .Times(2)
        .WillOnce([](size_t nbytes) { return std::malloc(nbytes); })
        .WillRepeatedly([]([[maybe_unused]] size_t nbytes) {
            throw std::bad_alloc{};
            return nullptr;
        });

    EXPECT_CALL(mock, deallocate_bytes).Times(1).WillOnce([](void* p, [[maybe_unused]] size_t nbytes) { return std::free(p); });

    auto nestedTask = [](int32_t i) -> tinycoro::Task<int32_t, AllocatorAdapter> { co_return i; };

    auto task = [&](int32_t i) -> tinycoro::Task<int32_t, AllocatorAdapter> {
        // this need to throw std::bad_alloc
        std::ignore = co_await tinycoro::AllOfAwait(scheduler, nestedTask(0), nestedTask(1));
        co_return ++i;
    };
    auto func = [&] { std::ignore = tinycoro::AllOf(scheduler, task(0), task(1), task(2)); };

    EXPECT_THROW(func(), std::bad_alloc);
}

TEST_F(AllocatorAdapterTest, AllocatorAdapterTest_sync_await_throw_noexcept)
{
    tinycoro::Scheduler scheduler;

    EXPECT_CALL(mock, allocate_bytes_noexcept)
        .Times(2)
        .WillOnce([](size_t nbytes) { return std::malloc(nbytes); })
        .WillRepeatedly([]([[maybe_unused]] size_t nbytes) { return nullptr; });

    EXPECT_CALL(mock, deallocate_bytes).Times(1).WillOnce([](void* p, [[maybe_unused]] size_t nbytes) { return std::free(p); });

    // throw and exception
    EXPECT_CALL(mock, get_return_object_on_allocation_failure).Times(1).WillRepeatedly([] { throw std::bad_alloc{}; });

    auto nestedTask = [](int32_t i) -> tinycoro::Task<int32_t, AllocatorAdapterNoExcept> { co_return i; };

    auto task = [&](int32_t i) -> tinycoro::Task<int32_t, AllocatorAdapterNoExcept> {
        // this need to throw std::bad_alloc
        std::ignore = co_await tinycoro::AllOfAwait(scheduler, nestedTask(0), nestedTask(1));
        co_return ++i;
    };
    auto func = [&] { std::ignore = tinycoro::AllOf(scheduler, task(0), task(1), task(2)); };

    EXPECT_THROW(func(), std::bad_alloc);
}