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

//A mock class that acts as the parameter server
struct MockParameterServer{
  void* context;
  void* socket;
  
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
      msg_t.set_msg(std::string("Hello!I am ZMQNET!"), false);
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



TEST(ADOutlierTestConnectPS, ConnectsMock){
#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)
  std::cout << "Using ZMQ net" << std::endl;

  Barrier barrier2(2);

  std::string sinterface = "tcp://*:5559";
  std::string sname = "tcp://localhost:5559";

  std::thread ps_thr([&]{
		       MockParameterServer ps;
		       ps.start(barrier2, sinterface);
		       std::cout << "PS thread waiting at barrier" << std::endl;
		       barrier2.wait();
		       std::cout << "PS thread terminating connection" << std::endl;
		       ps.end();
		     });
		       
  std::thread out_thr([&]{
			barrier2.wait();
			try{
			  ADOutlierSSTD outlier;
			  outlier.connect_ps(0, 0, sname);
			  std::cout << "AD thread terminating connection" << std::endl;
			  outlier.disconnect_ps();
			  std::cout << "AD thread waiting at barrier" << std::endl;
			  barrier2.wait();			  
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



TEST(ADOutlierTestConnectPS, ConnectsZMQnet){
#ifdef _USE_MPINET
#warning "Testing with MPINET not available"
#elif defined(_USE_ZMQNET)
  std::cout << "Using ZMQ net" << std::endl;

  Barrier barrier2(2);

  std::string sinterface = "tcp://*:5559";
  std::string sname = "tcp://localhost:5559";

  int argc; char** argv = nullptr;
  std::cout << "Initializing PS thread" << std::endl;
  std::thread ps_thr([&]{
  		       ZMQNet ps;
  		       ps.init(&argc, &argv, 4); //4 workers
  		       ps.run(".");
		       std::cout << "PS thread waiting at barrier" << std::endl;
		       barrier2.wait();
		       std::cout << "PS thread terminating connection" << std::endl;
		       ps.finalize();
  		     });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::cout << "Initializing AD thread" << std::endl;
  std::thread out_thr([&]{
			try{
			  ADOutlierSSTD outlier;
			  outlier.connect_ps(0, 0, sname);
			  std::cout << "AD thread terminating connection" << std::endl;
			  outlier.disconnect_ps();
			  std::cout << "AD thread waiting at barrier" << std::endl;
			  barrier2.wait();
			}catch(const std::exception &e){
			  std::cerr << e.what() << std::endl;
			}
			//barrier2.wait();
		      });
  
  ps_thr.join();
  out_thr.join();
		         
#else
#error "Requires compiling with MPI or ZMQ net"
#endif

}

