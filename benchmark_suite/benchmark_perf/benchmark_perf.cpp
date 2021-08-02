#include<mpi.h>
#include<chimbuko_config.h>
#include "gtest/gtest.h"
#include<unit_test_common.hpp>
#include<chimbuko/verbose.hpp>
#include<chimbuko/util/time.hpp>
#include<chimbuko/ad/ADParser.hpp>
#include<chimbuko/util/commandLineParser.hpp>

using namespace chimbuko;

void benchmark_ADParserget_events(int tries, int rank, int nthread, int nexec_per_thread){
  ADParser parser("", 0, rank);
  std::unordered_map<int, std::string> funcs = { {0,"my_func"} };
  std::unordered_map<int, std::string> event_types = { {0,"ENTRY"}, {1,"EXIT"}, {2,"SEND"}, {3,"RECV"} };
  std::unordered_map<int, std::string> counters = { {0,"my_counter"} };
  parser.setFuncMap(funcs);
  parser.setEventTypeMap(event_types);
  parser.setCounterMap(counters);

  PerfStats perf;
  parser.linkPerf(&perf);

  parser.setFuncDataCapacity(2*nthread*nexec_per_thread);
  parser.setCommDataCapacity(nthread*nexec_per_thread);
  parser.setCounterDataCapacity(nthread*nexec_per_thread);
  
  int ENTRY = 0;
  int EXIT = 1;
  int SEND = 2;
  int RECV = 3;
  int MYCOUNTER = 0;
  int MYFUNC = 0;

  int pid=0, rid=0;

  unsigned long tmp[100];

  for(int tid=0;tid<nthread;tid++){
    unsigned long time = 0;
    for(int e=0; e<nexec_per_thread; e++){ //each exec should have 1 comm and 1 counter event
      Event_t ev = createFuncEvent_t(pid, rid, tid, ENTRY, MYFUNC, time, tmp);
      parser.addFuncData(tmp);
      time += 100;
      ev = createCommEvent_t(pid, rid, tid, SEND, 1, 99, 1024, time, tmp);
      parser.addCommData(tmp);
      time += 100;
      ev = createCounterEvent_t(pid, rid, tid, MYCOUNTER, 1256, time, tmp);
      parser.addCounterData(tmp);
      time += 100;
      ev = createFuncEvent_t(pid, rid, tid, EXIT, MYFUNC, time, tmp);
      parser.addFuncData(tmp);
      time += 100;
    }
  }

  Timer timer(true);
  for(int t=0;t<tries;t++){
    auto v = parser.getEvents();
    assert(v.size() == 4*nthread*nexec_per_thread);
  }
  std::cout << "getEvents for " << 4*nexec_per_thread * nthread << " events, " << tries << " tries, avg time " << timer.elapsed_ms()/tries << " ms" << std::endl;

  perf.write(std::cout);

}

struct Args{
  int tries;
  int nthread;
  int nexec_per_thread;

  Args(){
    tries = 5;
    nthread = 10;
    nexec_per_thread = 10000;
  }
};


int main(int argc, char **argv){
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
    std::cout << "Enabling verbose debug output" << std::endl;
    enableVerboseLogging() = true;
  }       

  MPI_Init(&argc, &argv);
  int rank;
      
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  commandLineParser<Args> cmdline;
  addOptionalCommandLineArgDefaultHelpString(cmdline, tries);
  addOptionalCommandLineArgDefaultHelpString(cmdline, nthread);
  addOptionalCommandLineArgDefaultHelpString(cmdline, nexec_per_thread);
  
  if(argc == 2 && std::string(argv[1]) == "-help"){
    cmdline.help();
    MPI_Finalize();
    return 0;
  }

  Args args;
  cmdline.parse(args, argc-1, (const char**)(argv+1));

  benchmark_ADParserget_events(args.tries, rank, args.nthread, args.nexec_per_thread);
    
  MPI_Finalize();

  std::cout << "Rank " << rank << " done" << std::endl;
  return 0;
}
