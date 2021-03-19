#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <utility>

/**
 * @brief A multi-threaded wrapper around FIFO queue (std::queue)
 */
template <typename T>
class mtQueue
{
public:
    mtQueue(){}
    ~mtQueue()
    {
        invalidate();
    }

    /**
     * @brief Try to obtain a value from the front of the queue
     * @param[out] out The value
     * @return True if the value is populated, false if the queue is invalid or the queue is empty
     */
    bool tryPop(T& out)
    {
        std::lock_guard<std::mutex> _{m_mutex};
        if (m_queue.empty() || !m_valid)
            return false;

        out = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

    /**
     * @brief Wait until the queue either has an entry or is invalidated. Value taken from front of queue.
     * @param[out] out The value
     * @return True if queue is valid, false otherwise
     */
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

    /**
     * @brief Push a new value onto the end of the queue
     */
    void push(T value)
    {
        std::lock_guard<std::mutex> _{m_mutex};
        m_queue.push(std::move(value));
        m_cond.notify_one();
    }

    /** @brief Return true if the queue is empty */
    bool empty() const
    {
        std::lock_guard<std::mutex> _{m_mutex};
        return m_queue.empty();
    }

    /** @brief Remove all entries from the queue */
    void clear()
    {
        std::lock_guard<std::mutex> _{m_mutex};
        while (!m_queue.empty())
            m_queue.pop();
        m_cond.notify_all();
    }

    /**
     * @brief Mark the queue as invalid
     */
    void invalidate()
    {
        std::lock_guard<std::mutex> _{m_mutex};
        m_valid = false;
        m_cond.notify_all();
    }

    /**
     * Check if the queue has been invalidated
     */    
    bool is_valid() const 
    {
        std::lock_guard<std::mutex> _{m_mutex};
        return m_valid;
    }

    /** @brief The number of entries in the queue */
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
