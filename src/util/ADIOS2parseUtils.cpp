#include <chimbuko/util/ADIOS2parseUtils.hpp>

std::ostream & chimbuko::operator<<(std::ostream &os, const mapPrint &mp){
  os << "{";
  for(auto it=mp.mp.begin(); it != mp.mp.end(); ++it)
    os << it->first << ":" << it->second << " ";
  os << "} ";
  return os;
}


chimbuko::varBase* chimbuko::parseVariable(const std::string &name, const std::map<std::string, std::string> &varinfo, adios2::IO &io, adios2::Engine &eng){
  std::map<std::string,std::string>::const_iterator it;
  int singleValue = -1;
  if( (it = varinfo.find("SingleValue")) != varinfo.end() ){
    if(it->second == "true") singleValue = 1;
    else if(it->second == "false") singleValue = 0;
    else assert(0);
  }	   
  std::vector<unsigned long> shape;
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
	shape.push_back(strToAny<unsigned long>(m[i]));
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
