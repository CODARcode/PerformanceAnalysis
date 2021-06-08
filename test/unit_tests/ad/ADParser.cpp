#include<chimbuko/ad/ADParser.hpp>
#include<chimbuko/pserver/PSglobalFunctionIndexMap.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <stdio.h>

using namespace chimbuko;

TEST(ADParserTestConstructor, opensTimesoutCorrectlySST){
  std::string filename = "commfile";
  bool got_err= false;
  try{
    ADParser parser(filename, 0,0, "SST",1); //2 second timeout
  }catch(const std::runtime_error& error){
    std::cout << "\nADParser (by design) threw the following error:\n" << error.what() << std::endl;
    got_err = true;
  }
  EXPECT_EQ( got_err, true );
}

struct SSTrw{
  //Common variables accessed by both writer and reader threads
  Barrier barrier; //a barrier for a fixed number of threads
  bool completed;
  std::mutex m;
  std::condition_variable cv;
  const std::string filename;
  

  //Writer (should be used only by writer thread)
  adios2::ADIOS ad;
  adios2::IO io;
  adios2::Engine wr;

  void openWriter(){
    barrier.wait(); 
    std::cout << "Writer thread initializing" << std::endl;
  
    ad = adios2::ADIOS(MPI_COMM_SELF, adios2::DebugON);
    io = ad.DeclareIO("tau-metrics");
    io.SetEngine("SST");
    io.SetParameters({
		      {"MarshalMethod", "BP"}
		      //,
		      //{"DataTransport", "RDMA"}
      });
  
    wr = io.Open(filename, adios2::Mode::Write);

    std::cout << "Writer thread init is completed" << std::endl;

    barrier.wait();
  }
  void closeWriter(){
    barrier.wait();
    
    std::cout << "Writer thread closing shop" << std::endl;
    wr.Close();
    
    std::cout << "Writer thread closed" << std::endl;
    
    barrier.wait();    
  }
  
  //Reader
  ADParser *parser;

  void openReader(const int pid = 0, const int rid = 0){
    barrier.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); //ADIOS2 seems to crash if we don't wait a short amount of time between starting the writer and starting the reader
    std::cout << "Parse thread initializing" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
    
    parser = new ADParser(filename, pid, rid, "SST");
    std::cout << "Parser initialized" << std::endl;
    
    barrier.wait();
  }
  void closeReader(){
    barrier.wait();
    std::cout << "Parser thread waiting to exit" << std::endl;
    
    barrier.wait(); //have to wait until after the writer has closed!
    
    std::cout << "Parser thread exiting" << std::endl;
    
    if(parser){ delete parser; parser = NULL; }
  }
  
  SSTrw(int nbarrier = 2): barrier(nbarrier), completed(false), parser(NULL), filename("commFile"){}
};


TEST(ADParserTestConstructor, opensCorrectlyBPFile){
  bool err = false;
  try{
    std::string filename = "commFile";   
    {
      adios2::ADIOS ad = adios2::ADIOS(MPI_COMM_SELF, adios2::DebugON);
      adios2::IO io = ad.DeclareIO("tau-metrics");
      io.SetEngine("BPFile");
      io.SetParameters({
	  {"MarshalMethod", "BP"}
	});
      
      adios2::Engine wr = io.Open(filename, adios2::Mode::Write);
      wr.Close();
    }
    ADParser parser(filename, 0,0, "BPFile");    
  }catch(std::exception &e){
    std::cout << "Caught exception:\n" << e.what() << std::endl;
    err = true;
  }
  EXPECT_EQ(err, false);
}



TEST(ADParserTestConstructor, opensCorrectlySST){
  bool err = false;
  try{
    SSTrw rw;
    
    std::thread wthr([&](){
		       rw.openWriter();
		       rw.closeWriter();
		     });
    
    std::thread rthr([&](){
		       rw.openReader();
		       rw.closeReader();
		     });		     
    
    rthr.join();
    wthr.join();
  }catch(std::exception &e){
    std::cout << "Caught exception:\n" << e.what() << std::endl;
    err = true;
  }
  EXPECT_EQ(err, false);
}


TEST(ADParserTestbeginStep, stepStartEndOK){
  bool err = false;
  int step_idx;
  try{
    SSTrw rw;

    std::thread wthr([&](){
		       rw.openWriter();
		       rw.wr.BeginStep();
		       rw.wr.EndStep();
		       rw.closeWriter();
		     });

    std::thread rthr([&](){
		       rw.openReader();
		       step_idx = rw.parser->beginStep(true);
		       rw.parser->endStep();
		       rw.closeReader();
		     });
  
    rthr.join();
    wthr.join();
  }catch(std::exception &e){
    std::cout << "Caught exception:\n" << e.what() << std::endl;
    err = true;
  }
  EXPECT_EQ(err, false);
  EXPECT_EQ(step_idx, 0);
}



TEST(ADParserTestAttributeIO, funcAttributeCommunicated){
  bool err = false;

  try{
    SSTrw rw;

    std::thread wthr([&](){
		       rw.openWriter();

		       auto var = rw.io.DefineVariable<int>("dummy");  //it seems at least 1 variable must be Put for attributes to be communicated properly
		                                                       //testing suggests it does not require a corresponding get.  Bug report?

		       int dummy = 5678;
		       
		       rw.wr.BeginStep();
		       rw.wr.Put(var, &dummy);			 
		       rw.io.DefineAttribute<std::string>("timer 1234", "my_function");
		       rw.wr.EndStep();

		       rw.barrier.wait();
		       
		       rw.closeWriter();
		     });
    
    rw.openReader();
    
    rw.parser->beginStep();    
    rw.parser->endStep();

    rw.parser->update_attributes(); //can be called during begin/end or after
    
    const std::unordered_map<int, std::string>* fmap = rw.parser->getFuncMap();
    EXPECT_EQ(fmap->size(), 1);
    auto it = fmap->find(1234);
    EXPECT_NE(it, fmap->end());
    if(it != fmap->end()){
      EXPECT_EQ(it->second, "my_function");
    }

    rw.barrier.wait();
    
    rw.closeReader();
  
    wthr.join();
  }catch(std::exception &e){
    std::cout << "Caught exception:\n" << e.what() << std::endl;
    err = true;
  }
  EXPECT_EQ(err, false);
}



TEST(ADParserTestAttributeIO, eventAttributeCommunicated){
  bool err = false;

  try{
    SSTrw rw;

    std::thread wthr([&](){
		       rw.openWriter();

		       auto var = rw.io.DefineVariable<int>("dummy");  //it seems at least 1 variable must be Put for attributes to be communicated properly
		                                                       //testing suggests it does not require a corresponding get.  Bug report?

		       int dummy = 5678;
		       
		       rw.wr.BeginStep();
		       rw.wr.Put(var, &dummy);			 
		       rw.io.DefineAttribute<std::string>("event_type 1234", "my_event");
		       rw.wr.EndStep();

		       rw.barrier.wait();
		       
		       rw.closeWriter();
		     });
    
    rw.openReader();
    
    rw.parser->beginStep();    
    rw.parser->endStep();

    rw.parser->update_attributes(); //can be called during begin/end or after
    
    const std::unordered_map<int, std::string>* fmap = rw.parser->getEventType();
    EXPECT_EQ(fmap->size(), 1);
    auto it = fmap->find(1234);
    EXPECT_NE(it, fmap->end());
    if(it != fmap->end()){
      EXPECT_EQ(it->second, "my_event");
    }

    rw.barrier.wait();
    
    rw.closeReader();
  
    wthr.join();
  }catch(std::exception &e){
    std::cout << "Caught exception:\n" << e.what() << std::endl;
    err = true;
  }
  EXPECT_EQ(err, false);
}






TEST(ADParserTestFuncDataIO, funcDataCommunicated){
  bool err = false;

  try{
    SSTrw rw;

    std::thread wthr([&](){
		       unsigned long data[FUNC_EVENT_DIM];
		       for(int i=0;i<FUNC_EVENT_DIM;i++) data[i] = i;
		       size_t count_val = 1;
		       
		       rw.openWriter();
		    		       
		       auto count = rw.io.DefineVariable<size_t>("timer_event_count");
		       auto timestamps = rw.io.DefineVariable<unsigned long>("event_timestamps", {1, FUNC_EVENT_DIM}, {0, 0}, {1, FUNC_EVENT_DIM});

		       rw.wr.BeginStep();
		       rw.wr.Put(count, &count_val);			 
		       rw.wr.Put(timestamps, data);
		       
		       rw.wr.EndStep();

		       rw.barrier.wait();
		       
		       rw.closeWriter();
		     });
    
    rw.openReader();
    
    rw.parser->beginStep();
    ParserError perr = rw.parser->fetchFuncData();    
    EXPECT_EQ(perr , chimbuko::ParserError::OK );
    rw.parser->endStep();

    EXPECT_EQ(rw.parser->getNumFuncData(), 1 );

    unsigned long expect_data[FUNC_EVENT_DIM]; for(int i=0;i<FUNC_EVENT_DIM;i++) expect_data[i] = i;
    const unsigned long* rdata = rw.parser->getFuncData(0);
    EXPECT_TRUE( 0 == std::memcmp( expect_data, rdata, FUNC_EVENT_DIM*sizeof(unsigned long) ) );
    
    rw.barrier.wait();
    
    rw.closeReader();
  
    wthr.join();
  }catch(std::exception &e){
    std::cout << "Caught exception:\n" << e.what() << std::endl;
    err = true;
  }
  EXPECT_EQ(err, false);
}





//Check the program index is correctly substituted for all data types
TEST(ADParserTestFuncDataIO, checkProgramIndexSubstitution){
  bool err = false;

  try{
    SSTrw rw;

    std::thread wthr([&](){
		       unsigned long func_data[FUNC_EVENT_DIM];
		       for(int i=0;i<FUNC_EVENT_DIM;i++) func_data[i] = i;
		       func_data[IDX_P] = 0;
		       size_t func_count_val = 1;

		       unsigned long comm_data[COMM_EVENT_DIM];
		       for(int i=0;i<COMM_EVENT_DIM;i++) comm_data[i] = i;
		       comm_data[IDX_P] = 0;
		       size_t comm_count_val = 1;

		       unsigned long counter_data[COUNTER_EVENT_DIM];
		       for(int i=0;i<COUNTER_EVENT_DIM;i++) counter_data[i] = i;
		       counter_data[IDX_P] = 0;
		       size_t counter_count_val = 1;
		       
		       rw.openWriter();

		       //Func
		       auto func_count = rw.io.DefineVariable<size_t>("timer_event_count");
		       auto func_timestamps = rw.io.DefineVariable<unsigned long>("event_timestamps", {1, FUNC_EVENT_DIM}, {0, 0}, {1, FUNC_EVENT_DIM});

		       //Comm
		       auto comm_count = rw.io.DefineVariable<size_t>("comm_count");
		       auto comm_timestamps = rw.io.DefineVariable<unsigned long>("comm_timestamps", {1, COMM_EVENT_DIM}, {0, 0}, {1, COMM_EVENT_DIM});

		       //Counter
		       auto counter_count = rw.io.DefineVariable<size_t>("counter_event_count");
		       auto counter_timestamps = rw.io.DefineVariable<unsigned long>("counter_values", {1, COUNTER_EVENT_DIM}, {0, 0}, {1, COUNTER_EVENT_DIM});


		       rw.wr.BeginStep();
		       rw.wr.Put(func_count, &func_count_val);			 
		       rw.wr.Put(func_timestamps, func_data);

		       rw.wr.Put(comm_count, &comm_count_val);			 
		       rw.wr.Put(comm_timestamps, comm_data);

		       rw.wr.Put(counter_count, &counter_count_val);			 
		       rw.wr.Put(counter_timestamps, counter_data);
		       
		       rw.wr.EndStep();

		       rw.barrier.wait();
		       
		       rw.closeWriter();
		     });

    int pid = 199;
    rw.openReader(pid);
    
    rw.parser->beginStep();

    //Check func
    {
      ParserError perr = rw.parser->fetchFuncData();    
      EXPECT_EQ(perr , chimbuko::ParserError::OK );
      EXPECT_EQ(rw.parser->getNumFuncData(), 1 );
      
      unsigned long expect_data[FUNC_EVENT_DIM]; for(int i=0;i<FUNC_EVENT_DIM;i++) expect_data[i] = i;
      expect_data[IDX_P] = pid;
      const unsigned long* rdata = rw.parser->getFuncData(0);
      std::cout << "Func data expect pid " << pid << " got " << rdata[IDX_P] << std::endl;

      EXPECT_TRUE( 0 == std::memcmp( expect_data, rdata, FUNC_EVENT_DIM*sizeof(unsigned long) ) );    
    }

    //Check comm
    {
      ParserError perr = rw.parser->fetchCommData();    
      EXPECT_EQ(perr , chimbuko::ParserError::OK );
      EXPECT_EQ(rw.parser->getNumCommData(), 1 );
      
      unsigned long expect_data[COMM_EVENT_DIM]; for(int i=0;i<COMM_EVENT_DIM;i++) expect_data[i] = i;
      expect_data[IDX_P] = pid;
      const unsigned long* rdata = rw.parser->getCommData(0);
      std::cout << "Comm data expect pid " << pid << " got " << rdata[IDX_P] << std::endl;

      EXPECT_TRUE( 0 == std::memcmp( expect_data, rdata, COMM_EVENT_DIM*sizeof(unsigned long) ) );    
    }

    //Check counter
    {
      ParserError perr = rw.parser->fetchCounterData();    
      EXPECT_EQ(perr , chimbuko::ParserError::OK );     
      EXPECT_EQ(rw.parser->getNumCounterData(), 1 );
      
      unsigned long expect_data[COUNTER_EVENT_DIM]; for(int i=0;i<COUNTER_EVENT_DIM;i++) expect_data[i] = i;
      expect_data[IDX_P] = pid;
      const unsigned long* rdata = rw.parser->getCounterData(0);
      std::cout << "Counter data expect pid " << pid << " got " << rdata[IDX_P] << std::endl;

      EXPECT_TRUE( 0 == std::memcmp( expect_data, rdata, COUNTER_EVENT_DIM*sizeof(unsigned long) ) );    
    }

    rw.parser->endStep();

    rw.barrier.wait();
    
    rw.closeReader();
  
    wthr.join();
  }catch(std::exception &e){
    std::cout << "Caught exception:\n" << e.what() << std::endl;
    err = true;
  }
  EXPECT_EQ(err, false);
}









//The pserver stores a global map of function name to a global index
//this is synchronized to a local object that maintains a mapping of local to global index
TEST(ADParserTestFuncDataIO, funcDataLocalToGlobalIndexReplacementWorks){
  unsigned long local_idx = 1234;
  unsigned long global_idx = 0; //this is the value always assigned to the first function
  std::string func_name = "my_function";
  
  std::string sname = "tcp://localhost:5559"; //ip of "pserver"

  int argc; char** argv = nullptr;
  SSTrw rw;
  
  Barrier ps_barrier(2);

  //This thread will own the "pserver" instance
  std::thread psthr([&](){
      //The "pserver"
      std::cout << "TEST: Writer thread initializing pserver" << std::endl;
      PSglobalFunctionIndexMap glob_map;
      ZMQNet ps;
      ps.add_payload(new NetPayloadGlobalFunctionIndexMapBatched(&glob_map));
      ps.init(&argc, &argv, 1); //1 worker
      ps.run(".");

      std::cout << "PS thread waiting at barrier" << std::endl;
      ps_barrier.wait(); //main thread should disconnect before ps is finalized
      std::cout << "PS thread terminating connection" << std::endl;
      ps.finalize();
    });


  ADThreadNetClient net_client;
  net_client.connect_ps(0, 0, sname);
  std::cout << "TEST: Net client has connected to pserver" << std::endl;


  //This is the ADIOS2 writer thread
  std::thread wthr([&](){
      //The ADIOS2 writer
      std::cout << "TEST: Writer thread opening ADIOS2 writer" << std::endl;
      rw.openWriter();
      rw.wr.BeginStep();
		       
      //Pass the map of local index to func name
      rw.io.DefineAttribute<std::string>("timer " + anyToStr(local_idx), func_name);

      //Setup a data packet
      unsigned long data[FUNC_EVENT_DIM];
      for(int i=0;i<FUNC_EVENT_DIM;i++) data[i] = i;
      size_t count_val = 1;
      data[FUNC_IDX_F] = local_idx;
		    		       
      auto count = rw.io.DefineVariable<size_t>("timer_event_count");
      auto timestamps = rw.io.DefineVariable<unsigned long>("event_timestamps", {1, FUNC_EVENT_DIM}, {0, 0}, {1, FUNC_EVENT_DIM});

      rw.wr.Put(count, &count_val);			 
      rw.wr.Put(timestamps, data);
		       
      rw.wr.EndStep();

      std::cout << "TEST: Writer thread waiting at barrier for completion" << std::endl;
      rw.barrier.wait();
		       
      rw.closeWriter();
    });
  

  std::cout << "TEST: Opening SST reader" << std::endl;
  rw.openReader();
  std::cout << "TEST: Link net client" << std::endl;
  rw.parser->linkNetClient(&net_client);

  std::cout << "TEST: Beginning step" << std::endl;
  rw.parser->beginStep();
  std::cout << "TEST: Updating attributes" << std::endl;
  rw.parser->update_attributes();
  std::cout << "TEST: Updating func data" << std::endl;
  rw.parser->fetchFuncData();
  std::cout << "TEST: Ending step" << std::endl;
  rw.parser->endStep();
 
  std::cout << "TEST: Checking results" << std::endl;
  EXPECT_EQ(rw.parser->getGlobalFunctionIndex(local_idx), global_idx);

  auto it = rw.parser->getFuncMap()->find(global_idx); //check the function index -> name map uses the global index
  EXPECT_NE(it, rw.parser->getFuncMap()->end());
  EXPECT_EQ(it->second, func_name);

  const unsigned long* dp = rw.parser->getFuncData(0); //check the function index has been replaced in the data
  EXPECT_NE(dp, nullptr);
  EXPECT_EQ(dp[FUNC_IDX_F], global_idx);

  std::cout << "TEST: Main thread waiting at barrier for writer thread completion" << std::endl;
  rw.barrier.wait();  
  rw.closeReader();
  wthr.join();

  net_client.disconnect_ps();
  std::cout << "TEST syncing with pserver thread for ps finalize" << std::endl;
  ps_barrier.wait();
  psthr.join();
}










//Events same up to id string
bool same_up_to_id_string(const Event_t &l, const Event_t &r){
  if(r.type() != l.type() || r.idx() != l.idx()) return false;
  int len = l.get_data_len();
  for(int i=0;i<len;i++) if(l.get_ptr()[i] != r.get_ptr()[i]) return false;
  return true;
}


TEST(ADParserTest, eventsOrderedCorrectly){
  std::unordered_map<int, std::string> event_types = { {0,"ENTRY"}, {1,"EXIT"}, {2,"SEND"}, {3,"RECV"} };
  std::unordered_map<int, std::string> func_names = { {0,"MYFUNC"} };
  std::unordered_map<int, std::string> counter_names = { {0,"MYCOUNTER"} };


  int ENTRY = 0;
  int EXIT = 1;
  int SEND = 2;
  int RECV = 3;
  int MYCOUNTER = 0;
  int MYFUNC = 0;

  int pid=0, tid=0, rid=0;

  std::vector<Event_t> events = {
    createCounterEvent_t(pid, rid, tid, MYCOUNTER, 1234, 100),
    createFuncEvent_t(pid, rid, tid, ENTRY, MYFUNC, 110),
    createCommEvent_t(pid, rid, tid, SEND, 0, 99, 1024, 110), //Same time as entry. Should be included in function. For this to happen it has to appear in the ordering after func entry
    createCounterEvent_t(pid, rid, tid, MYCOUNTER, 1256, 130),
    createCounterEvent_t(pid, rid, tid, MYCOUNTER, 1267, 140),
    createCommEvent_t(pid, rid, tid, SEND, 1, 99, 1024, 150),
    createCommEvent_t(pid, rid, tid, RECV, 2, 99, 1024, 160),
    createCounterEvent_t(pid, rid, tid+1, MYCOUNTER, 1267, 170),  //not on same thread
    createCommEvent_t(pid, rid, tid, SEND, 3, 99, 1024, 180), //Same time as exit. Should be included in function. For this to happen it has to appear in the ordering before func exit
    createFuncEvent_t(pid, rid, tid, EXIT, MYFUNC, 180)
  };

  ADParser parser("",0,rid,"BPFile");
  parser.setFuncDataCapacity(100);
  parser.setCommDataCapacity(100);
  parser.setCounterDataCapacity(100);
  parser.setFuncMap(func_names);
  parser.setEventTypeMap(event_types);
  parser.setCounterMap(counter_names);

  for(int i=0;i<events.size();i++){
    if(events[i].type() == EventDataType::FUNC)
      parser.addFuncData(events[i].get_ptr());
    else if(events[i].type() == EventDataType::COMM)
      parser.addCommData(events[i].get_ptr());
    else if(events[i].type() == EventDataType::COUNT)
      parser.addCounterData(events[i].get_ptr());
    else
      FAIL() << "Invalid EventDataType";
  }
  
  EXPECT_EQ(parser.getNumFuncData(), 2);
  EXPECT_EQ(parser.getNumCommData(), 4);
  EXPECT_EQ(parser.getNumCounterData(), 4);

  std::vector<Event_t> events_out = parser.getEvents();
  
  EXPECT_EQ(events_out.size(), events.size());

  for(int i=0;i<events_out.size();i++){
    //Note, the event id strings will differ because the step index in the parser has not been set and so will default to -1
    std::cout << i << " " << events[i].get_json().dump() << " " << events_out[i].get_json().dump() << std::endl;
    EXPECT_EQ(same_up_to_id_string(events[i], events_out[i]), true);
  }
  

}


//Check the optional override of the rank index in the data is working
TEST(ADParserTestFuncDataIO, checkRankOverride){
  bool err = false;

  try{
    SSTrw rw;

    std::thread wthr([&](){
		       unsigned long func_data[FUNC_EVENT_DIM];
		       for(int i=0;i<FUNC_EVENT_DIM;i++) func_data[i] = i;
		       func_data[IDX_R] = 0;
		       size_t func_count_val = 1;

		       unsigned long comm_data[COMM_EVENT_DIM];
		       for(int i=0;i<COMM_EVENT_DIM;i++) comm_data[i] = i;
		       comm_data[IDX_R] = 0;
		       size_t comm_count_val = 1;

		       unsigned long counter_data[COUNTER_EVENT_DIM];
		       for(int i=0;i<COUNTER_EVENT_DIM;i++) counter_data[i] = i;
		       counter_data[IDX_R] = 0;
		       size_t counter_count_val = 1;
		       
		       rw.openWriter();

		       //Func
		       auto func_count = rw.io.DefineVariable<size_t>("timer_event_count");
		       auto func_timestamps = rw.io.DefineVariable<unsigned long>("event_timestamps", {1, FUNC_EVENT_DIM}, {0, 0}, {1, FUNC_EVENT_DIM});

		       //Comm
		       auto comm_count = rw.io.DefineVariable<size_t>("comm_count");
		       auto comm_timestamps = rw.io.DefineVariable<unsigned long>("comm_timestamps", {1, COMM_EVENT_DIM}, {0, 0}, {1, COMM_EVENT_DIM});

		       //Counter
		       auto counter_count = rw.io.DefineVariable<size_t>("counter_event_count");
		       auto counter_timestamps = rw.io.DefineVariable<unsigned long>("counter_values", {1, COUNTER_EVENT_DIM}, {0, 0}, {1, COUNTER_EVENT_DIM});


		       rw.wr.BeginStep();
		       rw.wr.Put(func_count, &func_count_val);			 
		       rw.wr.Put(func_timestamps, func_data);

		       rw.wr.Put(comm_count, &comm_count_val);			 
		       rw.wr.Put(comm_timestamps, comm_data);

		       rw.wr.Put(counter_count, &counter_count_val);			 
		       rw.wr.Put(counter_timestamps, counter_data);
		       
		       rw.wr.EndStep();

		       rw.barrier.wait();
		       
		       rw.closeWriter();
		     });

    int pid = 199;
    int rid = 887;
    rw.openReader(pid, rid);
    rw.parser->setDataRankOverride(true); //enable the rank override
    rw.parser->beginStep();

    //Check func
    {
      ParserError perr = rw.parser->fetchFuncData();    
      EXPECT_EQ(perr , chimbuko::ParserError::OK );
      EXPECT_EQ(rw.parser->getNumFuncData(), 1 );
      
      unsigned long expect_data[FUNC_EVENT_DIM]; for(int i=0;i<FUNC_EVENT_DIM;i++) expect_data[i] = i;
      expect_data[IDX_P] = pid;
      expect_data[IDX_R] = rid;
      const unsigned long* rdata = rw.parser->getFuncData(0);
      std::cout << "Func data expect rid " << rid << " got " << rdata[IDX_R] << std::endl;

      EXPECT_TRUE( 0 == std::memcmp( expect_data, rdata, FUNC_EVENT_DIM*sizeof(unsigned long) ) );    
    }

    //Check comm
    {
      ParserError perr = rw.parser->fetchCommData();    
      EXPECT_EQ(perr , chimbuko::ParserError::OK );
      EXPECT_EQ(rw.parser->getNumCommData(), 1 );
      
      unsigned long expect_data[COMM_EVENT_DIM]; for(int i=0;i<COMM_EVENT_DIM;i++) expect_data[i] = i;
      expect_data[IDX_P] = pid;
      expect_data[IDX_R] = rid;
      const unsigned long* rdata = rw.parser->getCommData(0);
      std::cout << "Comm data expect rid " << rid << " got " << rdata[IDX_R] << std::endl;

      EXPECT_TRUE( 0 == std::memcmp( expect_data, rdata, COMM_EVENT_DIM*sizeof(unsigned long) ) );    
    }

    //Check counter
    {
      ParserError perr = rw.parser->fetchCounterData();    
      EXPECT_EQ(perr , chimbuko::ParserError::OK );     
      EXPECT_EQ(rw.parser->getNumCounterData(), 1 );
      
      unsigned long expect_data[COUNTER_EVENT_DIM]; for(int i=0;i<COUNTER_EVENT_DIM;i++) expect_data[i] = i;
      expect_data[IDX_P] = pid;
      expect_data[IDX_R] = rid;
      const unsigned long* rdata = rw.parser->getCounterData(0);
      std::cout << "Counter data expect rid " << rid << " got " << rdata[IDX_R] << std::endl;

      EXPECT_TRUE( 0 == std::memcmp( expect_data, rdata, COUNTER_EVENT_DIM*sizeof(unsigned long) ) );    
    }

    rw.parser->endStep();

    rw.barrier.wait();
    
    rw.closeReader();
  
    wthr.join();
  }catch(std::exception &e){
    std::cout << "Caught exception:\n" << e.what() << std::endl;
    err = true;
  }
  EXPECT_EQ(err, false);
}
