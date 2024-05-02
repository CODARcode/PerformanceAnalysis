#include <adios2.h>
#include <string>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <set>
#include <regex>
#include <algorithm>
#include <climits>
#include <chimbuko/core/util/string.hpp>
#include <chimbuko/core/util/ADIOS2parseUtils.hpp>
#include <chimbuko/verbose.hpp>

using namespace chimbuko;


int main(int argc, char** argv){
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
    std::cout << "Enabling verbose debug output" << std::endl;
    enableVerboseLogging() = true;
  }       

  if(argc < 3){
    std::cout << "Usage: bpfile_replay <input BPfile filename> <output step freq (ms)> <options>\n"
	      << "Output will be on SST under the same filename ${input BPfile filename}. The temporary .sst file created will thus be ${input BPfile filename}.sst\n"
	      << "Options:\n"
	      << "-spoof_rank <idx>: Change the rank index associated with the MetaData and comm_timestamps, counter_values, event_timestamps data sets.\n"
	      << "-outfile <file>: Manually choose the output filename.\n"
	      << "-nreplay <value>: Set the number of times the file is replayed (default 1). On subsequent replays the timestamps will be offset to make it look like a single run.\n"
	      << "-out_engine <BPFile/SST>: Set the output engine type. Outfile must be set different from infile if BPFile engine chosen\n"
	      << std::endl;
    exit(0);
  }
  std::string in_filename = argv[1];
  std::string out_filename = argv[1];
  size_t step_freq_ms = strToAny<size_t>(argv[2]);
  int nreplay = 1;

  bool spoof_rank = false;
  int spoof_rank_val;
  
  std::string out_engine = "SST";
  
  int arg = 3;
  while(arg < argc){
    std::string sarg = argv[arg];
    if(sarg == "-spoof_rank"){
      spoof_rank = true;
      assert(arg != argc-1);
      spoof_rank_val = strToAny<int>(argv[arg+1]);
      arg+=2;
      std::cout << "Spoofing rank as " << spoof_rank_val << std::endl;
    }else if(sarg == "-outfile"){
      assert(arg != argc-1);
      out_filename = argv[arg+1];
      arg+=2;
      std::cout << "Set output file to " << out_filename << std::endl;
    }else if(sarg == "-nreplay"){
      assert(arg != argc-1);
      nreplay = strToAny<int>(argv[arg+1]);
      arg+=2;
      std::cout << "Replaying file " << nreplay << " times" << std::endl;
    }else if(sarg == "-out_engine"){
      assert(arg != argc-1);
      out_engine = argv[arg+1];
      if(out_engine != "SST" && out_engine != "BPFile")
	throw std::runtime_error("Invalid output engine type");      
      arg+=2;
      std::cout << "Set output engine to " << out_engine << std::endl;
    }else{
      std::cerr << "Unknown option " << argv[arg] << std::endl;
      exit(-1);
    }
  }


  adios2::ADIOS ad = adios2::ADIOS();

  adios2::IO io_out = ad.DeclareIO("writer");
    
  if(out_engine == "BPFile" && out_filename == in_filename)
    throw std::runtime_error("If output engine set to BPFile, the input and output filenames cannot be the same");
  
  io_out.SetEngine(out_engine);
  io_out.SetParameters({
      {"MarshalMethod", "BP"},
  	{"DataTransport", "RDMA"}
    });

  adios2::Engine wr = io_out.Open(out_filename, adios2::Mode::Write); //adds .sst to the actual temp file
  std::set<std::string> attribs_seen;
  size_t step = 0;

  //Column offset of timestamp
  std::unordered_map<std::string, int> ts_column = { {"comm_timestamps",7},
						     {"counter_values",5},
						     {"event_timestamps",5} };
  unsigned long ts_delta = 0; //offset added to timestamps
  unsigned long input_ts_largest = 0; //largest input ts seen
  unsigned long input_ts_smallest = ULONG_MAX;

  //Replay the file nreplay times
  for(int r=0;r<nreplay;r++){
    adios2::IO io_in = ad.DeclareIO("reader");
    io_in.SetEngine("BPFile");
    io_in.SetParameters({
	{"MarshalMethod", "BP"},
	  {"DataTransport", "RDMA"}
      });
    
    adios2::Engine rd = io_in.Open(in_filename, adios2::Mode::Read);
    while(1){
      adios2::StepStatus status = rd.BeginStep(adios2::StepMode::Read, 10.0f);
      
      if(status == adios2::StepStatus::NotReady){
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	continue;
      }else if(status == adios2::StepStatus::OK){
	std::cout << "Starting read step " << step << std::endl;
	
	adios2::StepStatus wstatus = adios2::StepStatus::NotReady;
	while(wstatus == adios2::StepStatus::NotReady){
	  wstatus = wr.BeginStep();
	  std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	
	//Get new attributes
	const std::map<std::string, adios2::Params> attributes = io_in.AvailableAttributes(); 
	for (const auto attributePair: attributes){
	  if(!attribs_seen.count(attributePair.first)){
	    verboseStream << "Defining attribute " << attributePair.first << std::endl;
	    std::string key = attributePair.first;
	    if(spoof_rank){ //spoof rank index
	      key = std::regex_replace(key, std::regex(R"(MetaData\:(\d+)\:)"), stringize("MetaData:%d:", spoof_rank_val));
	    }
	    std::string value = attributePair.second.find("Value")->second;
	    if(spoof_rank && std::regex_search(key, std::regex(R"(MPI\sComm\sWorld\sRank)"))){
	      value = anyToStr(spoof_rank_val);
	    }
	    
	    io_out.DefineAttribute<std::string>(key, value);
	    attribs_seen.insert(attributePair.first);
	  }
	}
	
	//Get new variables
	const std::map<std::string, adios2::Params> variables = io_in.AvailableVariables(); 
	for (const auto variablePair: variables){
	  std::string name = variablePair.first;
	  verboseStream << "Found variable " << name << std::endl;
	  varBase* var = parseVariable(name, variablePair.second, io_in, rd);
	  
	  //Apply rank spoofing if required
	  if(spoof_rank && (			  
			    name == "comm_timestamps" ||
			    name == "counter_values" ||
			    name == "event_timestamps" )
	     ){
	    varTensor<uint64_t> *var_t = dynamic_cast<varTensor<uint64_t> * >(var);
	    assert(var_t->getShape().size() == 2);
	    std::vector<unsigned long> c(2);
	    c[1] = 1; //rank index is always the second column
	    for(c[0]=0; c[0]< var_t->getShape()[0]; c[0]++){
	      (*var_t)(c) = spoof_rank_val;
	    }
	  }

	  //Apply the timestamp offset
	  auto col_it = ts_column.find(name);
	  if(col_it != ts_column.end()){
	    int col = col_it->second;

	    varTensor<uint64_t> *var_t = dynamic_cast<varTensor<uint64_t> * >(var);
	    assert(var_t->getShape().size() == 2);
	    std::vector<unsigned long> c(2);
	    c[1] = col;
	    for(c[0]=0; c[0]< var_t->getShape()[0]; c[0]++){
	      unsigned long ts_in = (*var_t)(c);
	      input_ts_largest = std::max(ts_in, input_ts_largest);	      
	      input_ts_smallest = std::min(ts_in, input_ts_smallest);	      
	      (*var_t)(c) = ts_in + ts_delta;
	    }
	  }	  	 

	  //Write to SST output
	  if(var != NULL){
	    verboseStream << " Value : " << var->value() << std::endl;
	    var->put(io_out, wr);
	    delete var;
	  }
	}

	rd.EndStep();
	wr.EndStep();
      
	++step;

	std::this_thread::sleep_for(std::chrono::milliseconds(step_freq_ms));
      }else{
	std::cout << "Viewer lost contact with writer" << std::endl;
	break;
      }
    }
    std::cout << "Finished replay " << r << std::endl;
    rd.Close();
    if(r != nreplay-1){
      ad.RemoveIO("reader");
      ts_delta += input_ts_largest - input_ts_smallest + 1;
      std::cout << "Smallest input timestamp " << input_ts_smallest << ", largest " << input_ts_largest << std::endl; 
      std::cout << "Timestamp offset set to " << ts_delta << std::endl;
    }
  }

  std::cout << "Shutting down" << std::endl;
  wr.Close();

  return 0;
};
