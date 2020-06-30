#include<chimbuko/ad/ADParser.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cstring>

using namespace chimbuko;

TEST(ADParserTestConstructor, opensTimesoutCorrectlySST){
  std::string filename = "commfile";
  bool got_err= false;
  try{
    ADParser parser(filename, "SST",1); //2 second timeout
  }catch(const std::runtime_error& error){
    std::cout << "\nADParser (by design) threw the following error:\n" << error.what() << std::endl;
    got_err = true;
  }
  EXPECT_EQ( got_err, true );
}

struct SSTrw{
  //Common variables accessed by both threads
  Barrier barrier2; //a barrier for 2 threads
  bool completed;
  std::mutex m;
  std::condition_variable cv;
  const std::string filename;
  

  //Writer (should be used only by writer thread)
  adios2::ADIOS ad;
  adios2::IO io;
  adios2::Engine wr;

  void openWriter(){
    barrier2.wait(); 
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

    barrier2.wait();
  }
  void closeWriter(){
    barrier2.wait();
    
    std::cout << "Writer thread closing shop" << std::endl;
    wr.Close();
    
    std::cout << "Writer thread closed" << std::endl;
    
    barrier2.wait();    
  }
  
  //Reader
  ADParser *parser;

  void openReader(){
    barrier2.wait();
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); //ADIOS2 seems to crash if we don't wait a short amount of time between starting the writer and starting the reader
    std::cout << "Parse thread initializing" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); 
    
    parser = new ADParser(filename, "SST");
    std::cout << "Parser initialized" << std::endl;
    
    barrier2.wait();
  }
  void closeReader(){
    barrier2.wait();
    std::cout << "Parser thread waiting to exit" << std::endl;
    
    barrier2.wait(); //have to wait until after the writer has closed!
    
    std::cout << "Parser thread exiting" << std::endl;
    
    if(parser){ delete parser; parser = NULL; }
  }
  
  SSTrw(): barrier2(2), completed(false), parser(NULL), filename("commFile"){}
};


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

		       rw.barrier2.wait();
		       
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

    rw.barrier2.wait();
    
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

		       rw.barrier2.wait();
		       
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

    rw.barrier2.wait();
    
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

		       rw.barrier2.wait();
		       
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
    
    rw.barrier2.wait();
    
    rw.closeReader();
  
    wthr.join();
  }catch(std::exception &e){
    std::cout << "Caught exception:\n" << e.what() << std::endl;
    err = true;
  }
  EXPECT_EQ(err, false);
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
    createCommEvent_t(pid, rid, tid, SEND, 0, 99, 1024, 110),
    createFuncEvent_t(pid, rid, tid, ENTRY, MYFUNC, 120),
    createCounterEvent_t(pid, rid, tid, MYCOUNTER, 1256, 130),
    createCounterEvent_t(pid, rid, tid, MYCOUNTER, 1267, 140),
    createCommEvent_t(pid, rid, tid, SEND, 1, 99, 1024, 150),
    createCommEvent_t(pid, rid, tid, RECV, 2, 99, 1024, 160),
    createCounterEvent_t(pid, rid, tid+1, MYCOUNTER, 1267, 170),  //not on same thread
    createFuncEvent_t(pid, rid, tid, EXIT, MYFUNC, 180),
    createCommEvent_t(pid, rid, tid, SEND, 3, 99, 1024, 190)
  };

  ADParser parser("","BPFile");
  parser.setFuncDataCapacity(100);
  parser.setCommDataCapacity(100);
  parser.setCounterDataCapacity(100);

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

  std::vector<Event_t> events_out = parser.getEvents(rid);
  
  EXPECT_EQ(events_out.size(), events.size());

  for(int i=0;i<events_out.size();i++)
    EXPECT_EQ(same_up_to_id_string(events[i], events_out[i]), true);
  

}
