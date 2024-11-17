#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <tinycoro/DynamicStorage.hpp>

#include "mock/StorageTracer.h"

using namespace tinycoro::test;

TEST(DynamicStorageTest, DynamicStorageTest_tracer_defaultConstructor)
{
    tinycoro::detail::DynamicStorage<StorageTracer> storage;

    // call default constructor
    auto* tracer = storage.Construct<StorageTracer>();
    
    EXPECT_EQ(tracer->defaultConstructor, 1);   // only default constructor is called once
    EXPECT_EQ(tracer->constructor, 0);
    EXPECT_EQ(tracer->copyConstructor, 0);
    EXPECT_EQ(tracer->copyAssign, 0);
    EXPECT_EQ(tracer->moveConstructor, 0);
    EXPECT_EQ(tracer->moveAssign, 0);

    EXPECT_TRUE(tracer->str == "");

    EXPECT_CALL(*tracer, Destructor).Times(1);
}

TEST(DynamicStorageTest, DynamicStorageTest_tracer_constructor)
{
    tinycoro::detail::DynamicStorage<StorageTracer> storage;

    // call constructor
    auto* tracer = storage.Construct<StorageTracer>("testString");
    
    EXPECT_EQ(tracer->defaultConstructor, 0);
    EXPECT_EQ(tracer->constructor, 1);  // only constructor is called once
    EXPECT_EQ(tracer->copyConstructor, 0);
    EXPECT_EQ(tracer->copyAssign, 0);
    EXPECT_EQ(tracer->moveConstructor, 0);
    EXPECT_EQ(tracer->moveAssign, 0);

    EXPECT_TRUE(tracer->str == "testString");

    EXPECT_CALL(*tracer, Destructor).Times(1);
}

TEST(DynamicStorageTest, DynamicStorageTest_tracer_copyConstructor)
{
    StorageTracer copy{"123"};
    EXPECT_CALL(copy, Destructor).Times(1);

    tinycoro::detail::DynamicStorage<StorageTracer> storage;

    // call copy constructor
    auto* tracer = storage.Construct<StorageTracer>(copy);
    
    EXPECT_EQ(tracer->defaultConstructor, 0);
    EXPECT_EQ(tracer->constructor, 0);
    EXPECT_EQ(tracer->copyConstructor, 1); // only copy constructor is called once
    EXPECT_EQ(tracer->copyAssign, 0);
    EXPECT_EQ(tracer->moveConstructor, 0);
    EXPECT_EQ(tracer->moveAssign, 0);

    EXPECT_TRUE(tracer->str == "123");

    EXPECT_CALL(*tracer, Destructor).Times(1);
}

TEST(DynamicStorageTest, DynamicStorageTest_tracer_moveConstructor)
{
    StorageTracer other{"123"};
    EXPECT_CALL(other, Destructor).Times(1);

    tinycoro::detail::DynamicStorage<StorageTracer> storage;

    // call copy constructor
    auto* tracer = storage.Construct<StorageTracer>(std::move(other));
    
    EXPECT_EQ(tracer->defaultConstructor, 0);
    EXPECT_EQ(tracer->constructor, 0);
    EXPECT_EQ(tracer->copyConstructor, 0);
    EXPECT_EQ(tracer->copyAssign, 0);
    EXPECT_EQ(tracer->moveConstructor, 1);  // called
    EXPECT_EQ(tracer->moveAssign, 0);

    EXPECT_TRUE(tracer->str == "123");

    EXPECT_CALL(*tracer, Destructor).Times(1);
}

TEST(DynamicStorageTest, DynamicStorageTest_tracer_copyAssign)
{
    StorageTracer other{"123"};
    EXPECT_CALL(other, Destructor).Times(1);

    tinycoro::detail::DynamicStorage<StorageTracer> storage;

    // call default constructor
    auto* tracer = storage.Construct<StorageTracer>();

    storage.Assign<StorageTracer>(other);
    
    EXPECT_EQ(tracer->defaultConstructor, 1);   // called
    EXPECT_EQ(tracer->constructor, 0);
    EXPECT_EQ(tracer->copyConstructor, 0);
    EXPECT_EQ(tracer->copyAssign, 1);           // called
    EXPECT_EQ(tracer->moveConstructor, 0);
    EXPECT_EQ(tracer->moveAssign, 0);

    EXPECT_TRUE(tracer->str == "123");

    EXPECT_CALL(*tracer, Destructor).Times(1);
}

TEST(DynamicStorageTest, DynamicStorageTest_tracer_moveAssign)
{
    StorageTracer other{"123"};
    EXPECT_CALL(other, Destructor).Times(1);

    tinycoro::detail::DynamicStorage<StorageTracer> storage;

    // call default constructor
    auto* tracer = storage.Construct<StorageTracer>();

    storage.Assign<StorageTracer>(std::move(other));
    
    EXPECT_EQ(tracer->defaultConstructor, 1);   // called
    EXPECT_EQ(tracer->constructor, 0);
    EXPECT_EQ(tracer->copyConstructor, 0);
    EXPECT_EQ(tracer->copyAssign, 0);           
    EXPECT_EQ(tracer->moveConstructor, 0);
    EXPECT_EQ(tracer->moveAssign, 1);           // called

    EXPECT_TRUE(tracer->str == "123");

    EXPECT_CALL(*tracer, Destructor).Times(1);
}

TEST(DynamicStorageTest, DynamicStorageTest_tracer_all)
{
    StorageTracer other1{"123"};
    EXPECT_CALL(other1, Destructor).Times(1);

    StorageTracer other2{"321"};
    EXPECT_CALL(other2, Destructor).Times(1);

    StorageTracer other3{"4242"};
    EXPECT_CALL(other3, Destructor).Times(1);

    StorageTracer other4{"65535"};
    EXPECT_CALL(other4, Destructor).Times(1);

    tinycoro::detail::DynamicStorage<StorageTracer> storage;

    auto tracer = storage.Construct<StorageTracer>(std::move(other1));
    EXPECT_TRUE(tracer->str == "123");

    storage.Assign<StorageTracer>(std::move(other2));
    EXPECT_TRUE(tracer->str == "321");

    storage.Assign<StorageTracer>(other3);
    EXPECT_TRUE(tracer->str == other3.str);

    storage.Assign<StorageTracer>(other4);
    EXPECT_TRUE(tracer->str == other4.str);
    
    EXPECT_EQ(tracer->defaultConstructor, 0);   // called
    EXPECT_EQ(tracer->constructor, 0);
    EXPECT_EQ(tracer->copyConstructor, 0);
    EXPECT_EQ(tracer->copyAssign, 2);   // called           
    EXPECT_EQ(tracer->moveConstructor, 1);  // called
    EXPECT_EQ(tracer->moveAssign, 1);           // called

    EXPECT_CALL(*tracer, Destructor).Times(1);
}

TEST(DynamicStorageTest, DynamicStorageTest_tracer_constructException)
{
    StorageTracer other{"123"};
    EXPECT_CALL(other, Destructor).Times(1);

    tinycoro::detail::DynamicStorage<StorageTracer> storage;

    // call default constructor
    auto* tracer = storage.Construct<StorageTracer>();

    // calling construct 2. time
    EXPECT_THROW(storage.Construct<StorageTracer>(other), tinycoro::DynamicStorageException);
    
    EXPECT_EQ(tracer->defaultConstructor, 1);   // called
    EXPECT_EQ(tracer->constructor, 0);
    EXPECT_EQ(tracer->copyConstructor, 0);
    EXPECT_EQ(tracer->copyAssign, 0);
    EXPECT_EQ(tracer->moveConstructor, 0);
    EXPECT_EQ(tracer->moveAssign, 0);

    EXPECT_CALL(*tracer, Destructor).Times(1);
}

TEST(DynamicStorageTest, DynamicStorageTest_tracer_assignException)
{
    StorageTracer other{"123"};
    EXPECT_CALL(other, Destructor).Times(1);

    tinycoro::detail::DynamicStorage<StorageTracer> storage;

    // calling assign on a uninitialized object
    EXPECT_THROW(storage.Assign<StorageTracer>(other), tinycoro::DynamicStorageException);
}