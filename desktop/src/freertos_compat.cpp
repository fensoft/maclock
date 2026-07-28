#include <freertos/event_groups.h>
#include <freertos/semphr.h>
#include <freertos/stream_buffer.h>
#include <freertos/task.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <pthread.h>
#include <thread>
#include <vector>

struct LocalTask
{
    std::thread thread;
    std::mutex mutex;
    std::condition_variable condition;
    std::atomic<bool> suspended{false};
    std::atomic<bool> finished{false};
};

struct LocalSemaphore
{
    std::recursive_timed_mutex mutex;
};

struct LocalEventGroup
{
    std::mutex mutex;
    std::condition_variable condition;
    EventBits_t bits = 0;
};

struct LocalStreamBuffer
{
    explicit LocalStreamBuffer(size_t size)
        : capacity(size)
    {
    }
    std::mutex mutex;
    std::condition_variable condition;
    std::deque<uint8_t> bytes;
    size_t capacity;
};

namespace
{
thread_local LocalTask *current_task = nullptr;
std::atomic<bool> shutting_down{false};
std::mutex tasks_mutex;
std::vector<LocalTask *> tasks;
}

BaseType_t xTaskCreatePinnedToCore(
    TaskFunction_t function,
    const char *,
    uint32_t,
    void *parameter,
    UBaseType_t,
    TaskHandle_t *created_task,
    BaseType_t)
{
    if (!function)
        return pdFAIL;
    auto *task = new LocalTask();
    {
        std::lock_guard<std::mutex> lock(tasks_mutex);
        tasks.push_back(task);
    }
    task->thread = std::thread(
        [task, function, parameter]()
        {
            current_task = task;
            function(parameter);
            task->finished = true;
            task->condition.notify_all();
            current_task = nullptr;
        });
    if (created_task)
        *created_task = task;
    else
        task->thread.detach();
    return pdPASS;
}

void vTaskDelay(TickType_t ticks)
{
    if (shutting_down)
        pthread_exit(nullptr);
    if (current_task)
    {
        std::unique_lock<std::mutex> lock(current_task->mutex);
        current_task->condition.wait(
            lock,
            [=]()
            {
                return !current_task->suspended.load();
            });
    }
    std::this_thread::sleep_for(
        std::chrono::milliseconds(ticks));
    if (shutting_down)
        pthread_exit(nullptr);
}

void vTaskDelete(TaskHandle_t task)
{
    if (!task)
        return;
    task->suspended = false;
    task->condition.notify_all();
    if (task->thread.joinable())
        task->thread.join();
    {
        std::lock_guard<std::mutex> lock(tasks_mutex);
        tasks.erase(
            std::remove(tasks.begin(), tasks.end(), task),
            tasks.end());
    }
    delete task;
}

void vTaskSuspend(TaskHandle_t task)
{
    if (task)
        task->suspended = true;
}

void vTaskResume(TaskHandle_t task)
{
    if (!task)
        return;
    task->suspended = false;
    task->condition.notify_all();
}

void maclock_local_freertos_shutdown()
{
    shutting_down = true;
    std::vector<LocalTask *> snapshot;
    {
        std::lock_guard<std::mutex> lock(tasks_mutex);
        snapshot = tasks;
    }
    for (LocalTask *task : snapshot)
    {
        task->suspended = false;
        task->condition.notify_all();
    }
    for (LocalTask *task : snapshot)
    {
        if (task->thread.joinable())
            task->thread.join();
        delete task;
    }
    {
        std::lock_guard<std::mutex> lock(tasks_mutex);
        tasks.clear();
    }
}

SemaphoreHandle_t xSemaphoreCreateMutex()
{
    return new LocalSemaphore();
}

BaseType_t xSemaphoreTake(
    SemaphoreHandle_t semaphore, TickType_t timeout)
{
    if (!semaphore)
        return pdFALSE;
    if (timeout == portMAX_DELAY)
    {
        semaphore->mutex.lock();
        return pdTRUE;
    }
    return semaphore->mutex.try_lock_for(
               std::chrono::milliseconds(timeout))
               ? pdTRUE
               : pdFALSE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore)
{
    if (!semaphore)
        return pdFALSE;
    semaphore->mutex.unlock();
    return pdTRUE;
}

void vSemaphoreDelete(SemaphoreHandle_t semaphore)
{
    delete semaphore;
}

EventGroupHandle_t xEventGroupCreate()
{
    return new LocalEventGroup();
}

EventBits_t xEventGroupSetBits(
    EventGroupHandle_t group, EventBits_t bits)
{
    if (!group)
        return 0;
    std::lock_guard<std::mutex> lock(group->mutex);
    group->bits |= bits;
    group->condition.notify_all();
    return group->bits;
}

EventBits_t xEventGroupWaitBits(
    EventGroupHandle_t group,
    EventBits_t bits,
    BaseType_t clear_on_exit,
    BaseType_t wait_for_all,
    TickType_t timeout)
{
    if (!group)
        return 0;
    auto ready = [=]()
    {
        return wait_for_all
                   ? (group->bits & bits) == bits
                   : (group->bits & bits) != 0;
    };
    std::unique_lock<std::mutex> lock(group->mutex);
    if (timeout == portMAX_DELAY)
        group->condition.wait(lock, ready);
    else
        group->condition.wait_for(
            lock, std::chrono::milliseconds(timeout), ready);
    const EventBits_t result = group->bits;
    if (clear_on_exit && ready())
        group->bits &= ~bits;
    return result;
}

void vEventGroupDelete(EventGroupHandle_t group)
{
    delete group;
}

StreamBufferHandle_t xStreamBufferCreateStatic(
    size_t capacity,
    size_t,
    uint8_t *,
    StaticStreamBuffer_t *control)
{
    if (!control)
        return nullptr;
    if (!control->instance)
        control->instance = new LocalStreamBuffer(capacity);
    return control->instance;
}

size_t xStreamBufferSend(
    StreamBufferHandle_t stream,
    const void *data,
    size_t bytes,
    TickType_t timeout)
{
    if (!stream || !data)
        return 0;
    const auto *input = static_cast<const uint8_t *>(data);
    std::unique_lock<std::mutex> lock(stream->mutex);
    if (timeout)
    {
        stream->condition.wait_for(
            lock, std::chrono::milliseconds(timeout),
            [=]()
            {
                return stream->bytes.size() < stream->capacity;
            });
    }
    const size_t writable = std::min(
        bytes, stream->capacity - stream->bytes.size());
    for (size_t i = 0; i < writable; ++i)
        stream->bytes.push_back(input[i]);
    stream->condition.notify_all();
    return writable;
}

size_t xStreamBufferReceive(
    StreamBufferHandle_t stream,
    void *data,
    size_t bytes,
    TickType_t timeout)
{
    if (!stream || !data)
        return 0;
    auto *output = static_cast<uint8_t *>(data);
    std::unique_lock<std::mutex> lock(stream->mutex);
    if (stream->bytes.empty() && timeout)
    {
        if (timeout == portMAX_DELAY)
        {
            stream->condition.wait(
                lock,
                [=]() { return !stream->bytes.empty(); });
        }
        else
        {
            stream->condition.wait_for(
                lock, std::chrono::milliseconds(timeout),
                [=]() { return !stream->bytes.empty(); });
        }
    }
    const size_t readable =
        std::min(bytes, stream->bytes.size());
    for (size_t i = 0; i < readable; ++i)
    {
        output[i] = stream->bytes.front();
        stream->bytes.pop_front();
    }
    stream->condition.notify_all();
    return readable;
}

size_t xStreamBufferBytesAvailable(
    StreamBufferHandle_t stream)
{
    if (!stream)
        return 0;
    std::lock_guard<std::mutex> lock(stream->mutex);
    return stream->bytes.size();
}

BaseType_t xStreamBufferReset(StreamBufferHandle_t stream)
{
    if (!stream)
        return pdFAIL;
    std::lock_guard<std::mutex> lock(stream->mutex);
    stream->bytes.clear();
    stream->condition.notify_all();
    return pdPASS;
}
