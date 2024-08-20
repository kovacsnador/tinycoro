#ifndef __TINY_CORO_GENERATOR_HPP__
#define __TINY_CORO_GENERATOR_HPP__

#include <coroutine>
#include <type_traits>

#include "Common.hpp"

namespace tinycoro
{

    template <typename PromiseT>
    struct GeneratorIterator
    {
        struct Sentinel
        {
        };

        using CoroHandleType = std::coroutine_handle<PromiseT>;

        using iterator_category = std::input_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = PromiseT::value_type;
        using pointer = PromiseT::pointer;
        using reference = PromiseT::reference;

        GeneratorIterator(auto hdl)
            : _hdl{hdl}
        {
        }

        reference operator*() const { return *operator->(); }
        pointer operator->() const { return _hdl.promise().value(); }

        // Prefix increment
        GeneratorIterator &operator++()
        {
            if (_hdl && _hdl.done() == false)
            {
                _hdl.resume();
            }
            return *this;
        }

        // Need to provide post-increment operator to implement the 'Range' concept.
        void operator++(int)
        {
            (void)operator++();
        }

        friend bool operator==(const GeneratorIterator &lhs, const Sentinel &)
        {
            return lhs._hdl && lhs._hdl.done();
        }

    private:
        CoroHandleType _hdl;
    };

    template <typename ValueT, typename InitialAwaiterT, typename FinalAwaiterT>
    struct GeneratorPromise
    {
        using value_type = std::remove_cvref_t<ValueT>;
        using pointer = std::add_pointer_t<value_type>;
        using reference = std::add_lvalue_reference_t<value_type>;

        auto get_return_object()
        {
            return std::coroutine_handle<std::decay_t<decltype(*this)>>::from_promise(*this);
        }

        auto initial_suspend()
        {
            SyncOut() << "      GeneratorPromise: initial_suspend()\n";
            return InitialAwaiterT{};
        }

        auto final_suspend() noexcept
        {
            SyncOut() << "      GeneratorPromise: final_suspend()\n";
            return FinalAwaiterT{};
        }

        void unhandled_exception()
        {
            SyncOut() << "      GeneratorPromise: unhandled_exception()\n";
            std::rethrow_exception(std::current_exception());
        }

        void return_void()
        {
        }

        template <typename T>
        auto yield_value(T &&val)
        {
            _value = std::addressof(val);
            return std::suspend_always{};
        }

        auto value() const
        {
            return _value;
        }

    private:
        std::add_pointer_t<ValueT> _value;
    };

    template <typename PromiseT>
    struct GeneratorT
    {
        using promise_type = PromiseT;
        using CoroHandleType = std::coroutine_handle<PromiseT>;

        template <typename... Args>
            requires std::constructible_from<CoroHandleType, Args...>
        GeneratorT(Args &&...args)
            : _hdl{std::forward<Args>(args)...}
        {
            SyncOut() << "    Generator: constructor\n";
        }

        GeneratorT(GeneratorT &&other) noexcept
            : _hdl{std::exchange(other._hdl, nullptr)}
        {
            SyncOut() << "    Generator: move constructor\n";
        }

        GeneratorT &operator=(GeneratorT &&other) noexcept
        {
            SyncOut() << "    Generator: move assign\n";

            if (std::addressof(other) != this)
            {
                destroy();
                _hdl = std::exchange(other._hdl, nullptr);
            }
            return *this;
        }

        ~GeneratorT()
        {
            SyncOut() << "    Generator: ~destructor\n";
            destroy();
        }

        auto begin() const
        {
            return GeneratorIterator<PromiseT>{_hdl};
        }

        auto end() const
        {
            return typename GeneratorIterator<PromiseT>::Sentinel{};
        }

    private:
        void destroy()
        {
            if (_hdl)
            {
                SyncOut() << "    CoroTask: destroy()\n";

                _hdl.destroy();
                _hdl = nullptr;
            }
        }

    private:
        CoroHandleType _hdl;
    };

    template <typename YieldValueT>
    using Generator = GeneratorT<GeneratorPromise<YieldValueT, std::suspend_never, std::suspend_always>>;

}

#endif // !__TINY_CORO_GENERATOR_HPP__
