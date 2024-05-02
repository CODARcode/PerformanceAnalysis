//A simple debugging tool that captures tau2 ADIOS2 output stream and displays captured data and metadata
//in real-time
#include <adios2.h>
#include<thread>
#include<chrono>
#include <condition_variable>
#include <mutex>
#include <cassert>
#include <sstream>
#include <regex>
#include <iostream>
#include <cstring>
#include <chimbuko/core/util/string.hpp>
#include <chimbuko/core/util/ADIOS2parseUtils.hpp>

using namespace chimbuko;  

int main(int argc, char** argv){
  if(argc < 2){
    std::cout << "Usage sst_view <bp filename (without .sst extension)> <options>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "-nsteps_show_variable_values  Set the number of io steps for which the data will be displayed, after which output will be suppressed. Use -1 for all steps. (Default -1)" << std::endl;
    std::cout << "-offline Rather than connecting online via SST, read the BP file offline (use TAU_ADIOS2_ENGINE=BPFile when running main program)" << std::endl;
    exit(0);
  }
  std::string filename = argv[1];

  //Options
  bool offline = false; //if TAU_ADIOS2_ENGINE=BPFile the main program will store a BP file for offline analysis. Use this option to read the BP file
  size_t nsteps_show_variable_values = -1; //the number of steps for which the values of updated variables will be dumped to output. Use -1 for all steps
  int arg = 2;
  while(arg < argc){
    std::string sarg = std::string(argv[arg]);
    if(sarg == "-nsteps_show_variable_values"){
      nsteps_show_variable_values = strToAny<size_t>(argv[arg+1]);
      arg+=2;
    }else if(sarg == "-offline"){
      offline = true;
      arg++;
    }else{
      std::cerr << "Unknown argument " << sarg;
      exit(-1);
    }
  }

  //Setup ADIOS2 capture
  adios2::ADIOS ad;
  adios2::IO io;
  adios2::Engine eng;

  ad = adios2::ADIOS();
  io = ad.DeclareIO("tau-metrics");
  if(!offline) io.SetEngine("SST");
  io.SetParameters({
		       {"MarshalMethod", "BP"},{"DataTransport", "RDMA"}
    });

  std::cout << "sst_view is connecting to file " << filename << " on mode " << (offline ? "BPFile" : "SST") << std::endl;
  
  eng = io.Open(filename, adios2::Mode::Read);
  
  std::cout << "Established comms" << std::endl;

  std::map<std::string, std::map<std::string, std::string> > attribs_all;
  std::map<std::string, std::map<std::string, std::string> > vars_all;
  std::map<std::string, int> vars_seen_count;

  //Loop over IO steps and dump information
  size_t step = 0;
  while(1){
    adios2::StepStatus status = eng.BeginStep();
    
    if(status == adios2::StepStatus::NotReady){
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }else if(status == adios2::StepStatus::OK){
      std::cout << "Starting step " << step << std::endl;
      
      //Get new attributes
      const std::map<std::string, adios2::Params> attributes = io.AvailableAttributes(); 
      for (const auto attributePair: attributes){
	if(!attribs_all.count(attributePair.first)){ 
	
	  attribs_all[attributePair.first] = attributePair.second;
	  std::cout << "FOUND NEW ATTRIBUTE: " << attributePair.first << " " << mapPrint(attributePair.second) << std::endl;
	}
      }

      //Get new variables
      const std::map<std::string, adios2::Params> variables = io.AvailableVariables(); 
      for (const auto variablePair: variables){
	
	if(!vars_all.count(variablePair.first)){ //haven't seen it yet
	  vars_all[variablePair.first] = variablePair.second;
	  std::cout << "FOUND NEW VARIABLE: " << variablePair.first << " " << mapPrint(variablePair.second);

	  varBase* var = parseVariable(variablePair.first, variablePair.second, io, eng);		     
	  if(var != NULL){
	    std::cout << " Value : " << var->value();
	    delete var;
	  }

	  std::cout << std::endl;
	  vars_seen_count[variablePair.first] = 1;
	}//is new variable
	else{
	  int &count = vars_seen_count[variablePair.first];
	  ++count;

	  if(nsteps_show_variable_values == -1 || count <= nsteps_show_variable_values){
	    vars_all[variablePair.first] = variablePair.second;
	    std::cout << "FOUND UPDATE " << count << " FOR VARIABLE VARIABLE: " << variablePair.first << " " << mapPrint(variablePair.second);
	    
	    varBase* var = parseVariable(variablePair.first, variablePair.second, io, eng);		     
	    if(var != NULL){
	      std::cout << " Value : " << var->value();
	      delete var;
	    }

	    std::cout << std::endl;
	  }
	}

      }//var loop


      eng.EndStep();
      ++step;
    }else{
      std::cout << "Viewer lost contact with writer" << std::endl;
      break;
    }
  }

  std::cout << "Shutting down" << std::endl;
  eng.Close();
  
  return 0;
}
