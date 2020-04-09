#include<chimbuko/ad/ADOutlier.hpp>
#include<chimbuko/param/sstd_param.hpp>
#include<chimbuko/message.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <random>

using namespace chimbuko;

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
  
  void end(){
    zmq_close(socket);
    zmq_ctx_term(context);
  }


};

//Derived class to allow access to protected member functions
class ADOutlierSSTDTest: public ADOutlierSSTD{
public:
  std::pair<size_t, size_t> sync_param_test(ParamInterface* param){ return this->ADOutlierSSTD::sync_param(param); }

  unsigned long compute_outliers_test(const unsigned long func_id, std::vector<CallListIterator_t>& data,
				      long& min_ts, long& max_ts){ return this->compute_outliers(func_id, data, min_ts, max_ts); }

  std::pair<size_t, size_t> update_local_statistics_test(std::string l_stats, int step){ return this->update_local_statistics(l_stats, step); }
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


TEST(ADOutlierTestComputeOutliersWithoutPS, Works){
  //Generate statistics
  std::default_random_engine gen;
  std::normal_distribution<double> dist(420.,10.);
  int N = 50;
  int func_id = 1234;
  SstdParam stats;
  RunStats &stats_r = stats[func_id];
  for(int i=0;i<N;i++) stats_r.push(dist(gen));

  ADOutlierSSTDTest outlier;
  outlier.sync_param_test(&stats);
  
  std::string stats_state = outlier.get_global_parameters()->serialize();

  std::cout << "Stats: " << stats_state << std::endl;
  
  //Generate some events with an outlier
  
  std::list<ExecData_t> call_list;  //aka CallList_t
  for(int i=0;i<N;i++){
    long val = i==N-1 ? 800 : long(dist(gen)); //outlier on N-1
    call_list.push_back( createFuncExecData_t(0,0,0,  func_id, "my_func", 1000*(i+1),val) );
    //std::cout << call_list.back().get_json().dump() << std::endl;
  }
  long ts_end = 1000*N + 800;
  
  
  std::vector<CallListIterator_t> call_list_its;
  for(CallListIterator_t it=call_list.begin(); it != call_list.end(); ++it)
    call_list_its.push_back(it);

  long min_ts, max_ts;
  unsigned long nout = outlier.compute_outliers_test(func_id, call_list_its, min_ts, max_ts);

  std::cout << "# outliers detected: " << nout << std::endl;
  std::cout << "min_ts " << min_ts << " max_ts " << max_ts << std::endl;

  EXPECT_EQ(nout, 1);
  EXPECT_EQ(min_ts, 1000);
  EXPECT_EQ(max_ts, ts_end);
}


TEST(ADOutlierTestRunWithoutPS, Works){
  //Generate statistics
  std::default_random_engine gen;
  std::normal_distribution<double> dist(420.,10.);
  int N = 50;
  int func_id = 1234;

  ADOutlierSSTDTest outlier;
  
  //Generate some events with an outlier
  
  std::list<ExecData_t> call_list;  //aka CallList_t
  for(int i=0;i<N;i++){
    long val = i==N-1 ? 800 : long(dist(gen)); //outlier on N-1
    call_list.push_back( createFuncExecData_t(0,0,0,  func_id, "my_func", 1000*(i+1),val) );
    //std::cout << call_list.back().get_json().dump() << std::endl;
  }
  long ts_end = 1000*N + 800;
  
  //typedef std::unordered_map<unsigned long, std::vector<CallListIterator_t>> ExecDataMap_t;
  ExecDataMap_t data_map;
  std::vector<CallListIterator_t> &call_list_its = data_map[func_id];
  for(CallListIterator_t it=call_list.begin(); it != call_list.end(); ++it)
    call_list_its.push_back(it);

  //run method generates statistics from input data map and merges with stored stats
  //thus including the outliers in the stats! Nevertheless with enough good events the stats shouldn't be poisoned too badly
  outlier.linkExecDataMap(&data_map);
  unsigned long nout = outlier.run(0);

  std::cout << "# outliers detected: " << nout << std::endl;

  EXPECT_EQ(nout, 1);
}

TEST(ADOutlierTestUpdateLocalStatisticsWithPS, Works){
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
		       std::cout << "PS thread waiting for stat update" << std::endl;
		       ps.receive_statistics(barrier2,"test");
		       barrier2.wait();
		       

		       barrier2.wait();		       
		       std::cout << "PS thread terminating connection" << std::endl;
		       ps.end();
		     });
		       
  std::thread out_thr([&]{
			barrier2.wait();
			try{
			  ADOutlierSSTDTest outlier;
			  outlier.connect_ps(0, 0, sname);
			  
			  barrier2.wait();
			  std::cout << "AD thread updating local stats" << std::endl;
			  outlier.update_local_statistics_test("test",0);
			  barrier2.wait();
			  
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












