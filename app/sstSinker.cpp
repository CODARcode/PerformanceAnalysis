//A program for connecting to tau2 adios2 output and running the parser routines for timing purposes and/or debugging
#include "chimbuko/AD.hpp"
#include <chrono>
#include "chimbuko/util/commandLineParser.hpp"
#include "chimbuko/util/string.hpp"
#include "chimbuko/util/error.hpp"

using namespace chimbuko;
using namespace std::chrono;

struct SinkerArgs{
  int timeout;
  int beginstep_timeout;
  bool dump_events;
  
  SinkerArgs(): timeout(60), beginstep_timeout(30), dump_events(false){}
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


int main(int argc, char ** argv){
  MPI_Init(&argc, &argv);

  int world_rank, world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  bool error = false;
  try
    {
      commandLineParser<SinkerArgs> cmdline;
      addOptionalCommandLineArg(cmdline, timeout, "Specify the SST connect timeout in seconds (Default 60s)");
      addOptionalCommandLineArg(cmdline, beginstep_timeout, "Specify the SST beginStep timeout in seconds (Default 30s)");
      addOptionalCommandLineArg(cmdline, dump_events, "Request that the parsed events be dumped to a file \"${BPFILENAME}.dump\". Requires \"fetch\" to be true. (Default false)");
      
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
      std::string inputFile = prefix + "-" + std::to_string(world_rank) + ".bp";
      int fetch_data = atoi(argv[4]);

      SinkerArgs args;
      cmdline.parse(args, argc-5, (const char**)(argv+5) );

      if(args.dump_events && !fetch_data) fatal_error("dump_events option requires fetch=1");
      
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

	    if(args.dump_events)
	      dumpEvents(*dump, parser->getEvents(),parser);
	  }

	  frames++;
	  parser->endStep();
	}
      t2 = high_resolution_clock::now();
      if (world_rank == 0) {
	std::cout << "rank: " << world_rank << " analysis done!\n";
      }

      // -----------------------------------------------------------------------
      // Average analysis time and total number of outliers
      // -----------------------------------------------------------------------
      //MPI_Barrier(MPI_COMM_WORLD);
      processing_time = duration_cast<milliseconds>(t2 - t1).count();

      if (false) {
	const unsigned long local_measures[] = {processing_time, frames};
	unsigned long global_measures[] = {0, 0};
	MPI_Reduce(
		   local_measures, global_measures, 2, MPI_UNSIGNED_LONG,
		   MPI_SUM, 0, MPI_COMM_WORLD
		   );
	total_processing_time = global_measures[0];
	total_frames = global_measures[1];
      }

      if (false && world_rank == 0) {
	std::cout << "\n"
		  << "Avg. num. frames     : " << (double)total_frames/(double)world_size << "\n"
		  << "Avg. processing time : " << (double)total_processing_time/(double)world_size << " msec\n"
		  << std::endl;
      }

      // -----------------------------------------------------------------------
      // Finalize
      // -----------------------------------------------------------------------
      delete parser;
      if(dump) delete dump;
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

  MPI_Finalize();
  return error ? 1 : 0;
}
