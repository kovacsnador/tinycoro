#include "AggregateValue_example.h"

#include "AnyOf_example.h"
#include "AnyOfDynamic_example.h"
#include "AnyOfDynamicVoid_example.h"
#include "AnyOfVoid_example.h"
#include "AnyOfVoidException_example.h"

#include "AsyncCallbackAwaiterCStyleVoid_example.h"
#include "AsyncCallbackAwaiterCStyle_example.h"
#include "AsyncCallbackAwaiterReturnValue.h"
#include "AsyncCallbackAwaiter_example.h"
#include "AsyncPulling_example.h"

#include "Exception_example.h"
#include "Generator_example.h"
#include "MoveOnlyValue_example.h"

#include "MultiMovedDynamicTasks_example.h"
#include "MultiMovedTasksDynamicVoid_example.h"

#include "MultiTaskDifferentValues_example.h"
#include "MultiTasks_example.h"
#include "MultiTasksDynamic_example.h"
#include "NestedException_example.h"
#include "NestedTask_example.h"
#include "ReturnValueTask_example.h"
#include "Sleep_example.h"
#include "TaskView_example.h"
#include "UsageWithStopToken_example.h"
#include "VoidTask_example.h"

#include "CustomAwaiter.h"
#include "tinycoro/SyncAwait.hpp"

tinycoro::Task<std::string> Example_asyncAwaiter(auto& scheduler)
{
    auto task1 = []() -> tinycoro::Task<std::string> { co_return "123"; };
    auto task2 = []() -> tinycoro::Task<std::string> { co_return "456"; };
    auto task3 = []() -> tinycoro::Task<std::string> { co_return "789"; };

    auto tupleResult = co_await tinycoro::SyncAwait(scheduler, task1(), task2(), task3());

    // tuple accumulate
    co_return std::apply(
        []<typename... Ts>(Ts&&... ts) {
            std::string result;
            (result.append(std::forward<Ts>(ts)), ...);
            return result;
        },
        tupleResult);
}

tinycoro::Task<void> Example_asyncAwaiter2(auto& scheduler)
{
    auto task1 = []() -> tinycoro::Task<void> { co_return; };
    auto task2 = []() -> tinycoro::Task<void> { co_return; };
    auto task3 = []() -> tinycoro::Task<void> { co_return; };

    co_await tinycoro::SyncAwait(scheduler, task1(), task2(), task3());
}

tinycoro::Task<void> Example_AnyOfCoAwait(auto& scheduler)
{
    SyncOut() << "\n\nExample_AnyOf:\n";

    auto task1 = [](auto duration) -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        int32_t count{0};

        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend{++count};
        }
        co_return count;
    };

    auto results = co_await tinycoro::AnyOfAwait(scheduler, task1(1s), task1(2s), task1(3s));

    auto t1 = std::get<0>(results);
    auto t2 = std::get<1>(results);
    auto t3 = std::get<2>(results);

    SyncOut() << "co_return => " << t1 << ", " << t2 << ", " << t3 << '\n';
}

tinycoro::Task<void> Example_AnyOfCoAwait2(auto& scheduler)
{
    SyncOut() << "\n\nExample_AnyOf:\n";

    auto task1 = [](auto duration) -> tinycoro::Task<int32_t> {
        SyncOut() << "  Coro starting..." << "  Thread id : " << std::this_thread::get_id() << '\n';

        int32_t count{0};

        for (auto start = std::chrono::system_clock::now(); std::chrono::system_clock::now() - start < duration;)
        {
            co_await tinycoro::CancellableSuspend{++count};
        }
        co_return count;
    };

    auto stopSource = co_await tinycoro::StopSourceAwaiter{};

    auto results = co_await tinycoro::AnyOfStopSourceAwait(scheduler, stopSource, task1(1s), task1(2s), task1(3s));

    auto t1 = std::get<0>(results);
    auto t2 = std::get<1>(results);
    auto t3 = std::get<2>(results);

    SyncOut() << "co_return => " << t1 << ", " << t2 << ", " << t3 << '\n';
}

int main()
{
    tinycoro::CoroScheduler scheduler{std::thread::hardware_concurrency()};


    auto future = scheduler.Enqueue(Example_asyncAwaiter(scheduler));
    std::cout << future.get() << '\n';

    auto future2 = scheduler.Enqueue(Example_asyncAwaiter2(scheduler));
    future2.get();

    scheduler.Enqueue(Example_AnyOfCoAwait(scheduler)).get();
    scheduler.Enqueue(Example_AnyOfCoAwait2(scheduler)).get();

    /*{
        Example_voidTask(scheduler);

        Example_taskView(scheduler);

        Example_returnValueTask(scheduler);

        Example_moveOnlyValue(scheduler);

        Example_aggregateValue(scheduler);

        Example_exception(scheduler);

        Example_nestedTask(scheduler);

        Example_nestedException(scheduler);

        Example_generator();

        Example_multiTasks(scheduler);

        Example_multiMovedTasksDynamic(scheduler);

        Example_multiMovedTasksDynamicVoid(scheduler);

        Example_multiTasksDynamic(scheduler);

        Example_multiTaskDifferentValues(scheduler);

        Example_sleep(scheduler);

        Example_asyncPulling(scheduler);

        Example_asyncCallbackAwaiter(scheduler);

        Example_asyncCallbackAwaiter_CStyle(scheduler);

        Example_asyncCallbackAwaiter_CStyleVoid(scheduler);

        Example_asyncCallbackAwaiterWithReturnValue(scheduler);

        Example_usageWithStopToken(scheduler);

        Example_AnyOfVoid(scheduler);

        Example_AnyOf(scheduler);

        Example_AnyOfDynamic(scheduler);

        Example_AnyOfDynamicVoid(scheduler);

        Example_AnyOfException(scheduler);

        Example_CustomAwaiter(scheduler);
    }*/

    return 0;
}
