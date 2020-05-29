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
#include <chimbuko/util/string.hpp>

using namespace chimbuko;

template<typename T>
struct vecPrint{
  const std::vector<T> &mp;
  vecPrint(const std::vector<T> &mp): mp(mp){}
};

/**
 * @brief iostream output for vectors
 */
template<typename T>
std::ostream & operator<<(std::ostream &os, const vecPrint<T> &mp){
  os << "(";
  for(auto it=mp.mp.begin(); it != mp.mp.end(); ++it)
    os << *it << " ";
  os << ")";
  return os;
}


/** 
 * @brief Base class for capturing ADIOS2 variables for string conversion
 */
struct varBase{
  std::string name;

  varBase(const std::string &name): name(name){}
  
  virtual std::string value() const{ return ""; };
  virtual void get(adios2::IO &io, adios2::Engine &eng){
    assert(0);
  };
  virtual ~varBase(){}
};

/**
   @brief Capture POD
*/
template<typename T>
struct varPOD: public varBase{
  T val;

  varPOD(const std::string &name): varBase(name){}
  varPOD(const std::string &name, adios2::IO &io, adios2::Engine &eng): varBase(name){
    this->get(io, eng);
  }
  
  void get(adios2::IO &io, adios2::Engine &eng){
    auto var = io.InquireVariable<T>(this->name);
    if(!var){
      std::cout << "Variable " << this->name << " does not seem to exist?!" << std::endl;
    }else{
      eng.Get<T>(var, &val, adios2::Mode::Sync);
    }
  }
  std::string value() const{
    std::ostringstream os;
    os <<val;
    return os.str();
  }
};

/**
 * @brief Capture multi-dimensional tensor data
 */
template<typename T>
struct varTensor: public varBase{
  std::vector<int> shape;

  template<typename listType>
  inline size_t map(const listType &c) const{
    assert(c.size() == shape.size());
    size_t out = 0;
    auto it = c.begin();
    for(int i=0;i<shape.size();i++){  //k + nk*(j + nj*i)  row-major order
      out *= shape[i];
      out += *it;
      ++it;
    }
    return out;
  }
  inline void unmap(std::vector<int> &c, size_t o) const{
    c.resize(shape.size());
    for(int i=shape.size()-1;i>=0;i--){
      c[i] = o % shape[i];
      o /= shape[i];
    }    
  }

  
  std::vector<T> val;

  varTensor(const std::string &name): varBase(name){}
  varTensor(const std::string &name, const std::vector<int> &shape, adios2::IO &io, adios2::Engine &eng): varBase(name), shape(shape){
    this->get(io, eng);
  }
  
  void get(adios2::IO &io, adios2::Engine &eng){
    auto var = io.InquireVariable<T>(this->name);
    if(!var){
      std::cout << "Variable " << this->name << " does not seem to exist?!" << std::endl;
    }else{
      size_t sz = 1; for(int i=0;i<shape.size();i++) sz *= shape[i];
      val.resize(sz);
      memset((void*)val.data(), 0, sz*sizeof(T));
      
      eng.Get<T>(var, val.data(), adios2::Mode::Sync);
    }
  }
  std::string value() const{
    std::ostringstream os;
    std::vector<int> c(shape.size());
    for(size_t i=0;i<val.size();i++){
      unmap(c, i);
      if( map(c) != i ) assert(0); //check consistency of map and unmap
      os << vecPrint<int>(c) << ":" << val[i] << " ";

      if(shape.size() == 2 && i % shape[1] == shape[1]-1) os << std::endl;  //endline after each row of matrix
    }
    return os.str();
  }
};




/**
   @brief Get a variable from ADIOS2 and return storage object
*/
varBase* parseVariable(const std::string &name, const std::map<std::string, std::string> &varinfo, adios2::IO &io, adios2::Engine &eng){
  std::map<std::string,std::string>::const_iterator it;
  int singleValue = -1;
  if( (it = varinfo.find("SingleValue")) != varinfo.end() ){
    if(it->second == "true") singleValue = 1;
    else if(it->second == "false") singleValue = 0;
    else assert(0);
  }	   
  std::vector<int> shape;
  if(!singleValue && (it = varinfo.find("Shape")) != varinfo.end() ){
    std::string shape_s = it->second;
    std::regex r(R"((\d+)(?:\,\s(\d+))*)"); 
    std::smatch m;
    if(!std::regex_match(shape_s, m, r)){
      std::cout << "Could not match string '" << shape_s << "' to regex" << std::endl;
    }else{
      std::cout << "Parsed size ";
      for(int i=1;i<m.size();i++){
	std::cout << m[i] << " ";
	shape.push_back(std::stoi(m[i]));
      }
      std::cout << std::endl;
    }
  }
  
  if( (it = varinfo.find("Type")) != varinfo.end() ){
    std::string type = it->second;
    
    varBase *val = NULL;
    if(singleValue){
#define DOIT(T) if(type == #T) val = new varPOD<T>(name, io, eng);
      DOIT(uint64_t);
      DOIT(int32_t);
#undef DOIT
    }else if(shape.size() > 0){
#define DOIT(T) if(type == #T) val = new varTensor<T>(name, shape, io, eng);
      DOIT(uint64_t);
      DOIT(int32_t);
#undef DOIT
    }
    return val;
  }
  return NULL;
}


/**
   @brief iostream output for string map
*/
struct mapPrint{
  const std::map<std::string, std::string> &mp;
  mapPrint(const std::map<std::string, std::string> &mp): mp(mp){}
};
std::ostream & operator<<(std::ostream &os, const mapPrint &mp){
  os << "{";
  for(auto it=mp.mp.begin(); it != mp.mp.end(); ++it)
    os << it->first << ":" << it->second << " ";
  os << "} ";
  return os;
}
  

int main(int argc, char** argv){
  if(argc < 2){
    std::cout << "Usage sst_view <bp filename (without .sst extension)> <options>" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "-nsteps_show_variable_values  Set the number of io steps for which the data will be displayed, after which output will be suppressed. Default 2. Use -1 for all steps." << std::endl;
    std::cout << "-offline Rather than connecting online via SST, read the BP file offline (use TAU_ADIOS2_ENGINE=BPFile when running main program)" << std::endl;
    exit(0);
  }
  std::string filename = argv[1];

  //Options
  bool offline = false; //if TAU_ADIOS2_ENGINE=BPFile the main program will store a BP file for offline analysis. Use this option to read the BP file
  size_t nsteps_show_variable_values = 2; //the number of steps for which the values of updated variables will be dumped to output. Use -1 for all steps
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

  //Begin main
  assert( MPI_Init(&argc, &argv) == MPI_SUCCESS );

  //Setup ADIOS2 capture
  adios2::ADIOS ad;
  adios2::IO io;
  adios2::Engine eng;

  ad = adios2::ADIOS(MPI_COMM_SELF, adios2::DebugON);
  io = ad.DeclareIO("tau-metrics");
  if(!offline) io.SetEngine("SST");
  io.SetParameters({
		       {"MarshalMethod", "BP"},{"DataTransport", "RDMA"}
    });
  
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
  
  assert(MPI_Finalize() == MPI_SUCCESS );
  return 0;
}
