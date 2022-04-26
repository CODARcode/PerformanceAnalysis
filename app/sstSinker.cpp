//A program for connecting to tau2 adios2 output and running the parser routines for timing purposes and/or debugging
#include <chimbuko_config.h>
#ifdef USE_MPI
#include<mpi.h>
#endif
#include "chimbuko/AD.hpp"
#include <chrono>
#include <map>
#include "chimbuko/util/commandLineParser.hpp"
#include "chimbuko/util/string.hpp"
#include "chimbuko/util/error.hpp"

using namespace chimbuko;
using namespace std::chrono;

struct SinkerArgs{
  int timeout;
  int beginstep_timeout;
  bool dump_events;
  bool dump_runtimes;
#ifndef USE_MPI
  int rank;
#endif
  int rank_override; //manually override the rank ;  -1 default does nothing

  SinkerArgs(): timeout(60), beginstep_timeout(30), dump_events(false), dump_runtimes(false), rank_override(-1)
#ifndef USE_MPI
	      , rank(-1)
#endif
  {}
};

void dumpEvents(std::ostream &os, const std::vector<Event_t> &events, ADParser* parser){
  const std::unordered_map<int, std::string> &func_map = *parser->getFuncMap();
  const std::unordered_map<int, std::string> &counter_map = *parser->getCounterMap();
  const std::unordered_map<int, std::string> &event_type_map = *parser->getEventType();
  
  for(const Event_t &e: events){
    if(e.type() == EventDataType::FUNC){
      auto eit = event_type_map.find(e.eid()); if(eit == event_type_map.end()) fatal_error("Could not find event type in map!");
      const std::string &etype = eit->second;
      auto fit = func_map.find(e.fid());  if(fit == func_map.end()) fatal_error("Could not find fid in map!");
      os << e.tid() << " " << e.ts() << " FUNC " << etype << " " << fit->second << std::endl;
    }else if(e.type() == EventDataType::COUNT){
      auto cit = counter_map.find(e.counter_id());  if(cit == counter_map.end()) fatal_error("Could not find counter id in map!");
      os << e.tid() << " " << e.ts() << " COUNT " << cit->second << " " << e.counter_value() << std::endl;
    }else if(e.type() == EventDataType::COMM){
      os << e.tid() << " " << e.ts() << " COMM " << e.partner() << " " << e.bytes() << std::endl;
    }
  }
}

void extractRuntimes(std::map<unsigned long, std::vector<unsigned long> > &into, const std::vector<Event_t> &events, ADEvent &event_man){ 
  for(auto const &e: events) event_man.addEvent(e);
  ExecDataMap_t const &execs = *event_man.getExecDataMap();  //std::unordered_map<unsigned long, std::vector<CallListIterator_t>> 
  for(auto const &e: execs){
    std::vector<unsigned long> &into_f = into[e.first];
    for(auto const &cit: e.second)
      into_f.push_back(cit->get_exclusive());
  }
  event_man.purgeCallList();
}

void dumpRuntimes(std::ostream &os, const std::map<unsigned long, std::vector<unsigned long> > &rtimes){
  for(auto const &fe: rtimes){
    for(unsigned long t: fe.second)
      os << fe.first << " " << t << std::endl;
  }
}  
    




int main(int argc, char ** argv){
#ifdef USE_MPI
  MPI_Init(&argc, &argv);
#endif

  bool error = false;
  try
    {
      commandLineParser<SinkerArgs> cmdline;
      addOptionalCommandLineArg(cmdline, timeout, "Specify the SST connect timeout in seconds (Default 60s)");
      addOptionalCommandLineArg(cmdline, beginstep_timeout, "Specify the SST beginStep timeout in seconds (Default 30s)");
      addOptionalCommandLineArg(cmdline, dump_events, "Request that the parsed events be dumped to a file \"${BPFILENAME}.dump\". Requires \"fetch\" to be true. (Default false)");
      addOptionalCommandLineArg(cmdline, dump_runtimes, "Request that the parsed function execution (exclusive) runtimes be dumped to a file \"${BPFILENAME}.runtimes\". Requires \"fetch\" to be true. (Default false)");
      addOptionalCommandLineArg(cmdline, rank_override, "Manually override the rank index obtained from MPI. (Default: -1  (do nothing) )");

#ifndef USE_MPI
      addOptionalCommandLineArg(cmdline, rank, "Specify the rank of the application (Default 0)");
#endif

      if(argc < 5 || (argc == 2 && std::string(argv[1]) == "-help")){
	std::cout << "Usage: <exe> <engine type (BPFile, SST)> <bp directory> <bpfile prefix (eg tau-metrics-nwchem)> <fetch> <options>\n"
		  << "Where \"fetch\" indicates whether the data is actually transferred or we just iterate over the IO steps\n"
		  << "Options:" << std::endl;
	cmdline.help(std::cout);
	return 0;
      }    

      std::string engineType = argv[1]; // BPFile or SST
      std::string data_dir = argv[2]; // *.bp location
      std::string prefix = argv[3]; // "tau-metrics-nwchem"
      int fetch_data = atoi(argv[4]);

      SinkerArgs args;
      cmdline.parse(args, argc-5, (const char**)(argv+5) );

      if(args.dump_events && !fetch_data) fatal_error("dump_events option requires fetch=1");
      
      int world_rank;
#ifdef USE_MPI
      MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
#else
      //Rank is specified by cmdline option (default 0)
      world_rank = args.rank;
#endif
      if(args.rank_override != -1){
	if(args.rank_override < 0) fatal_error("Invalid rank provided");
	world_rank = args.rank_override;
      }

      std::string inputFile = prefix + "-" + std::to_string(world_rank) + ".bp";

      if (world_rank == 0) {
	std::cout << "\n"
		  << "rank       : " << world_rank << "\n"
		  << "Engine     : " << engineType << "\n"
		  << "BP in dir  : " << data_dir << "\n"
		  << "BP file    : " << inputFile << "\n"
		  << "Fetch      : " << fetch_data << "\n"
		  << "Timeout (s): " << args.timeout
		  << std::endl;
      }

      // -----------------------------------------------------------------------
      // AD module variables
      // -----------------------------------------------------------------------
      ADParser * parser;

      // int step = 0;

      // -----------------------------------------------------------------------
      // Measurement variables
      // -----------------------------------------------------------------------
      unsigned long total_frames = 0, frames = 0;
      unsigned long total_processing_time = 0, processing_time = 0;
      high_resolution_clock::time_point t1, t2;

      // -----------------------------------------------------------------------
      // Init. AD module
      // First, init io to make sure file (or connection) handler
      // -----------------------------------------------------------------------
      parser = new ADParser(data_dir + "/" + inputFile, 0, world_rank, engineType, args.timeout);

      parser->setBeginStepTimeout(args.beginstep_timeout);

      //Initialize dump output
      std::ofstream *dump = nullptr;
      if(args.dump_events) dump = new std::ofstream(inputFile + ".dump");

      //Initialize execution time dump output
      std::ofstream *dump_runtimes = nullptr;
      ADEvent event_man;
      std::map<unsigned long, std::vector<unsigned long> > fruntimes; //fid -> runtimes

      if(args.dump_runtimes){
	dump_runtimes = new std::ofstream(inputFile + ".runtimes");
	event_man.linkFuncMap(parser->getFuncMap());
	event_man.linkEventType(parser->getEventType());
	event_man.linkCounterMap(parser->getCounterMap());
      }
            
      // -----------------------------------------------------------------------
      // Start analysis
      // -----------------------------------------------------------------------
      if (world_rank == 0) {
	std::cout << "rank: " << world_rank << std::endl;
      }
      t1 = high_resolution_clock::now();
      while ( parser->getStatus() )
	{
	  parser->beginStep();
	  if (!parser->getStatus())
	    {
	      // No more steps available.
	      break;
	    }

	  // step = parser->getCurrentStep();

	  if (fetch_data) {
	    parser->update_attributes();
	    parser->fetchFuncData();
	    parser->fetchCommData();
	    parser->fetchCounterData();
	    parser->endStep(); //end step before parsing events as ADIOS2 can delay copy to step end

	    std::vector<Event_t> events;
	    if(args.dump_events || args.dump_runtimes) events = parser->getEvents();

	    if(args.dump_events)
	      dumpEvents(*dump, events,parser);
	    
	    if(args.dump_runtimes)
	      extractRuntimes(fruntimes, events, event_man);
	    
	  }else{
	    parser->endStep();
	  }
	  frames++;
	}
      t2 = high_resolution_clock::now();
      if (world_rank == 0) {
	std::cout << "rank: " << world_rank << " analysis done!\n";
      }

      // -----------------------------------------------------------------------
      // Finalize
      // -----------------------------------------------------------------------
      delete parser;
      if(dump) delete dump;
      if(dump_runtimes){
	dumpRuntimes(*dump_runtimes, fruntimes);
	delete dump_runtimes;
      }
    }
  catch (std::invalid_argument &e)
    {
      std::cout << e.what() << std::endl;
      error = true;
      //todo: usages()
    }
  catch (std::ios_base::failure &e)
    {
      std::cout << "I/O base exception caught\n";
      std::cout << e.what() << std::endl;
      error = true;
    }
  catch (std::exception &e)
    {
      std::cout << "Exception caught\n";
      std::cout << e.what() << std::endl;
      error = true;
    }

#ifdef USE_MPI
  MPI_Finalize();
#endif
  return error ? 1 : 0;
}
