#include <gtest/gtest.h>

#include "tinycoro/StaticStorage.hpp"

struct Base
{
    int b{41};
};

struct Derived : Base
{
    int d{42};
};

TEST(StaticStorageTest, StaticStorage_defaultConstructor)
{
    tinycoro::detail::StaticStorage<Base, sizeof(Derived), Derived> storage{};
    
    EXPECT_FALSE(storage);
    EXPECT_TRUE(storage == nullptr);
}

TEST(StaticStorageTest, StaticStorage_Constructor)
{
    tinycoro::detail::StaticStorage<Base, sizeof(Derived), Derived> storage{std::type_identity<Derived>{}};
    
    EXPECT_TRUE(storage);
    EXPECT_TRUE(storage != nullptr);
}

TEST(StaticStorageTest, StaticStorage_MoveConstructor)
{
    tinycoro::detail::StaticStorage<Base, sizeof(Derived), Derived> storage{std::type_identity<Derived>{}};

    EXPECT_TRUE(storage);
    EXPECT_TRUE(storage != nullptr);

    EXPECT_EQ(storage->b, 41);

    storage->b = 2;

    EXPECT_EQ(storage->b, 2);

    decltype(storage) storage2{std::move(storage)};

    EXPECT_FALSE(storage);
    EXPECT_TRUE(storage == nullptr);

    EXPECT_THROW(storage.operator->(), tinycoro::StaticStorageException);

    EXPECT_TRUE(storage2);
    EXPECT_TRUE(storage2 != nullptr);

    EXPECT_EQ(storage2->b, 2);
}

TEST(StaticStorageTest, StaticStorage_MoveOperator)
{
    tinycoro::detail::StaticStorage<Base, sizeof(Derived), Derived> storage{std::type_identity<Derived>{}};

    EXPECT_TRUE(storage);
    EXPECT_TRUE(storage != nullptr);

    EXPECT_EQ(storage->b, 41);

    decltype(storage) storage2;
    EXPECT_FALSE(storage2);
    EXPECT_TRUE(storage2 == nullptr);
    
    storage2 = std::move(storage);

    EXPECT_FALSE(storage);
    EXPECT_TRUE(storage == nullptr);

    EXPECT_THROW(storage.operator->(), tinycoro::StaticStorageException);

    EXPECT_TRUE(storage2);
    EXPECT_TRUE(storage2 != nullptr);

    EXPECT_EQ(storage2->b, 41);
}

TEST(StaticStorageTest, StaticStorage_Exception)
{
    tinycoro::detail::StaticStorage<Base, sizeof(Derived), Derived> storage{};
    EXPECT_THROW(storage.operator->(), tinycoro::StaticStorageException);
}