#pragma once
#include <functional>
#include <future>
#include <queue>

class ThreadPool {
public:
    ThreadPool(size_t nt);
    ~ThreadPool();

    template<class F, class... Args>
    decltype(auto) enqueue(F&& f, Args&&... args);

private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > m_workers;
    // the task queue
    std::queue< std::function<void()> > m_tasks;

    // synchronization
    std::mutex m_queue_mutex;
    std::condition_variable m_cv;
    bool m_stop;
};

template<class F, class... Args>
decltype(auto) ThreadPool::enqueue(F&& f, Args&&... args)
{
    using return_type = std::invoke_result_t<F, Args...>;

    std::packaged_task<return_type()> task(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    // auto task = std::make_shared< std::packaged_task<return_type()> >(
    //     std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    // );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(m_queue_mutex);

        // don't allow enqueueing after stopping the pool
        if (m_stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        m_tasks.emplace(std::make_unique)
        //m_tasks.emplace(std::move(task));
        // m_tasks.emplace([task](){ (*task)(); });
    }
    m_cv.notify_one();
    return res;
}