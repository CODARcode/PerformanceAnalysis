#pragma once

#include "chimbuko/util/mtQueue.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

class threadPool
{
private:
    class IThreadTask
    {
    public:
        IThreadTask() = default;
        virtual ~IThreadTask() = default;

        //Can only be moved, not copied
        IThreadTask(const IThreadTask& rhs) = delete;
        IThreadTask& operator=(const IThreadTask& rhs) = delete;
        IThreadTask(IThreadTask&& other) = default;
        IThreadTask& operator=(IThreadTask&& other) = default;

        virtual void execute() = 0;
    };

    template <typename Func>
    class ThreadTask: public IThreadTask
    {
    public:
        ThreadTask(Func&& func) : m_func{std::move(func)}
        {

        }
        ~ThreadTask() override = default;

        //Can only be moved, not copied
        ThreadTask(const ThreadTask& rhs) = delete;
        ThreadTask& operator=(const ThreadTask& rhs) = delete;
        ThreadTask(ThreadTask&& other) = default;
        ThreadTask& operator=(ThreadTask&& other) = default;

        void execute() override
        {
            m_func();
        }

    private:
        Func m_func;
    };

public:

    /**
     * @brief A wrapper class for an std::future instance representing the result of an asynchronous operation
     */
    template <typename T>
    class TaskFuture
    {
    public:
        TaskFuture(std::future<T>&& future) : m_future{std::move(future)}
        {

        }
        /**
	 * @brief The destructor waits for the asynchronous operation to complete before exiting
	 */
        ~TaskFuture()
        {
            if (m_future.valid())
            {
                m_future.get();
            }
        }
        TaskFuture(const TaskFuture& rhs) = delete;
        TaskFuture& operator=(const TaskFuture& rhs) = delete;
        TaskFuture(TaskFuture&& other) = default;
        TaskFuture& operator=(TaskFuture&& other) = default;

        /**
	 * @brief Wait until the asynchronous operation has completed and return the value
	 */
        auto get()
        {
            return m_future.get();
        }
    private:
        std::future<T> m_future;
    };

public:
    threadPool() : threadPool{std::max(std::thread::hardware_concurrency(), 2u) - 1u}
    {

    }

    /**
     * @brief Instantiate a pool of nt threads
     * @param nt The number of threads to instantiate
     */
    explicit threadPool(const std::uint32_t nt) : m_done{false}, m_workQueue{}, m_threads{}
    {
        try
        {
            for (std::uint32_t i = 0u; i < nt; ++i)
                m_threads.emplace_back(&threadPool::worker, this);
        }
        catch(...)
        {
            destroy();
            throw;
        }        
    }

    /**
     * @brief The class is not copyable but can be moved
     */
    threadPool(const threadPool& rhs) = delete;
    threadPool& operator=(const threadPool& rhs) = delete;
    ~threadPool()
    {
        destroy();
    }

    template <typename Func, typename... Args>
    auto sumit(Func&& func, Args&&... args)
    {
        auto boundTask = std::bind(std::forward<Func>(func), std::forward<Args>(args)...);
        using result_type = std::result_of_t<decltype(boundTask)()>;
        using packaged_task = std::packaged_task<result_type()>;
        using task_type = ThreadTask<packaged_task>;

        packaged_task task{std::move(boundTask)};
        TaskFuture<result_type> result{task.get_future()};
        m_workQueue.push(std::make_unique<task_type>(std::move(task)));
        return result;
    }

    size_t pool_size() const {
        return m_threads.size();
    }

    size_t queue_size() const {
        return m_workQueue.size();
    }

private:
    void worker()
    {
        while(!m_done)
        {
            std::unique_ptr<IThreadTask> pTask{nullptr};
            if (m_workQueue.waitPop(pTask))
            {
                pTask->execute();
            }
        }
    }

    void destroy()
    {
        m_done = true;
        m_workQueue.invalidate();
        for (auto& t: m_threads)
            if (t.joinable())
                t.join();
    }

private:
    std::atomic_bool m_done;
    mtQueue<std::unique_ptr<IThreadTask>> m_workQueue;
    std::vector<std::thread> m_threads;
};

namespace DefaultThreadPool
{
    inline threadPool& getThreadPool()
    {
        static threadPool defaultPool;
        return defaultPool;
    }

    template <typename Func, typename... Args>
    inline auto submitJob(Func&& func, Args&&... args)
    {
        return getThreadPool().sumit(std::forward<Func>(func), std::forward<Args>(args)...);
    }
}
