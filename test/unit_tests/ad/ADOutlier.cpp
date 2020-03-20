#include<chimbuko/ad/ADOutlier.hpp>
#include<chimbuko/message.hpp>
#include "gtest/gtest.h"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>

using namespace chimbuko;



class Barrier {
public:
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



TEST(ADOutlierTestConnectPS, Connects){
#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)
  std::cout << "Using ZMQ net" << std::endl;

  Barrier barrier2(2);

  std::string sinterface = "tcp://*:5559";
  std::string sname = "tcp://localhost:5559";

  std::thread ps_thr([&]{
		       void* context = zmq_ctx_new();
		       void* socket = zmq_socket(context, ZMQ_REP);
		       assert(zmq_bind(socket, sinterface.c_str()) == 0);
		       barrier2.wait();
		       std::cout << "ps_thr advanced past barrier" << std::endl;

		       std::string strmsg;
		       {
			 zmq_msg_t msg;
			 int ret;
			 zmq_msg_init(&msg);
			 ret = zmq_msg_recv(&msg, socket, 0);
			 strmsg.assign((char*)zmq_msg_data(&msg), ret);
			 zmq_msg_close(&msg);
		       }
		       std::cout << "ps_thr received " << strmsg << std::endl;

		       {
			 Message msg_t;
			 msg_t.set_msg(std::string("Hello!I am ZMQNET!"), false);
			 strmsg = msg_t.data();
			 
			 zmq_msg_t msg;
			 int ret;
			 zmq_msg_init_size(&msg, strmsg.size());
			 memcpy(zmq_msg_data(&msg), (const void*)strmsg.data(), strmsg.size());
			 ret = zmq_msg_send(&msg, socket, 0);
			 zmq_msg_close(&msg);
		       }


		       
		     });
		       
  std::thread out_thr([&]{
			barrier2.wait();
			std::cout << "out_thr advanced past barrier" << std::endl;
			try{
			  ADOutlierSSTD outlier;
			  outlier.connect_ps(0, 0, sname);
			}catch(const std::exception &e){
			  std::cerr << e.what() << std::endl;
			}
		      });
  
  ps_thr.join();
  out_thr.join();
		       
  


  
#else
#error "Requires compiling with MPI or ZMQ net"
#endif

}


