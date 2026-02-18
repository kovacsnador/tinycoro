#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <tinycoro/SimpleStorage.hpp>

#include <tinycoro/tinycoro_all.h>

struct StorageObject
{
    std::string data;
    ~StorageObject() { Destructor(); }

    MOCK_METHOD(void, Destructor, ());
};

TEST(SimpleStorageTest, SimpleStorageTest_empty)
{
    tinycoro::detail::SimpleStorage<sizeof(StorageObject), alignof(StorageObject)> storage;

    EXPECT_TRUE(storage.Empty());
    EXPECT_FALSE(storage);
}

TEST(SimpleStorageTest, SimpleStorageTest_not_empty)
{
    tinycoro::detail::SimpleStorage<sizeof(StorageObject), alignof(StorageObject)> storage;

    storage.Emplace<StorageObject>("test");

    auto* obj = storage.UnsafeGet<StorageObject>();
    EXPECT_CALL(*obj, Destructor).Times(1);

    EXPECT_FALSE(storage.Empty());
    EXPECT_TRUE(storage);

    EXPECT_EQ(obj->data, std::string{"test"});
}

TEST(SimpleStorageTest, SimpleStorageTest_2times_initialize)
{
    tinycoro::detail::SimpleStorage<sizeof(StorageObject), alignof(StorageObject)> storage;

    storage.Emplace<StorageObject>("test");

    auto* obj = storage.UnsafeGet<StorageObject>();
    EXPECT_CALL(*obj, Destructor).Times(1);

    storage.Emplace<StorageObject>("test2");

    // get the new value
    obj = storage.UnsafeGet<StorageObject>();
    EXPECT_CALL(*obj, Destructor).Times(1);

    // still holds the first value
    EXPECT_EQ(obj->data, std::string{"test2"});
}

TEST(SimpleStorageTest, SimpleStorageTest_reassign_explicit_destroy)
{
    tinycoro::detail::SimpleStorage<sizeof(StorageObject), alignof(StorageObject)> storage;

    auto test = [&](std::string str) {
        storage.Emplace<StorageObject>(str);

        auto* obj = storage.UnsafeGet<StorageObject>();
        EXPECT_CALL(*obj, Destructor).Times(1);

        EXPECT_EQ(obj->data, str);

        // explicit destroy here
        storage.Destroy();
    };

    test("test1");
    test("test2");
    test("test3");
}

TEST(SimpleStorageTest, SimpleStorageTest_reassign)
{
    tinycoro::detail::SimpleStorage<sizeof(StorageObject), alignof(StorageObject)> storage;

    auto test = [&](std::string str) {
        storage.Emplace<StorageObject>(str);

        auto* obj = storage.UnsafeGet<StorageObject>();
        EXPECT_CALL(*obj, Destructor).Times(1);

        EXPECT_EQ(obj->data, str);
    };

    test("test1");
    test("test2");
    test("test3");
}

TEST(SimpleStorageTest, SimpleStorageTest_raw_data_compare)
{
    tinycoro::detail::SimpleStorage<sizeof(StorageObject), alignof(StorageObject)> storage;

    storage.Emplace<StorageObject>("str");

    auto* obj = storage.UnsafeGet<StorageObject>();
    EXPECT_CALL(*obj, Destructor).Times(1);

    EXPECT_EQ(obj->data, std::string{"str"});

    // expect to hold the same pointer value
    EXPECT_EQ(storage.RawData(), obj);
}