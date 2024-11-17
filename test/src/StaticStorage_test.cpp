#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <string>

#include "tinycoro/StaticStorage.hpp"

struct Base
{
    Base(int bb, std::function<void()> f = []{}) 
    : b{bb}
    , func(f)
    {}

    virtual ~Base()
    {
        if(func)
        {
            func();
        }
    }

    int b{41};
    std::function<void()> func;
};

struct Derived : Base
{
    Derived(int bb, int dd, std::function<void()> f = []{}) 
    : Base{bb, f}
    , d{dd}
    , func{f}
    {}

    ~Derived()
    {
        if(func)
        {
            func();
        }
    }

    int d{42};
    std::function<void()> func;
};

TEST(StaticStorageTest, StaticStorage_defaultConstructor)
{
    tinycoro::detail::StaticStorage<Base, sizeof(Derived), Derived> storage{};

    EXPECT_FALSE(storage);
    EXPECT_TRUE(storage == nullptr);
}

TEST(StaticStorageTest, StaticStorage_Construct)
{
    tinycoro::detail::StaticStorage<Base, sizeof(Derived), Derived> storage{};

    storage.Construct<Derived>(41, 42);

    EXPECT_TRUE(storage);
    EXPECT_TRUE(storage != nullptr);
}

TEST(StaticStorageTest, StaticStorage_Destroy)
{
    tinycoro::detail::StaticStorage<Base, sizeof(Derived), Derived> storage{};

    size_t destructorCalled{};

    storage.Construct<Derived>(41, 42, [&destructorCalled] { destructorCalled++; });

    EXPECT_TRUE(storage);
    EXPECT_TRUE(storage != nullptr);

    storage.reset();

    EXPECT_EQ(destructorCalled, 2);
}

struct StorageTracer
{
    StorageTracer()
    {
        defaultConstructor++;
    }

    StorageTracer(std::string s)
    : str{std::move(s)}
    {
        constructor++;
    }
    
    StorageTracer(const StorageTracer& other)
    : str{other.str}
    {
        copyConstructor++;
    }
    
    StorageTracer(StorageTracer&& other) noexcept
    : str{std::move(other.str)}
    {
        moveConstructor++;
    }
    
    StorageTracer& operator=(const StorageTracer& other)
    {
        str = other.str;
        copyAssign++;
        return *this;
    }
    
    StorageTracer& operator=(StorageTracer&& other) noexcept
    {
        str = std::move(other.str);
        moveAssign++;
        return *this;
    }
    
    ~StorageTracer()
    {
        Destructor();
    }

    size_t defaultConstructor{};
    size_t constructor{};
    size_t copyConstructor{};
    size_t moveConstructor{};
    size_t copyAssign{};
    size_t moveAssign{};

    MOCK_METHOD(void, Destructor, ());

    std::string str{};
};

TEST(StaticStorageTest, StaticStorageTest_tracer_defaultConstructor)
{
    tinycoro::detail::StaticStorage<StorageTracer, sizeof(StorageTracer), StorageTracer> storage;

    // call default constructor
    auto* tracer = storage.Construct<StorageTracer>();
    
    EXPECT_EQ(tracer->defaultConstructor, 1);   // only default constructor is called once
    EXPECT_EQ(tracer->constructor, 0);
    EXPECT_EQ(tracer->copyConstructor, 0);
    EXPECT_EQ(tracer->copyAssign, 0);
    EXPECT_EQ(tracer->moveConstructor, 0);
    EXPECT_EQ(tracer->moveAssign, 0);

    EXPECT_TRUE(tracer->str == "");

    EXPECT_CALL(*storage.GetAs<StorageTracer>(), Destructor).Times(1);
}

TEST(StaticStorageTest, StaticStorageTest_tracer_constructor)
{
    tinycoro::detail::StaticStorage<StorageTracer, sizeof(StorageTracer), StorageTracer> storage;

    // call constructor
    auto* tracer = storage.Construct<StorageTracer>("testString");
    
    EXPECT_EQ(tracer->defaultConstructor, 0);
    EXPECT_EQ(tracer->constructor, 1);  // only constructor is called once
    EXPECT_EQ(tracer->copyConstructor, 0);
    EXPECT_EQ(tracer->copyAssign, 0);
    EXPECT_EQ(tracer->moveConstructor, 0);
    EXPECT_EQ(tracer->moveAssign, 0);

    EXPECT_TRUE(tracer->str == "testString");

    EXPECT_CALL(*storage.GetAs<StorageTracer>(), Destructor).Times(1);
}

TEST(StaticStorageTest, StaticStorageTest_tracer_copyConstructor)
{
    StorageTracer copy{"123"};
    EXPECT_CALL(copy, Destructor).Times(1);

    tinycoro::detail::StaticStorage<StorageTracer, sizeof(StorageTracer), StorageTracer> storage;

    // call copy constructor
    auto* tracer = storage.Construct<StorageTracer>(copy);
    
    EXPECT_EQ(tracer->defaultConstructor, 0);
    EXPECT_EQ(tracer->constructor, 0);
    EXPECT_EQ(tracer->copyConstructor, 1); // only copy constructor is called once
    EXPECT_EQ(tracer->copyAssign, 0);
    EXPECT_EQ(tracer->moveConstructor, 0);
    EXPECT_EQ(tracer->moveAssign, 0);

    EXPECT_TRUE(tracer->str == "123");

    EXPECT_CALL(*storage.GetAs<StorageTracer>(), Destructor).Times(1);
}

TEST(StaticStorageTest, StaticStorageTest_tracer_moveConstructor)
{
    StorageTracer other{"123"};
    EXPECT_CALL(other, Destructor).Times(1);

    tinycoro::detail::StaticStorage<StorageTracer, sizeof(StorageTracer), StorageTracer> storage;

    // call copy constructor
    auto* tracer = storage.Construct<StorageTracer>(std::move(other));
    
    EXPECT_EQ(tracer->defaultConstructor, 0);
    EXPECT_EQ(tracer->constructor, 0);
    EXPECT_EQ(tracer->copyConstructor, 0);
    EXPECT_EQ(tracer->copyAssign, 0);
    EXPECT_EQ(tracer->moveConstructor, 1);  // called
    EXPECT_EQ(tracer->moveAssign, 0);

    EXPECT_TRUE(tracer->str == "123");

    EXPECT_CALL(*storage.GetAs<StorageTracer>(), Destructor).Times(1);
}

TEST(StaticStorageTest, StaticStorageTest_tracer_copyAssign)
{
    StorageTracer other{"123"};
    EXPECT_CALL(other, Destructor).Times(1);

    tinycoro::detail::StaticStorage<StorageTracer, sizeof(StorageTracer), StorageTracer> storage;

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

    EXPECT_CALL(*storage.GetAs<StorageTracer>(), Destructor).Times(1);
}

TEST(StaticStorageTest, StaticStorageTest_tracer_moveAssign)
{
    StorageTracer other{"123"};
    EXPECT_CALL(other, Destructor).Times(1);

    tinycoro::detail::StaticStorage<StorageTracer, sizeof(StorageTracer), StorageTracer> storage;

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

    EXPECT_CALL(*storage.GetAs<StorageTracer>(), Destructor).Times(1);
}

TEST(StaticStorageTest, StaticStorageTest_tracer_all)
{
    StorageTracer other1{"123"};
    EXPECT_CALL(other1, Destructor).Times(1);

    StorageTracer other2{"321"};
    EXPECT_CALL(other2, Destructor).Times(1);

    StorageTracer other3{"4242"};
    EXPECT_CALL(other3, Destructor).Times(1);

    StorageTracer other4{"65535"};
    EXPECT_CALL(other4, Destructor).Times(1);

    tinycoro::detail::StaticStorage<StorageTracer, sizeof(StorageTracer), StorageTracer> storage;

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

    EXPECT_CALL(*storage.GetAs<StorageTracer>(), Destructor).Times(1);
}

TEST(StaticStorageTest, StaticStorageTest_tracer_constructException)
{
    StorageTracer other{"123"};
    EXPECT_CALL(other, Destructor).Times(1);

    tinycoro::detail::StaticStorage<StorageTracer, sizeof(StorageTracer), StorageTracer> storage;

    // call default constructor
    auto* tracer = storage.Construct<StorageTracer>();

    // calling construct 2. time
    EXPECT_THROW(storage.Construct<StorageTracer>(other), tinycoro::StaticStorageException);
    
    EXPECT_EQ(tracer->defaultConstructor, 1);   // called
    EXPECT_EQ(tracer->constructor, 0);
    EXPECT_EQ(tracer->copyConstructor, 0);
    EXPECT_EQ(tracer->copyAssign, 0);
    EXPECT_EQ(tracer->moveConstructor, 0);
    EXPECT_EQ(tracer->moveAssign, 0);

    EXPECT_CALL(*storage.GetAs<StorageTracer>(), Destructor).Times(1);
}

TEST(StaticStorageTest, StaticStorageTest_tracer_assignException)
{
    StorageTracer other{"123"};
    EXPECT_CALL(other, Destructor).Times(1);

    tinycoro::detail::StaticStorage<StorageTracer, sizeof(StorageTracer), StorageTracer> storage;

    // calling assign on a uninitialized object
    EXPECT_THROW(storage.Assign<StorageTracer>(other), tinycoro::StaticStorageException);
}