#pragma once

#include<thread>
#include <condition_variable>
#include <mutex>
#include <chimbuko/ad/ADDefine.hpp>
#include <chimbuko/ad/ExecData.hpp>
#include <sstream>
#include <list>

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

/**
 * @brief Create an Event_t object from the inputs provided. Index will be internally assigned, as will name
 */
chimbuko::Event_t createFuncEvent_t(unsigned long pid,
			  unsigned long rid,
			  unsigned long tid,
			  unsigned long eid,
			  unsigned long func_id,
			  unsigned long ts){
  using namespace chimbuko;
  static size_t event_idx = -1;
  event_idx++;
  std::stringstream ss; ss << "event" << event_idx;
  
  static std::list< std::array<unsigned long, FUNC_EVENT_DIM> > todelete; //make sure they get deleted eventually
  std::array<unsigned long, FUNC_EVENT_DIM> ev;
  ev[IDX_P] = pid;
  ev[IDX_R] = rid;
  ev[IDX_T] = tid;
  ev[IDX_E] = eid;
  ev[FUNC_IDX_F] = func_id;
  ev[FUNC_IDX_TS] = ts;
  todelete.push_back(ev);

  return Event_t(todelete.back().data(), EventDataType::FUNC, event_idx, ss.str());
}

/**
 * @brief Create an ExecData_t from the inputs provided
 */
chimbuko::ExecData_t createFuncExecData_t(unsigned long pid,
				unsigned long rid,
				unsigned long tid,
				unsigned long func_id,
				const std::string &func_name,
				long start,
				long runtime){
  using namespace chimbuko;
  Event_t entry = createFuncEvent_t(pid, rid, tid, 0, func_id, start);
  Event_t exit = createFuncEvent_t(pid, rid, tid, 1, func_id, start + runtime);

  ExecData_t exec(entry);
  exec.update_exit(exit);
  exec.set_funcname(func_name);
  return exec;
}



/**
 * @brief Create an Event_t object from the inputs provided. Index will be internally assigned, as will name
 */
chimbuko::Event_t createCounterEvent_t(unsigned long pid,
				       unsigned long rid,
				       unsigned long tid,
				       unsigned long counter_id,
				       unsigned long value,
				       unsigned long ts){
  using namespace chimbuko;
  static size_t event_idx = -1;
  event_idx++;
  std::stringstream ss; ss << "counter_event" << event_idx;
  
  static std::list< std::array<unsigned long, COUNTER_EVENT_DIM> > todelete; //make sure they get deleted eventually
  std::array<unsigned long, COUNTER_EVENT_DIM> ev;
  ev[IDX_P] = pid;
  ev[IDX_R] = rid;
  ev[IDX_T] = tid;
  ev[COUNTER_IDX_ID] = counter_id;
  ev[COUNTER_IDX_VALUE] = value;
  ev[COUNTER_IDX_TS] = ts;
  todelete.push_back(ev);

  return Event_t(todelete.back().data(), EventDataType::COUNT, event_idx, ss.str());
}
