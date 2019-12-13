#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

template <typename T>
class mtQueue
{
public:
    mtQueue(){}
    ~mtQueue()
    {
        invalidate();
    }

    bool tryPop(T& out)
    {
        std::lock_guard<std::mutex> _{m_mutex};
        if (m_queue.empty() || !m_valid)
            return false;

        out = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    bool waitPop(T& out)
    {
        std::unique_lock<std::mutex> lock{m_mutex};
        m_cond.wait(lock, [this](){
            return !m_queue.empty() || !m_valid;
        });

        if (!m_valid)
            return false;

        out = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }
   
    void pop()
    {
	std::unique_lock<std::mutex> lock{m_mutex};
	if (!m_queue.empty())
		 m_queue.pop();
    }	

    T front()
    {
	std::unique_lock<std::mutex> lock{m_mutex};
       // if (!m_queue.empty())
                 return m_queue.front();


    }	


    void push(T value)
    {
        std::lock_guard<std::mutex> _{m_mutex};
        m_queue.push(std::move(value));
        m_cond.notify_one();
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> _{m_mutex};
        return m_queue.empty();
    }

    void clear()
    {
        std::lock_guard<std::mutex> _{m_mutex};
        while (!m_queue.empty())
            m_queue.pop();
        m_cond.notify_all();
    }

    void invalidate()
    {
        std::lock_guard<std::mutex> _{m_mutex};
        m_valid = false;
        m_cond.notify_all();
    }

    bool is_valid() const 
    {
        std::lock_guard<std::mutex> _{m_mutex};
        return m_valid;
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> _{m_mutex};
        return m_queue.size();
    }

private:
    std::atomic_bool m_valid{true};
    mutable std::mutex m_mutex;
    std::queue<T> m_queue;
    std::condition_variable m_cond;
};
