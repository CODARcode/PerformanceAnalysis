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
  if(argc != 3){
    std::cout << "Usage: bpfile_replay <input BPfile filename> <output step freq (ms)>\n"
	      << "Output will be on SST under the same filename ${input BPfile filename}. The temporary .sst file created will thus be ${input BPfile filename}.sst"
	      << std::endl;
    exit(0);
  }
  std::string filename = argv[1];
  size_t step_freq_ms = strToAny<size_t>(argv[2]);

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


  adios2::Engine rd = io_in.Open(filename, adios2::Mode::Read);
  adios2::Engine wr = io_out.Open(filename, adios2::Mode::Write); //adds .sst to the actual temp file

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
	  io_out.DefineAttribute<std::string>(attributePair.first, attributePair.second.find("Value")->second);
	  attribs_seen.insert(attributePair.first);
	}
      }

      //Get new variables
      const std::map<std::string, adios2::Params> variables = io_in.AvailableVariables(); 
      for (const auto variablePair: variables){
	VERBOSE(std::cout << "Found variable " << variablePair.first << std::endl);
	varBase* var = parseVariable(variablePair.first, variablePair.second, io_in, rd);
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
};
