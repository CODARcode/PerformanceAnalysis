#pragma once

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

namespace chimbuko{
  /**
   * @brief Create an Event_t object from the inputs provided. Index will be internally assigned, as will name
   */
  Event_t createFuncEvent_t(unsigned long pid,
				      unsigned long rid,
				      unsigned long tid,
				      unsigned long eid,
				      unsigned long func_id,
				      unsigned long ts){
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
  ExecData_t createFuncExecData_t(unsigned long pid,
					    unsigned long rid,
					    unsigned long tid,
					    unsigned long func_id,
					    const std::string &func_name,
					    long start,
					    long runtime){
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
  Event_t createCounterEvent_t(unsigned long pid,
					 unsigned long rid,
					 unsigned long tid,
					 unsigned long counter_id,
					 unsigned long value,
					 unsigned long ts){
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
    static size_t event_idx = -1;
    event_idx++;
    std::stringstream ss; ss << "comm_event" << event_idx;
  
    static std::list< std::array<unsigned long, COMM_EVENT_DIM> > todelete; //make sure they get deleted eventually
    std::array<unsigned long, COMM_EVENT_DIM> ev;
    ev[IDX_P] = pid;
    ev[IDX_R] = rid;
    ev[IDX_T] = tid;
    ev[IDX_E] = eid;
    ev[COMM_IDX_TAG] = comm_tag;
    ev[COMM_IDX_PARTNER] = comm_partner;
    ev[COMM_IDX_BYTES] = comm_bytes;
    ev[COMM_IDX_TS] = ts;
    todelete.push_back(ev);

    return Event_t(todelete.back().data(), EventDataType::COMM, event_idx, ss.str());
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
      EXPECT_EQ(strmsg, R"({"Buffer":"Hello!","Header":{"dst":0,"frame":0,"kind":0,"size":6,"src":0,"type":5}})");

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

      std::stringstream ss;
      ss << "{\"Buffer\":\"" << test_msg << "\",\"Header\":{\"dst\":0,\"frame\":0,\"kind\":4,\"size\":" << test_msg.size() << ",\"src\":0,\"type\":1}}";
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

    void end(){
      zmq_close(socket);
      zmq_ctx_term(context);
    }


  };


}
