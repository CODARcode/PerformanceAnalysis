#include<chimbuko/ad/ADOutlier.hpp>
#include<chimbuko/param/sstd_param.hpp>
#include<chimbuko/message.hpp>
#include "gtest/gtest.h"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <random>

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



class ADOutlierSSTDTest: public ADOutlierSSTD{
public:
  std::pair<size_t, size_t> sync_param_test(ParamInterface* param){ return this->ADOutlierSSTD::sync_param(param); }
};

TEST(ADOutlierTestSyncParamWithoutPS, Works){  
  SstdParam local_params_ps;

  std::default_random_engine gen;
  std::normal_distribution<double> dist(500.,100.);
  int N = 50;

  std::unordered_map<unsigned long, RunStats> local_params_ps_in;
  {
    RunStats &r = local_params_ps_in[0];
    for(int i=0;i<N;i++) r.push(dist(gen));
  }
  local_params_ps.assign(local_params_ps_in);

  std::cout << local_params_ps_in[0].get_json().dump();
    
  ADOutlierSSTDTest outlier;
  outlier.sync_param_test(&local_params_ps);
  
  //internal copy should be equal to global copy
  std::string in_state = outlier.get_global_parameters()->serialize();
  
  EXPECT_EQ(local_params_ps.serialize(), in_state);
}



TEST(ADOutlierTestSyncParamWithPS, Works){  
  SstdParam global_params_ps; //parameters held in the parameter server
  SstdParam local_params_ad; //parameters collected by AD

  std::default_random_engine gen;
  std::normal_distribution<double> dist(500.,100.);
  int N = 50;
  
  {
    RunStats &r = global_params_ps[0];
    for(int i=0;i<N;i++) r.push(dist(gen));
  }

  {
    RunStats &r = local_params_ad[0];
    for(int i=0;i<N;i++) r.push(dist(gen));
  }

  std::cout << global_params_ps[0].get_json().dump();
  std::cout << local_params_ad[0].get_json().dump();

  SstdParam combined_params_ps; //what we expect
  combined_params_ps.assign(global_params_ps.get_runstats());
  combined_params_ps.update(local_params_ad.get_runstats());


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
		       ps.set_parameter(&global_params_ps);
  		       ps.init(&argc, &argv, 4); //4 workers
  		       ps.run(".");
		       std::cout << "PS thread waiting at barrier" << std::endl;
		       barrier2.wait();
		       std::cout << "PS thread terminating connection" << std::endl;
		       ps.finalize();
  		     });
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::string glob_params_comb_ad;
  std::cout << "Initializing AD thread" << std::endl;
  std::thread out_thr([&]{
			try{
			  ADOutlierSSTDTest outlier;
			  outlier.connect_ps(0, 0, sname);

			  outlier.sync_param_test(&local_params_ad); //add local to global in PS and return to AD
			  glob_params_comb_ad  = outlier.get_global_parameters()->serialize();
			  
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

  EXPECT_EQ(glob_params_comb_ad, combined_params_ps.serialize());
  
#else
#error "Requires compiling with MPI or ZMQ net"
#endif
}
