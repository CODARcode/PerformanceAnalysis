#pragma once
#include <thread>
#include <functional>
#include <string>
#include <mutex>
#include <vector>
#include <queue>
#include <condition_variable>

namespace chimbuko {

class DispatchQueue {
    typedef std::function<void(void)> fp_t;

public:
    DispatchQueue(std::string name, size_t thread_cnt=1);
    ~DispatchQueue();

    void dispatch(const fp_t& op);
    void dispatch(fp_t&& op);

private:
    void thread_handler(void);

    std::string m_name;
    std::mutex m_lock;
    std::vector<std::thread> m_threads;
    std::queue<fp_t> m_q;
    std::condition_variable m_cv;
    bool m_quit;
};

}