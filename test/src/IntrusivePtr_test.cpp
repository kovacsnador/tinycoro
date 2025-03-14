#include <gtest/gtest.h>

#include "tinycoro/IntrusiveObject.hpp"

#include "tinycoro/tinycoro_all.h"

#include "mock/StorageTracer.h"

struct TestStruct : tinycoro::detail::IntrusiveObject<TestStruct>, tinycoro::test::StorageTracer
{
    TestStruct() = default;

    TestStruct(std::string str)
    : tinycoro::test::StorageTracer{std::move(str)}
    {
    }
};

TEST(IntrusivePtrTest, IntrusivePtrTest_default_constructor)
{
    using type = tinycoro::detail::IntrusivePtr<TestStruct>;

    type obj1;
    obj1.emplace();

    auto tracer = obj1.get();

    EXPECT_EQ(tracer->RefCount(), 1);

    EXPECT_EQ(tracer->defaultConstructor, 1);
    EXPECT_EQ(tracer->constructor, 0);
    EXPECT_EQ(tracer->copyConstructor, 0);
    EXPECT_EQ(tracer->copyAssign, 0);
    EXPECT_EQ(tracer->moveConstructor, 0);
    EXPECT_EQ(tracer->moveAssign, 0);
    EXPECT_CALL(*tracer, Destructor).Times(1);
}

TEST(IntrusivePtrTest, IntrusivePtrTest_constructor)
{
    using type = tinycoro::detail::IntrusivePtr<TestStruct>;

    type obj1;
    obj1.emplace("123");

    auto tracer = obj1.get();

    EXPECT_EQ(tracer->RefCount(), 1);

    EXPECT_EQ(tracer->defaultConstructor, 0);
    EXPECT_EQ(tracer->constructor, 1);
    EXPECT_EQ(tracer->copyConstructor, 0);
    EXPECT_EQ(tracer->copyAssign, 0);
    EXPECT_EQ(tracer->moveConstructor, 0);
    EXPECT_EQ(tracer->moveAssign, 0);

    EXPECT_CALL(*tracer, Destructor).Times(1);
}

TEST(IntrusivePtrTest, IntrusivePtrTest_copy)
{
    using type = tinycoro::detail::IntrusivePtr<TestStruct>;

    type obj1;
    obj1.emplace("123");

    auto tracer = obj1.get();

    EXPECT_CALL(*tracer, Destructor).Times(1);

    type obj2;
    obj2 = obj1;

    type obj3{obj2};
    type obj4 = obj1;
    type obj5 = obj4;

    EXPECT_EQ(tracer->RefCount(), 5);

    EXPECT_EQ(tracer->defaultConstructor, 0);
    EXPECT_EQ(tracer->constructor, 1);
    EXPECT_EQ(tracer->copyConstructor, 0);
    EXPECT_EQ(tracer->copyAssign, 0);
    EXPECT_EQ(tracer->moveConstructor, 0);
    EXPECT_EQ(tracer->moveAssign, 0);
}

TEST(IntrusivePtrTest, IntrusivePtrTest_refCount)
{
    using type = tinycoro::detail::IntrusivePtr<TestStruct>;

    type obj1;
    obj1.emplace("123");

    auto tracer = obj1.get();

    EXPECT_CALL(*tracer, Destructor).Times(1);

    {
        type obj2;
        obj2 = obj1;

        EXPECT_EQ(tracer->RefCount(), 2);
    }

    EXPECT_EQ(tracer->RefCount(), 1);

    type obj3{obj1};

    EXPECT_EQ(tracer->RefCount(), 2);

    {
        type obj4 = obj1;
        type obj5 = obj4;

        EXPECT_EQ(tracer->RefCount(), 4);
    }

    EXPECT_EQ(tracer->RefCount(), 2);

    EXPECT_EQ(tracer->defaultConstructor, 0);
    EXPECT_EQ(tracer->constructor, 1);
    EXPECT_EQ(tracer->copyConstructor, 0);
    EXPECT_EQ(tracer->copyAssign, 0);
    EXPECT_EQ(tracer->moveConstructor, 0);
    EXPECT_EQ(tracer->moveAssign, 0);
}

TEST(IntrusivePtrTest, IntrusivePtrTest_emplace_exception)
{
    using type = tinycoro::detail::IntrusivePtr<TestStruct>;

    type obj{};

    EXPECT_NO_THROW(obj.emplace());
    EXPECT_CALL(*obj.get(), Destructor).Times(1);
    EXPECT_THROW(obj.emplace(), tinycoro::UnsafeSharedObjectException);
}

TEST(IntrusivePtrTest, IntrusivePtrTest_assign_exception)
{
    using type = tinycoro::detail::IntrusivePtr<TestStruct>;

    type obj{};

    EXPECT_NO_THROW(obj.emplace());
    EXPECT_CALL(*obj.get(), Destructor).Times(1);
    
    type obj2;

    obj2.emplace("123");

    EXPECT_CALL(*obj2.get(), Destructor).Times(1);
    
    EXPECT_THROW((obj = obj2), tinycoro::UnsafeSharedObjectException);
}

TEST(IntrusivePtrTest, IntrusivePtrTest_const)
{
    using type = tinycoro::detail::IntrusivePtr<TestStruct>;

    type obj{};

    EXPECT_NO_THROW(obj.emplace("123"));
    EXPECT_CALL(*obj.get(), Destructor).Times(1);
    
    const type& c_obj = obj;

    EXPECT_EQ(c_obj.get()->str, std::string{"123"});
    EXPECT_EQ(c_obj->str, std::string{"123"});
}

TEST(IntrusivePtrTest, IntrusivePtrTest_async)
{
    tinycoro::Scheduler scheduler;

    tinycoro::ManualEvent objInitialized;
    tinycoro::AutoEvent   allRefsReleased;

    using type = tinycoro::detail::IntrusivePtr<TestStruct>;

    auto obj1 = std::make_unique<type>();

    tinycoro::Barrier waitForSetRefs{4, [&] {
                                         // release the ref count for obj1
                                         obj1.reset();
                                     }}; // 4 tasks

    auto task = [&]() -> tinycoro::Task<void> {
        {
            type obj;
            obj.emplace("123");

            EXPECT_CALL(*obj.get(), Destructor).Times(1);

            *obj1 = obj;

            // obj1 is set
            objInitialized.Set();

            // here need to wait until
            // all refs are released...
        }

        EXPECT_TRUE(allRefsReleased.IsSet());

        co_await allRefsReleased;
    };

    auto task2 = [&]() -> tinycoro::Task<void> {
        type obj;

        // wait for obj1 to assigned
        co_await objInitialized;

        // save the object ptr
        obj = *obj1;

        EXPECT_EQ(obj->str, std::string{"123"});

        // wait for all the tasks to save obj1
        co_await waitForSetRefs.ArriveAndWait();

        // trigger again the event
        allRefsReleased.Set();

        // all refs are released...
    };

    tinycoro::GetAll(scheduler, task(), task2(), task2(), task2(), task2());
}