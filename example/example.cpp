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

#include <semaphore>

void async_get(std::string url, const std::function<void(const std::string&)>& callback)
{
    std::jthread t{[url, callback] {
        std::this_thread::sleep_for(1000ms);
        callback(url + " Response");
    }};
    t.detach();
}

void async_prepare(std::string response, const std::function<void(const std::string&)>& callback)
{
    std::jthread t{[response, callback] {
        std::this_thread::sleep_for(1000ms);
        callback(response + " Done");
    }};
    t.detach();
}

std::string DownloadAndPrepare(const std::string& url)
{
    std::binary_semaphore semaphore{0};
    std::string result{};
    async_get(url, [&result, &semaphore](const std::string& response) {
        async_prepare(response, [&result, &semaphore](const std::string& response) {
            result = response;
            semaphore.release();
        });
    });
    semaphore.acquire();
    return result;
}

tinycoro::Task<std::string> AsyncGet(auto url)
{
    std::string result;
    co_await tinycoro::MakeAsyncCallbackAwaiter(async_get, url, tinycoro::UserCallback{[&result](auto res){ result = res; }});
    co_return result;
}

tinycoro::Task<std::string> AsyncPrepare(auto result)
{
    co_await tinycoro::MakeAsyncCallbackAwaiter(async_prepare, result, tinycoro::UserCallback{[&result](auto res){ result = res; }});
    co_return result;
}

tinycoro::Task<std::string> DownloadAndPrepareCoro(const std::string& url)
{
    auto result = co_await AsyncGet(url);
    co_return co_await AsyncPrepare(result);
}

void Example_asyncCallbackAwaiter_CStyle3(auto& scheduler)
{
    SyncOut() << "\n\nExample_asyncCallbackAwaiter_CStyle3:\n";

    auto task1 = []() -> tinycoro::Task<void> {
        
        auto task2 = []() -> tinycoro::Task<void> {
                SyncOut() << "  Task2 AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';


            auto cb = [](tinycoro::CustomUserData userData, int i, int j) {
                SyncOut() << "  Callback called... i: " << i  << "  j: " << j << " Thread id: " << std::this_thread::get_id() << '\n';

                auto val = static_cast<int*>(userData.value);
                assert(*val == 42);
            };

            int i = 42;

            co_await tinycoro::MakeAsyncCallbackAwaiter_CStyle(AsyncCallbackAPI, tinycoro::CustomUserData{&i}, tinycoro::UserCallback{cb}, 44);
        };

        SyncOut() << "  Task1 AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';

        co_await task2();
    };

    auto future = scheduler.Enqueue(task1());

    future.get();

    SyncOut() << "co_return => void" << '\n';
}

void Example_asyncCallbackAwaiter_CStyle4(auto& scheduler)
{
    SyncOut() << "\n\nExample_asyncCallbackAwaiter_CStyle4:\n";

    auto task1 = []() -> tinycoro::Task<void> {
        
        auto task2 = []() -> tinycoro::Task<void> {
                SyncOut() << "  Task2 AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';


            auto cb = [](tinycoro::CustomUserData userData, int i) {
                SyncOut() << "  Callback called... i: " << i << " Thread id: " << std::this_thread::get_id() << '\n';

                auto val = userData.Get<int>();
                assert(val == 42);
            };

            int i = 42;

            co_await tinycoro::MakeAsyncCallbackAwaiter_CStyle(AsyncCallbackAPIvoid, tinycoro::UserCallback{cb}, tinycoro::CustomUserData{&i});
        };

        SyncOut() << "  Task1 AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';

        co_await task2();
    };

    auto future = scheduler.Enqueue(task1());

    future.get();

    SyncOut() << "co_return => void" << '\n';
}

void Example_asyncCallbackAwaiter_5(auto& scheduler)
{
    SyncOut() << "\n\nExample_asyncCallbackAwaiter_5:\n";

    auto task1 = []() -> tinycoro::Task<void> {
        
        auto task2 = []() -> tinycoro::Task<void> {
                SyncOut() << "  Task2 AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';


            auto cb = [](int i, bool flag) {
                SyncOut() << "  Callback called... i: " << i  << " flag: " << flag << " Thread id: " << std::this_thread::get_id() << '\n';

                assert(i == 42);
                assert(flag);
            };

            auto jthread = co_await tinycoro::MakeAsyncCallbackAwaiter(AsyncCallbackAPIFunctionObject, tinycoro::UserCallback{cb}, true);
        };

        SyncOut() << "  Task1 AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';

        co_await task2();
    };

    auto future = scheduler.Enqueue(task1());

    future.get();

    SyncOut() << "co_return => void" << '\n';
}

void Example_asyncCallbackAwaiter_6(auto& scheduler)
{
    SyncOut() << "\n\nExample_asyncCallbackAwaiter_6:\n";

    auto task1 = []() -> tinycoro::Task<void> {
        
        auto task2 = []() -> tinycoro::Task<void> {
                SyncOut() << "  Task2 AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';


            auto cb = [](int i, bool flag) {
                SyncOut() << "  Callback called... i: " << i  << " flag: " << flag << " Thread id: " << std::this_thread::get_id() << '\n';

                assert(i == 42);
                assert(flag);
            };

            co_await tinycoro::MakeAsyncCallbackAwaiter(AsyncCallbackAPIFunctionObjectVoid, tinycoro::UserCallback{cb}, true);
        };

        SyncOut() << "  Task1 AsyncCallback... Thread id: " << std::this_thread::get_id() << '\n';

        co_await task2();
    };

    auto future = scheduler.Enqueue(task1());

    future.get();

    SyncOut() << "co_return => void" << '\n';
}



int main()
{
    auto result = DownloadAndPrepare("testUrl");
    std::cout << result << '\n';

    tinycoro::CoroScheduler scheduler{std::thread::hardware_concurrency()};
    
    auto future = scheduler.Enqueue(DownloadAndPrepareCoro("testUrl"));
    auto result2 = tinycoro::GetAll(future);
    std::cout << result2 << '\n';
    

    Example_asyncCallbackAwaiter_CStyle3(scheduler);
    Example_asyncCallbackAwaiter_CStyle4(scheduler);

    Example_asyncCallbackAwaiter_5(scheduler);



    //tinycoro::CoroScheduler scheduler{std::thread::hardware_concurrency()};
    {
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
    }

    return 0;
}
