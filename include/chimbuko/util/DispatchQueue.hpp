#pragma once
#include <chimbuko_config.h>
#include <thread>
#include <functional>
#include <string>
#include <mutex>
#include <vector>
#include <queue>
#include <condition_variable>

namespace chimbuko {
  
  /**
   * @brief A class for dispatching work items over a thread pool
   * 
   */
  class DispatchQueue {
    typedef std::function<void(void)> fp_t;
    
  public:
    /**
     * @brief Construct an instance of class, providing a name for the instance and the number of threads
     *
     * @param name The name of the instance
     * @param thread_cnt The number of threads (default 1)
     */
    DispatchQueue(std::string name, size_t thread_cnt=1);
    ~DispatchQueue();

    /**
     * @brief Enqueue a work item (lvalue reference)
     *
     * @param op An instance of std::function<void(void)>
     *
     */
    void dispatch(const fp_t& op);

    /**
     * @brief Enqueue a work item (rvalue reference)
     *
     * @param op An instance of std::function<void(void)>
     *
     */
    void dispatch(fp_t&& op);

    /**
     * @brief Return the number of outstanding work items in the queue
     *
     */
    size_t size();

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
