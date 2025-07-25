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

#include "AutoEvent_example.h"

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
#include "UsageWithStopToken_example.h"
#include "VoidTask_example.h"

#include "CustomAwaiter.h"

#include "AnyOfCoAwait_example.h"
#include "AllOfAwait_example.h"

#include "Semaphore_example.h"

#include "BufferedChannel_example.h"

int main()
{
    tinycoro::Scheduler scheduler;
    {
        Example_voidTask(scheduler);

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

        Example_AutoEvent(scheduler);

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

        auto val = tinycoro::AllOf(scheduler, Example_AllOfAwait(scheduler));
        SyncOut() << *val << '\n';

        scheduler.Enqueue(Example_AnyOfCoAwait(scheduler)).get();

        Example_Semaphore(scheduler);

        Example_bufferedChannel(scheduler);
    }

    return 0;
}
