// -----------------------------------------------------------------------------
//  Copyright (c) 2024 Tamas Kovacs
//  Licensed under the MIT License – see LICENSE.txt for details.
// -----------------------------------------------------------------------------
#ifndef TINY_CORO_FLIP_STACK_HPP
#define TINY_CORO_FLIP_STACK_HPP

#include <atomic>
#include <ranges>
#include <array>

#include "Common.hpp"
#include "BitLock.hpp"

namespace tinycoro { namespace detail {

    // MPSC stack
    template <concepts::Linkable NodeT>
    struct FlipStack
    {
        using value_type = NodeT;

        using deleter_t = void (*)(NodeT);

        FlipStack() = default;

        FlipStack(deleter_t deleter)
        : _deleter(deleter)
        {
        }

        ~FlipStack()
        {
            for (auto& it : _stacks)
                Cleanup(it.load(std::memory_order::relaxed), _deleter);
        }

        void Push(NodeT node) noexcept { PushImpl(node, _stacks[StackIndex()]); }

        void Push(NodeT node, int32_t hint) noexcept { PushImpl(node, _stacks[hint]); }

        // pop can be called only from one thread!
        // pop can use the currently active stack
        [[nodiscard]] std::tuple<NodeT, int32_t> Pop() noexcept
        {
            // Try to pop something from the stacks
            for (size_t i = 0; i < _stacks.size(); ++i)
            {
                auto index = StackIndex();
                // try to pull from the currectly active stack
                auto node = PopImpl(index);

                // return with the next element
                if (node)
                    return {node, !index};
            }

            // stop was requested
            return {nullptr, 0};
        }

        [[nodiscard]] std::tuple<NodeT, int32_t> PopWait(auto stopToken) noexcept
        {
            while (stopToken.stop_requested() == false)
            {
                // get the current traffic indicator
                auto traffic = _trafficFlag.load(std::memory_order::acquire);

                auto [node, hint] = Pop();

                if(node)
                    return {node, hint};

                // wait for arriving traffic
                _trafficFlag.wait(traffic);
            }

            // stop was requested
            return {nullptr, 0};
        }

        // Can only be pulled from the inactive stack
        std::tuple<NodeT, NodeT> TryPull() noexcept
        {
            return {TryPullStack(0), TryPullStack(1)};
        }

        void Notify() noexcept
        {
            _trafficFlag.fetch_add(1, std::memory_order::release);
            _trafficFlag.notify_one();
        }

        [[nodiscard]] bool Empty() const noexcept
        {
            bool empty{true};
            for (auto& it : _stacks)
                empty &= (it.load(std::memory_order::relaxed) == nullptr);
            return empty;
        }

    private:
        void PushImpl(NodeT node, auto& stack) noexcept
        {
            node->next = stack.load(std::memory_order::relaxed);
            while (stack.compare_exchange_weak(node->next, node, std::memory_order::release, std::memory_order::relaxed) == false)
                ;

            // we pushed new element in the stack
            Notify();
        }

        [[nodiscard]] NodeT PopImpl(auto index) noexcept
        {
            std::scoped_lock lock{_mtx};

            auto head = _stacks[index].load(std::memory_order::acquire);
            do
            {
                if (head == nullptr)
                    break;
            } while (_stacks[index].compare_exchange_weak(head, head->next, std::memory_order::release, std::memory_order::relaxed) == false);

            // check if this was the last elem in the stack
            // if yes, we flip the active stack
            if (head == nullptr || head->next == nullptr)
                FlipStackIndex();

            if (head)
                head->next = nullptr;

            return head;
        }

        static void Cleanup(NodeT head, deleter_t deleter) noexcept
        {
            while (deleter && head)
            {
                auto next = head->next;
                deleter(head);
                head = next;
            }
        }

        [[nodiscard]] auto TryPullStack(size_t index) noexcept
        {
            std::scoped_lock lock{_mtx};
            return _stacks[index].exchange(nullptr, std::memory_order::release);
        }

        // Gets the stack index,
        // by checking the first bit
        [[nodiscard]] size_t StackIndex() const noexcept
        {
            return 1ul & _stackIndex.load(std::memory_order::relaxed);
        }

        void FlipStackIndex() noexcept
        {
            _stackIndex.fetch_xor(1ul, std::memory_order::release);
        }

        std::array<std::atomic<NodeT>, 2> _stacks{nullptr, nullptr};

        // First bit contanis the stack index. 0 => _stack1, 1 => _stack2
        // 2. bit contains the lock flag. If it's set the resource lock
        // is active.
        std::atomic<uint32_t> _stackIndex{0};
        std::atomic<uint32_t> _trafficFlag{0};

        // the 2 bit locker mutex
        BitLock<uint32_t, 1> _mtx{_stackIndex};

        // custom deleter for stucked elements
        deleter_t _deleter{nullptr};
    };

}} // namespace tinycoro::detail

#endif // TINY_CORO_FLIP_STACK_HPP