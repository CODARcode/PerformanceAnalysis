#include <mpi.h>
#include <adios2.h>
#include <string>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <set>
#include <regex>
#include <chimbuko/util/string.hpp>
#include <chimbuko/util/ADIOS2parseUtils.hpp>
#include <chimbuko/verbose.hpp>

using namespace chimbuko;


int main(int argc, char** argv){
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
    std::cout << "Enabling verbose debug output" << std::endl;
    Verbose::set_verbose(true);
  }       

  MPI_Init(&argc, &argv);
  if(argc < 3){
    std::cout << "Usage: bpfile_replay <input BPfile filename> <output step freq (ms)> <options>\n"
	      << "Output will be on SST under the same filename ${input BPfile filename}. The temporary .sst file created will thus be ${input BPfile filename}.sst\n"
	      << "Options:\n"
	      << "-spoof_rank : Change the rank index associated with the MetaData and comm_timestamps, counter_values, event_timestamps data sets.\n"
	      << "-outfile : Manually choose the output filename.\n"
	      << std::endl;
    exit(0);
  }
  std::string in_filename = argv[1];
  std::string out_filename = argv[1];
  size_t step_freq_ms = strToAny<size_t>(argv[2]);

  bool spoof_rank = false;
  int spoof_rank_val;
  
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
    }else{
      std::cerr << "Unknown option " << argv[arg] << std::endl;
      exit(-1);
    }
  }


  adios2::ADIOS ad = adios2::ADIOS(MPI_COMM_SELF, adios2::DebugON);
  adios2::IO io_in = ad.DeclareIO("reader");
  io_in.SetEngine("BPFile");
  io_in.SetParameters({
      {"MarshalMethod", "BP"},
	{"DataTransport", "RDMA"}
    });

  adios2::IO io_out = ad.DeclareIO("writer");
  io_out.SetEngine("SST");
  io_out.SetParameters({
      {"MarshalMethod", "BP"},
  	{"DataTransport", "RDMA"}
    });


  adios2::Engine rd = io_in.Open(in_filename, adios2::Mode::Read);
  adios2::Engine wr = io_out.Open(out_filename, adios2::Mode::Write); //adds .sst to the actual temp file

  std::set<std::string> attribs_seen;

  size_t step = 0;
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
	  VERBOSE(std::cout << "Defining attribute " << attributePair.first << std::endl);
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
	VERBOSE(std::cout << "Found variable " << name << std::endl);
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

	//Write to SST output
	if(var != NULL){
	  VERBOSE(std::cout << " Value : " << var->value() << std::endl);
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

  std::cout << "Shutting down" << std::endl;
  rd.Close();
  wr.Close();

  MPI_Finalize();
  return 0;
};
