#ifndef __TINY_CORO_RUN_INLINE_HPP__
#define __TINY_CORO_RUN_INLINE_HPP__

#include "Common.hpp"

namespace tinycoro
{
    namespace concepts
    {
        template<typename T>
        concept LocalRunable = requires(T t)
        {
            {t.await_resume()};
            {t.GetPauseHandler()};
            {t.SetPauseHandler([]{})};
            {t.Resume()} -> std::same_as<void>;
            {t.ResumeState()} -> std::same_as<ETaskResumeState>;
        };
    }

    template<concepts::LocalRunable TaskT>
    [[nodiscard]] auto RunInline(TaskT&& task)
    {
        using enum ETaskResumeState;

        auto pauseResumerCallback = [&task] {
            auto pauseHandler = task.GetPauseHandler();
            pauseHandler->Resume();
        };

        auto pauseHandler = task.SetPauseHandler(pauseResumerCallback);

        ETaskResumeState state{DONE};

        do
        {
            // resume the corouitne
            task.Resume();
            // after resumption getting the state
            state = task.ResumeState();

            if(state == PAUSED)
            {
                // wait for resumption
                pauseHandler->AtomicWait(true);
            }

        } while (state != DONE && state != STOPPED);

        return task.await_resume();
    }

    template<typename... Args>
        requires (sizeof...(Args) > 1)
    [[nodiscard]] auto RunInline(Args&&... tasks)
    {
        auto returnValueConverter = []<typename T>(T&& task)
        {
            if constexpr (requires { {RunInline(std::forward<T>(task))} -> std::same_as<void>; })
            {
                RunInline(std::forward<T>(task));
                return VoidType{};
            }
            else
            {
                return RunInline(std::forward<T>(task));
            }
        };

        return std::tuple{returnValueConverter(std::forward<Args>(tasks))...};
    }

    template<concepts::Iterable ContainerT>
    [[nodiscard]] auto RunInline(ContainerT&& container)
    {
        using returnType = typename std::decay_t<ContainerT>::value_type::promise_type::value_type;
        if constexpr (std::same_as<void, returnType>)
        {
            for(auto& it : container)
            {
                RunInline(it);
            }
        }
        else
        {
            std::vector<returnType> results;
            for(auto& it : container)
            {
                results.push_back(RunInline(it));
            }

            return results;
        }
    } 
    
} // namespace tinycoro

#endif //!__TINY_CORO_RUN_INLINE_HPP__