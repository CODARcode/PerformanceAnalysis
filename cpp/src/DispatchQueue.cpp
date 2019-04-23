#include "DispatchQueue.hpp"
#include <iostream>

DispatchQueue::DispatchQueue(std::string name, size_t thread_cnt) 
    : m_name(name), m_threads(thread_cnt), m_quit(false)
{
    std::cout << "Creating dispatch queue: " << m_name << std::endl;
    std::cout << "Dispatch threads: " << m_threads.size() << std::endl;

    for (size_t i = 0; i < m_threads.size(); i++) {
        std::cout << "thread " << i << std::endl;
        m_threads[i] = std::thread(&DispatchQueue::thread_handler, this);
    }
}

DispatchQueue::~DispatchQueue() {
    std::cout << "Destroying dispatch threads...\n";

    // Signal to dispatch threads that it's time to wrap up
    std::unique_lock<std::mutex> lock(m_lock);
    m_quit = true;
    lock.unlock();
    m_cv.notify_all();

    // Wait for threads to finish before we exit
    for (size_t i = 0; i < m_threads.size(); i++) {
        if (m_threads[i].joinable()) {
            std::cout << "Joining thread " << i << " until completion\n";
            m_threads[i].join();
        }
    }
}

void DispatchQueue::dispatch(const fp_t& op) {
    std::cout << "dispatch(const fp_t& op)" << std::endl;
    std::unique_lock<std::mutex> lock(m_lock);
    m_q.push(op);

    // Manual unlocking is done before notifying to avoid waking up
    // the waiting thread only to block again (see notify_one for details)
    lock.unlock();
    m_cv.notify_all();
}

void DispatchQueue::dispatch(fp_t&& op)
{
    std::cout << "dispatch(fp_t&& op)" << std::endl;
    std::unique_lock<std::mutex> lock(m_lock);
    m_q.push(std::move(op));

    // Manual unlocking is done before notifying to avoid waking up
    // the waiting thread only to block again (see notify_one for details)
    lock.unlock();
    m_cv.notify_all();
}

void DispatchQueue::thread_handler(void)
{
    std::unique_lock<std::mutex> lock(m_lock);
    do {
        // Wait until we have data or a quit signal
        std::cout << "Wait until we have data or a quit signal" << std::endl;
        m_cv.wait(lock, [this]{
            return (m_q.size() || m_quit);
        });

        // after wait, we own the lock
        if (!m_quit || m_q.size()) {
            std::cout << "Receive data!" << std::endl;
            auto op = std::move(m_q.front());
            m_q.pop();

            // unlock now that we're done messing with the queue
            lock.unlock();

            op();

            lock.lock();
        }
    } while (!m_quit || m_q.size());
    std::cout << "Quit" << std::endl;
}