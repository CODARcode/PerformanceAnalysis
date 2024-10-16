#pragma once
#include <chimbuko_config.h>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <chimbuko/ad/ADDefine.hpp>
#include <chimbuko/ad/ExecData.hpp>
#include <sstream>
#include <list>
#include <chimbuko/message.hpp>
#include <zmq.h>
#include <chimbuko/util/barrier.hpp>
#include <chimbuko/util/RunStats.hpp>

namespace chimbuko{
  /**
   * @brief Create an Event_t object from the inputs provided writing to 'to'. Index will be internally assigned, as will name. to must have size at least FUNC_EVENT_DIM
   */
  Event_t createFuncEvent_t(unsigned long pid,
			    unsigned long rid,
			    unsigned long tid,
			    unsigned long eid,
			    unsigned long func_id,
			    unsigned long ts,
			    unsigned long *to){
    static long event_idx_s = 0;
    long event_idx = event_idx_s++;
    
    eventID id(rid, 0, event_idx);

    to[IDX_P] = pid;
    to[IDX_R] = rid;
    to[IDX_T] = tid;
    to[IDX_E] = eid;
    to[FUNC_IDX_F] = func_id;
    to[FUNC_IDX_TS] = ts;
    return Event_t(to, EventDataType::FUNC, event_idx, id);
  }

  /**
   * @brief Create an Event_t object from the inputs provided. Index will be internally assigned, as will name
   */
  Event_t createFuncEvent_t(unsigned long pid,
			    unsigned long rid,
			    unsigned long tid,
			    unsigned long eid,
			    unsigned long func_id,
			    unsigned long ts,
			    std::list< std::array<unsigned long, FUNC_EVENT_DIM> > *into = nullptr){
    static std::list< std::array<unsigned long, FUNC_EVENT_DIM> > todelete; //make sure they get deleted eventually
    std::list< std::array<unsigned long, FUNC_EVENT_DIM> >* into_p = into == nullptr ? &todelete : into;

    into_p->push_back(std::array<unsigned long, FUNC_EVENT_DIM>());

    std::array<unsigned long, FUNC_EVENT_DIM> &ev = into_p->back();
    return createFuncEvent_t(pid,rid,tid,eid,func_id,ts,ev.data());
  }

  /**
   * @brief Create an ExecData_t from the inputs provided
   * If runtime == 0, no exit event will be generated
   */
  ExecData_t createFuncExecData_t(unsigned long pid,
				  unsigned long rid,
				  unsigned long tid,
				  unsigned long func_id,
				  const std::string &func_name,
				  unsigned long start,
				  unsigned long runtime,
				  std::list< std::array<unsigned long, FUNC_EVENT_DIM> > *into = nullptr){
    Event_t entry = createFuncEvent_t(pid, rid, tid, 0, func_id, start, into);
    ExecData_t exec(entry);

    if(runtime > 0){
      Event_t exit = createFuncEvent_t(pid, rid, tid, 1, func_id, start + runtime);
      exec.update_exit(exit);
    }
    exec.set_funcname(func_name);
    return exec;
  }


  /**
   * @brief Setup parent <-> child relationship between events
   */
  void bindParentChild(ExecData_t &parent, ExecData_t &child){
    child.set_parent(parent.get_id());
    parent.update_exclusive(child.get_runtime());
    parent.inc_n_children();
  }


  /**
   * @brief Create an Event_t object from the inputs provided writing to array 'to' which must have a size at least COUNTER_EVENT_DIM. Index will be internally assigned, as will name
   */
  Event_t createCounterEvent_t(unsigned long pid,
			       unsigned long rid,
			       unsigned long tid,
			       unsigned long counter_id,
			       unsigned long value,
			       unsigned long ts,
			       unsigned long* to){
    static long event_idx_s = 0;
    long event_idx = event_idx_s++;
    
    eventID id(rid, 0, event_idx);

    to[IDX_P] = pid;
    to[IDX_R] = rid;
    to[IDX_T] = tid;
    to[COUNTER_IDX_ID] = counter_id;
    to[COUNTER_IDX_VALUE] = value;
    to[COUNTER_IDX_TS] = ts;

    return Event_t(to, EventDataType::COUNT, event_idx, id);
  }



  /**
   * @brief Create an Event_t object from the inputs provided. Index will be internally assigned, as will name
   */
  Event_t createCounterEvent_t(unsigned long pid,
			       unsigned long rid,
			       unsigned long tid,
			       unsigned long counter_id,
			       unsigned long value,
			       unsigned long ts){
    static std::list< std::array<unsigned long, COUNTER_EVENT_DIM> > todelete; //make sure they get deleted eventually
    todelete.push_back(std::array<unsigned long, COUNTER_EVENT_DIM>());
    return createCounterEvent_t(pid,rid,tid,counter_id,value,ts,todelete.back().data());
  }


  /**
   * @brief Create an CounterData_t object from the inputs provided. Index will be internally assigned, as will name
   */
  CounterData_t createCounterData_t(unsigned long pid,
				    unsigned long rid,
				    unsigned long tid,
				    unsigned long counter_id,
				    unsigned long value,
				    unsigned long ts,
				    const std::string &counter_name){
    return CounterData_t( createCounterEvent_t(pid,rid,tid,counter_id,value,ts), counter_name);
  }


  /**
   * @brief Setup parent <-> child relationship between GPU event and CPU parent
   */
  void bindCPUparentGPUkernel(ExecData_t &cpu_parent, ExecData_t &gpu_kern, unsigned long corridx){
    assert(cpu_parent.get_pid() == gpu_kern.get_pid());
    assert(cpu_parent.get_rid() == gpu_kern.get_rid());

    gpu_kern.add_counter( createCounterData_t(gpu_kern.get_pid(), gpu_kern.get_rid(), gpu_kern.get_tid(), 99, corridx, gpu_kern.get_entry(), "Correlation ID" ) ); //correlation ID counter at start of kernel
    cpu_parent.add_counter( createCounterData_t(cpu_parent.get_pid(), cpu_parent.get_rid(), cpu_parent.get_tid(), 99, corridx, cpu_parent.get_entry(), "Correlation ID" ) ); //correlation ID counter at start of parent
    gpu_kern.set_GPU_correlationID_partner(cpu_parent.get_id());
    cpu_parent.set_GPU_correlationID_partner(gpu_kern.get_id());
  }
  void bindCPUparentGPUkernel(ExecData_t &cpu_parent, ExecData_t &gpu_kern){
    static unsigned long corridx_cnt = 0;
    bindCPUparentGPUkernel(cpu_parent, gpu_kern, corridx_cnt++);
  }


  /**
   * @brief Create an Event_t object from the inputs provided writing to array 'to' which must have a size at least COMM_EVENT_DIM. Index will be internally assigned, as will name
   */
  Event_t createCommEvent_t(unsigned long pid,
			    unsigned long rid,
			    unsigned long tid,
			    unsigned long eid,
			    unsigned long comm_tag,
			    unsigned long comm_partner,
			    unsigned long comm_bytes,
			    unsigned long ts,
			    unsigned long *to){
    static long event_idx_s = 0;
    long event_idx = event_idx_s++;

    eventID id(rid, 0, event_idx);

    to[IDX_P] = pid;
    to[IDX_R] = rid;
    to[IDX_T] = tid;
    to[IDX_E] = eid;
    to[COMM_IDX_TAG] = comm_tag;
    to[COMM_IDX_PARTNER] = comm_partner;
    to[COMM_IDX_BYTES] = comm_bytes;
    to[COMM_IDX_TS] = ts;

    return Event_t(to, EventDataType::COMM, event_idx, id);
  }



  /**
   * @brief Create an Event_t object from the inputs provided. Index will be internally assigned, as will name
   */
  Event_t createCommEvent_t(unsigned long pid,
			    unsigned long rid,
			    unsigned long tid,
			    unsigned long eid,
			    unsigned long comm_tag,
			    unsigned long comm_partner,
			    unsigned long comm_bytes,
			    unsigned long ts){
    static std::list< std::array<unsigned long, COMM_EVENT_DIM> > todelete; //make sure they get deleted eventually
    todelete.push_back( std::array<unsigned long, COMM_EVENT_DIM>());
    return createCommEvent_t(pid,rid,tid,eid,comm_tag,comm_partner,comm_bytes,ts,todelete.back().data());
  }

  /**
   * @brief Create a Commdata_t object
   * @param commType SEND or RECV
   */
  CommData_t createCommData_t(unsigned long pid,
			      unsigned long rid,
			      unsigned long tid,
			      unsigned long eid,
			      unsigned long comm_tag,
			      unsigned long comm_partner,
			      unsigned long comm_bytes,
			      unsigned long ts,
			      std::string commType){
    return CommData_t(createCommEvent_t(pid,rid,tid,eid,comm_tag,comm_partner,comm_bytes,ts), commType);
  }


  //A mock class that acts as the parameter server
  struct MockParameterServer{
    void* context;
    void* socket;

    //Handshake with the AD
    void start(Barrier &barrier2, const std::string &sinterface){
      context = zmq_ctx_new();
      socket = zmq_socket(context, ZMQ_REP);
      assert(zmq_bind(socket, sinterface.c_str()) == 0);
      barrier2.wait(); //wait until AD has been initialized

      zmq_pollitem_t items[1] =
	{
	  { socket, 0, ZMQ_POLLIN, 0 }
	};
      std::cout << "Mock PS start polling" << std::endl;
      zmq_poll(items, 1, -1);
      std::cout << "Mock PS has a message" << std::endl;

      //Receive the hello string from the AD
      std::string strmsg;
      {
	zmq_msg_t msg;
	int ret;
	zmq_msg_init(&msg);
	ret = zmq_msg_recv(&msg, socket, 0);
	strmsg.assign((char*)zmq_msg_data(&msg), ret);
	zmq_msg_close(&msg);
      }
      Message rmsg;
      rmsg.set_msg(strmsg,true);
      EXPECT_EQ(rmsg.buf(), "Hello!");

      std::cout << "Mock PS sending response" << std::endl;
      //Send a response back to the AD
      {
	Message msg_t;
	msg_t.set_msg(std::string("Hello!I am NET!"), false);
	strmsg = msg_t.data();

	zmq_msg_t msg;
	int ret;
	zmq_msg_init_size(&msg, strmsg.size());
	memcpy(zmq_msg_data(&msg), (const void*)strmsg.data(), strmsg.size());
	ret = zmq_msg_send(&msg, socket, 0);
	zmq_msg_close(&msg);
      }
    }


    //Act as a receiver for function stats sent by ADOutlier::update_local_statistics
    void receive_statistics(Barrier &barrier2, const std::string test_msg){
      zmq_pollitem_t items[1] =
	{
	  { socket, 0, ZMQ_POLLIN, 0 }
	};
      std::cout << "Mock PS start polling" << std::endl;
      zmq_poll(items, 1, -1);
      std::cout << "Mock PS has a message" << std::endl;

      //Receive the update string from the AD
      std::string strmsg;
      {
	zmq_msg_t msg;
	int ret;
	zmq_msg_init(&msg);
	ret = zmq_msg_recv(&msg, socket, 0);
	strmsg.assign((char*)zmq_msg_data(&msg), ret);
	zmq_msg_close(&msg);
      }
      std::cout << "Mock PS received string: " << strmsg << std::endl;

      std::stringstream ss;
      ss << "{\"Buffer\":\"" << test_msg << "\",\"Header\":{\"dst\":0,\"frame\":0,\"kind\":3,\"size\":" << test_msg.size() << ",\"src\":0,\"type\":1}}";
      EXPECT_EQ(strmsg, ss.str());

      std::cout << "Mock PS sending response" << std::endl;
      //Send a response back to the AD
      {
	Message msg_t;
	msg_t.set_msg(std::string(""), false); //apparently it doesn't expect the message to have content
	strmsg = msg_t.data();

	zmq_msg_t msg;
	int ret;
	zmq_msg_init_size(&msg, strmsg.size());
	memcpy(zmq_msg_data(&msg), (const void*)strmsg.data(), strmsg.size());
	ret = zmq_msg_send(&msg, socket, 0);
	zmq_msg_close(&msg);
      }
    }


    //Act as a receiver for function stats sent by ADOutlier::update_local_statistics
    void receive_counter_statistics(Barrier &barrier2, const std::string test_msg){
      zmq_pollitem_t items[1] =
	{
	  { socket, 0, ZMQ_POLLIN, 0 }
	};
      std::cout << "Mock PS start polling" << std::endl;
      zmq_poll(items, 1, -1);
      std::cout << "Mock PS has a message" << std::endl;

      //Receive the update string from the AD
      std::string strmsg;
      {
	zmq_msg_t msg;
	int ret;
	zmq_msg_init(&msg);
	ret = zmq_msg_recv(&msg, socket, 0);
	strmsg.assign((char*)zmq_msg_data(&msg), ret);
	zmq_msg_close(&msg);
      }
      std::cout << "Mock PS received string: " << strmsg << std::endl;

      Message rmsg;
      rmsg.set_msg(strmsg, true);
      EXPECT_EQ(rmsg.buf(), test_msg);
      EXPECT_EQ(rmsg.type(), REQ_ADD);
      EXPECT_EQ(rmsg.kind(), COUNTER_STATS);

      std::cout << "Mock PS sending response" << std::endl;
      //Send a response back to the AD
      {
	Message msg_t;
	msg_t.set_info(0,0,REP_ECHO,DEFAULT);
	msg_t.set_msg(std::string(""), false);
	strmsg = msg_t.data();

	zmq_msg_t msg;
	int ret;
	zmq_msg_init_size(&msg, strmsg.size());
	memcpy(zmq_msg_data(&msg), (const void*)strmsg.data(), strmsg.size());
	ret = zmq_msg_send(&msg, socket, 0);
	zmq_msg_close(&msg);
      }
    }

    void waitForDisconnect(){
      std::cout << "Mock PS is waiting for disconnect message" << std::endl;
      std::string strmsg;
      {
	zmq_msg_t msg;
	zmq_msg_init(&msg);
	int len = zmq_msg_recv(&msg, socket, 0);
	strmsg.assign((char*)zmq_msg_data(&msg), len);
	zmq_msg_close(&msg);
      }
      Message rmsg;
      rmsg.set_msg(strmsg, true);
      EXPECT_EQ(rmsg.buf(), "");
      EXPECT_EQ(rmsg.type(), REQ_QUIT);

      std::cout << "Mock PS received disconnect message, sending response" << std::endl;
      {
	Message msg_t;
	msg_t.set_info(0,0,REP_QUIT,DEFAULT);
	msg_t.set_msg(std::string(""), false);
	strmsg = msg_t.data();

	zmq_msg_t msg;
	int ret;
	zmq_msg_init_size(&msg, strmsg.size());
	memcpy(zmq_msg_data(&msg), (const void*)strmsg.data(), strmsg.size());
	ret = zmq_msg_send(&msg, socket, 0);
	zmq_msg_close(&msg);
      }
    }


    void end(){
      std::cout << "Mock PS is finalizing" << std::endl;
      zmq_close(socket);
      zmq_ctx_term(context);
    }


  };


  bool compare(const RunStats &a, const RunStats &b, const double tol = 1e-12){
    RunStats::RunStatsValues sa = a.get_stat_values(), sb = b.get_stat_values();
    bool ret = true;
#define COM(A) if(fabs( sa. A - sb. A ) > tol){ std::cout << #A << " " << sa. A << " " << sb. A << std::endl; ret = false; }
    COM(count);
    COM(minimum);
    COM(maximum);
    COM(accumulate);
    COM(mean);
    COM(stddev);
    COM(skewness);
    COM(kurtosis);
    return ret;
#undef COM
  }


}
