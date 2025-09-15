#include "chimbuko/core/util/DispatchQueue.hpp"
#include <iostream>

using namespace chimbuko;

DispatchQueue::DispatchQueue(std::string name, size_t thread_cnt) 
    : m_name(name), m_threads(thread_cnt), m_quit(false)
{
    for (size_t i = 0; i < m_threads.size(); i++) {
        m_threads[i] = std::thread(&DispatchQueue::thread_handler, this);
    }
}

DispatchQueue::~DispatchQueue() {
    // Signal to dispatch threads that it's time to wrap up
    std::unique_lock<std::mutex> lock(m_lock);
    m_quit = true;
    lock.unlock();
    m_cv.notify_all();

    // Wait for threads to finish before we exit
    for (size_t i = 0; i < m_threads.size(); i++) {
        if (m_threads[i].joinable()) {
            m_threads[i].join();
        }
    }
}

void DispatchQueue::dispatch(const fp_t& op) {
    std::unique_lock<std::mutex> lock(m_lock);
    m_q.push(op);

    // Manual unlocking is done before notifying to avoid waking up
    // the waiting thread only to block again (see notify_one for details)
    lock.unlock();
    m_cv.notify_all();
}

void DispatchQueue::dispatch(fp_t&& op)
{
    std::unique_lock<std::mutex> lock(m_lock);
    m_q.push(std::move(op));

    // Manual unlocking is done before notifying to avoid waking up
    // the waiting thread only to block again (see notify_one for details)
    lock.unlock();
    m_cv.notify_all();
}

size_t DispatchQueue::size() 
{
    std::unique_lock<std::mutex> lock(m_lock);
    return m_q.size();
}

void DispatchQueue::thread_handler(void)
{
    std::unique_lock<std::mutex> lock(m_lock);
    do {
        // Wait until we have data or a quit signal
        m_cv.wait(lock, [this]{
            return (m_q.size() || m_quit);
        });

        // after wait, we own the lock
        if (!m_quit || m_q.size()) {
            auto op = std::move(m_q.front());
            m_q.pop();

            // unlock now that we're done messing with the queue
            lock.unlock();

            op();

            lock.lock();
        }
    } while (!m_quit || m_q.size());
}