#include <gtest/gtest.h>

#include <memory>
#include <stop_token>
#include <thread>
#include <latch>

#include "tinycoro/FlipStack.hpp"

template <typename T>
struct FlipStackNode
{
    FlipStackNode(T v)
    : value{v}
    {
    }

    T              value{};
    FlipStackNode* next{nullptr};
};

struct FlipStackTest : testing::Test
{
    void SetUp() override { }
    void TearDown() override { }

    tinycoro::detail::FlipStack<FlipStackNode<int32_t>*> stack;

    FlipStackNode<int32_t> n1{1};
    FlipStackNode<int32_t> n2{2};
    FlipStackNode<int32_t> n3{3};
    FlipStackNode<int32_t> n4{4};
    FlipStackNode<int32_t> n5{5};
    FlipStackNode<int32_t> n6{6};
    FlipStackNode<int32_t> n7{7};
    FlipStackNode<int32_t> n8{8};
};

TEST_F(FlipStackTest, FlipStackTest_push_pop)
{
    stack.Push(&n1);
    stack.Push(&n2);
    stack.Push(&n3);
    stack.Push(&n4);
    stack.Push(&n5);
    stack.Push(&n6);
    stack.Push(&n7);
    stack.Push(&n8);

    for (size_t i = 0; i < 8; ++i)
    {
        auto [node, index] = stack.PopWait(std::stop_token{});
        EXPECT_EQ(node->value, 8 - i);
    }
}

TEST_F(FlipStackTest, FlipStackTest_empty)
{
    EXPECT_TRUE(stack.Empty());

    stack.Push(&n1);
    EXPECT_FALSE(stack.Empty());

    stack.Push(&n2);
    stack.Push(&n3);
    stack.Push(&n4);
    stack.Push(&n5);
    stack.Push(&n6);
    stack.Push(&n7);
    stack.Push(&n8);

    for (size_t i = 0; i < 8; ++i)
    {
        auto [node, index] = stack.PopWait(std::stop_token{});
        EXPECT_EQ(node->value, 8 - i);
    }

    EXPECT_TRUE(stack.Empty());
}

TEST_F(FlipStackTest, FlipStackTest_push_pop_2x)
{
    stack.Push(&n1);
    stack.Push(&n2);
    stack.Push(&n3);
    stack.Push(&n4);
    stack.Push(&n5);
    stack.Push(&n6);
    stack.Push(&n7);
    stack.Push(&n8);

    // [8... 0]
    for (size_t i = 0; i < 8; ++i)
    {
        auto [node, index] = stack.PopWait(std::stop_token{});
        EXPECT_EQ(node->value, 8 - i);

        stack.Push(node, index);
    }

    // flipping the order [0... 8]
    for (size_t i = 1; i < 8; ++i)
    {
        auto [node, index] = stack.PopWait(std::stop_token{});
        EXPECT_EQ(node->value, i);
    }
}

TEST_F(FlipStackTest, FlipStackTest_push_pop_single_value)
{
    stack.Push(&n1);

    {
        auto [node, hint] = stack.PopWait(std::stop_token{});
        EXPECT_EQ(node->value, 1);
        EXPECT_EQ(node->next, nullptr);
        EXPECT_EQ(hint, 1);

        stack.Push(node, hint);
    }

    {
        auto [node, hint] = stack.PopWait(std::stop_token{});
        EXPECT_EQ(node->value, 1);
        EXPECT_EQ(node->next, nullptr);
        EXPECT_EQ(hint, 0); // hint is flipped

        stack.Push(node, hint);
    }

    {
        auto [node, hint] = stack.PopWait(std::stop_token{});
        EXPECT_EQ(node->value, 1);
        EXPECT_EQ(node->next, nullptr);
        EXPECT_EQ(hint, 1); // hint is flipped
    }
}

TEST_F(FlipStackTest, FlipStackTest_try_pull)
{
    stack.Push(&n1);
    stack.Push(&n2);
    stack.Push(&n3);
    stack.Push(&n4);
    stack.Push(&n5);
    stack.Push(&n6);
    stack.Push(&n7);
    stack.Push(&n8);

    auto [head1, head2] = stack.TryPull();
    EXPECT_EQ(head2, nullptr);

    for(int32_t i = 8; i > 0; --i)
    {
        EXPECT_EQ(head1->value, i);
        head1 = head1->next;
    }

    EXPECT_TRUE(stack.Empty());
}

TEST_F(FlipStackTest, FlipStackTest_deleter)
{
    static size_t deleterCount{};

    auto deleter = [](auto ptr) {
        delete ptr;
        deleterCount++;
    };

    {
        tinycoro::detail::FlipStack<FlipStackNode<int32_t>*> stack2{deleter};

        for (auto it : std::ranges::iota_view(1, 6))
        {
            stack2.Push(new FlipStackNode<int32_t>(it));
        }

        // [5... 1]
        for (size_t i = 5; i > 0; --i)
        {
            auto [node, index] = stack2.PopWait(std::stop_token{});
            EXPECT_EQ(node->value, i);
            EXPECT_EQ(node->next, nullptr);

            stack2.Push(node, index);
        }
    }

    EXPECT_EQ(deleterCount, 5);
}

struct FlipStackDeleterTest : testing::TestWithParam<size_t>
{
    inline static size_t deleteCount{};

    static void Deleter(auto ptr)
    {
        delete ptr;
        deleteCount++;
    }
};

INSTANTIATE_TEST_SUITE_P(FlipStackDeleterTest, FlipStackDeleterTest, testing::Values(1, 10, 100, 1000, 10000));

void PushBack(auto& stack, auto head)
{
    while (head)
    {
        auto next = head->next;
        stack->Push(head);
        head = next;
    }
}

void Delete(auto head)
{
    while (head)
    {
        auto next = head->next;
        delete head;
        head = next;
    }
}

TEST_P(FlipStackDeleterTest, FlipStackTest_multi_threaded)
{
    deleteCount = 0;

    std::atomic<size_t> count{GetParam()};

    auto stack = std::make_unique<tinycoro::detail::FlipStack<FlipStackNode<int32_t>*>>([](auto p) { Deleter(p); });

    auto producer = [&] {
        for (size_t i = 0; i < count; ++i)
        {
            stack->Push(new FlipStackNode<int32_t>(i));
        }
    };

    auto consumer = [&] {
        for (size_t i = 0; i < GetParam(); ++i)
        {
            auto [node, hint] = stack->PopWait(std::stop_token{});
            stack->Push(node, hint);
        }

        for (size_t i = 0; i < GetParam(); ++i)
        {
            auto [node, hint] = stack->PopWait(std::stop_token{});
            stack->Push(node, hint);
        }
    };

    std::vector<std::thread> threads;

    threads.emplace_back(consumer);

    for (size_t i = 0; i < 8; ++i)
    {
        threads.emplace_back(producer);
    }

    for (auto& it : threads)
    {
        it.join();
    }

    // count the elements
    size_t totalCount{};
    decltype(stack)::element_type::value_type head{nullptr};
    while(stack->Empty() == false)
    {
        auto [elem, hint] = stack->Pop();
        elem->next = head;
        head = elem;
        totalCount++;
    }

    EXPECT_EQ(totalCount, GetParam() * (threads.size() - 1));

    PushBack(stack, head);

    stack.reset();

    EXPECT_EQ(deleteCount, GetParam() * (threads.size() - 1));
}

TEST_P(FlipStackDeleterTest, FlipStackTest_multi_threaded_trypull)
{
    deleteCount = 0;

    std::atomic<size_t> count{GetParam()};

    auto stack = std::make_unique<tinycoro::detail::FlipStack<FlipStackNode<int32_t>*>>([](auto p) { Deleter(p); });

    auto producer = [&] {
        for (size_t i = 0; i < count; ++i)
        {
            stack->Push(new FlipStackNode<int32_t>(i));
        }

        for (size_t i = 0; i < 10; ++i)
        {
            auto [head1, head2] = stack->TryPull();

            PushBack(stack, head1);
            PushBack(stack, head2);
        }
    };

    auto consumer = [&] {
        for (size_t i = 0; i < GetParam(); ++i)
        {
            auto [node, hint] = stack->PopWait(std::stop_token{});
            stack->Push(node, hint);
        }

        for (size_t i = 0; i < GetParam(); ++i)
        {
            auto [node, hint] = stack->PopWait(std::stop_token{});
            stack->Push(node, hint);
        }
    };

    std::vector<std::thread> threads;

    threads.emplace_back(consumer);

    for (size_t i = 0; i < 8; ++i)
    {
        threads.emplace_back(producer);
    }

    for (auto& it : threads)
    {
        it.join();
    }

    // count the elements
    size_t totalCount{};
    decltype(stack)::element_type::value_type head{nullptr};
    while(stack->Empty() == false)
    {
        auto [elem, hint] = stack->Pop();
        elem->next = head;
        head = elem;
        totalCount++;
    }

    EXPECT_EQ(totalCount, GetParam() * (threads.size() - 1));

    PushBack(stack, head);

    stack.reset();

    EXPECT_EQ(deleteCount, GetParam() * (threads.size() - 1));
}

TEST_P(FlipStackDeleterTest, FlipStackTest_multi_threaded_trypull_delete)
{
    deleteCount = 0;

    std::atomic<size_t> count{GetParam()};

    std::stop_source ss;

    auto stack = std::make_unique<tinycoro::detail::FlipStack<FlipStackNode<int32_t>*>>([](auto p) { Deleter(p); });

    std::latch latch{8};

    auto producer = [&] {
        for (size_t i = 0; i < count; ++i)
        {
            stack->Push(new FlipStackNode<int32_t>(i));
        }

        for (size_t i = 0; i < count; ++i)
        {
            auto [head1, head2] = stack->TryPull();

            Delete(head1);
            Delete(head2);
        }

        latch.arrive_and_wait();

        ss.request_stop();
        stack->Notify();
    };

    auto consumer = [&] {
        while (ss.stop_requested() == false)
        {
            auto [node, hint] = stack->PopWait(ss.get_token());
            if(node)
                stack->Push(node, hint);
        }
    };

    std::vector<std::thread> threads;

    threads.emplace_back(consumer);

    for (size_t i = 0; i < 8; ++i)
    {
        threads.emplace_back(producer);
    }

    for (auto& it : threads)
    {
        it.join();
    }

    stack.reset();

    SUCCEED();
}

TEST(FlipStackTests, MultipleProducersSingleConsumer_AllItemsReceived)
{
    tinycoro::detail::FlipStack<FlipStackNode<int32_t>*> stack;

    constexpr int producers = 4;
    constexpr int each = 1000;
    constexpr int total = producers * each;

    std::atomic<int> pushed{0};

    // start producer threads
    std::vector<std::jthread> threads;
    for (int p = 0; p < producers; ++p) {
        threads.emplace_back([&stack, &pushed, p] (std::stop_token) {
            // each producer pushes 'each' items with unique value range
            int base = p * each;
            for (int i = 0; i < each; ++i) {
                auto n = new FlipStackNode<int32_t>{base + i};
                stack.Push(n);
                ++pushed;
            }
        });
    }

    // consumer: use PopWait so it sleeps while there is no traffic
    std::stop_source stopSrc;
    std::vector<char> seen(total, 0);
    int received = 0;

    while (received < total) {
        auto [node, hint] = stack.PopWait(stopSrc.get_token());
        if (node) {
            int v = node->value;
            ASSERT_GE(v, 0);
            ASSERT_LT(v, total);
            // mark seen (no synchronization needed for single consumer)
            seen[v] = 1;
            ++received;
            delete node;
        }
        // else node == nullptr indicates stop requested; not expected here
    }

    // request stop to make sure any waiting calls unblock if any
    stopSrc.request_stop();

    // join producers (jthread will auto-join on destruction, but be explicit)
    threads.clear();

    // check we received all unique items
    int countSeen = 0;
    for (auto b : seen) if (b) ++countSeen;
    EXPECT_EQ(countSeen, total);
    EXPECT_TRUE(stack.Empty());
}

TEST(FlipStackTests, MultipleProducersSingleConsumer_PopWaitStopToken)
{
    tinycoro::detail::FlipStack<FlipStackNode<int32_t>*> stack;

    std::stop_source ss;

    bool stopped{false};

    std::thread consumer{[&] {
        std::ignore = stack.PopWait(ss.get_token());
        stopped = true;
    }};

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    ss.request_stop();

    EXPECT_FALSE(stopped);

    stack.Notify();

    consumer.join();

    EXPECT_TRUE(stopped);
    SUCCEED();
}