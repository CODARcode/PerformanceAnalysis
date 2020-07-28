#pragma once

#include<thread>
#include <condition_variable>
#include <mutex>

namespace chimbuko{

  /**
   * @brief Thread barrier
   */
  class Barrier {
  public:
    /**
     * @brief Constructor
     * @param iCount The number of threads in the barrier
     */
    explicit Barrier(std::size_t iCount) : 
      mThreshold(iCount), 
      mCount(iCount), 
      mGeneration(0) {
    }

    void wait() {
      std::unique_lock<std::mutex> lLock{mMutex};
      auto lGen = mGeneration;
      if (!--mCount) {
	mGeneration++;
	mCount = mThreshold;
	mCond.notify_all();
      } else {
	mCond.wait(lLock, [this, lGen] { return lGen != mGeneration; }); //stay here until lGen != mGeneration which will happen when one thread has incremented the generation counter
      }
    }

  private:
    std::mutex mMutex;
    std::condition_variable mCond;
    std::size_t mThreshold;
    std::size_t mCount;
    std::size_t mGeneration;
  };

};
