#pragma once

#include<chimbuko/ad/ADProvenanceDBclient.hpp>
#include<chimbuko/ad/ADMetadataParser.hpp>
#include<chimbuko/ad/ADNormalEventProvenance.hpp>
#include<chimbuko/ad/ADEvent.hpp>
#include<chimbuko/ad/ADCounter.hpp>

namespace chimbuko_sim{
  using namespace chimbuko;

  enum class CommType { Send, Recv };

  //An object that represents a rank of the AD
  class ADsim{
    std::unordered_map<unsigned long, std::list<ExecData_t> > m_all_execs; //map of thread to execs, never flushed
    std::unordered_map<eventID, CallListIterator_t> m_eventid_map; //map from event index to iterator ; never flushed
    std::list<CallListIterator_t> m_step_exec_its; //iterators to events on this io step; flushed at end of step
    std::unordered_map<unsigned long, std::vector<CallListIterator_t>> m_step_func_exec_its; //map of function id to iterators on this io step; flushed at end of step

    int m_step;
    int m_window_size;
    int m_pid;
    int m_rid;
    unsigned long m_step_start_time;
    ADNormalEventProvenance m_normal_events;
    ADCounter m_counters;
    ADMetadataParser m_metadata;
    std::unique_ptr<ADProvenanceDBclient> m_pdb_client;
  public:
    void init(int window_size, int pid, int rid);

    ADsim(int window_size, int pid, int rid): ADsim(){
      init(window_size, pid, rid);
    }
    ADsim(){}

    ADProvenanceDBclient &getProvDBclient(){ return *m_pdb_client; }

    //Add a function execution on a specific thread
    CallListIterator_t addExec(const int thread,
			       const std::string &func_name,
			       unsigned long start,
			       unsigned long runtime,
			       bool is_anomaly,
			       double outlier_score = 0.);

    //Attach a counter to an execution t_delta us after the start of the execution
    void attachCounter(const std::string &counter_name,
		       unsigned long value,
		       long t_delta,
		       CallListIterator_t to);

    //Attach a communication event to an execution t_delta us after the start of the execution
    //partner_rank is the origin rank of a receive or the destination rank of a send
    void attachComm(CommType type,
		    unsigned long partner_rank,
		    unsigned long bytes,
		    long t_delta,
		    CallListIterator_t to);
   
    //Register a thread index as corresponding to a GPU thread, allowing population of GPU information in provenance data
    void registerGPUthread(const int tid);
    
    //Register a GPU kernel event as originating from a cpu event
    void bindCPUparentGPUkernel(CallListIterator_t cpu_parent, CallListIterator_t gpu_kern);

    void beginStep(const unsigned long step_start_time);

    void endStep(const unsigned long step_end_time);
  };

  inline std::ostream & operator<<(std::ostream &os, const CallListIterator_t it){
    os << it->get_json(true, true).dump(4);
    return os;
  }

};
