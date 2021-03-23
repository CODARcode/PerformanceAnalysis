#include<chimbuko/ad/ADProvenanceDBclient.hpp>
#include<chimbuko/verbose.hpp>

#ifdef ENABLE_PROVDB

using namespace chimbuko;


void ADProvenanceDBengine::data_v::initialize(){
  if(!m_is_initialized){
    verboseStream << "ADProvenanceDBengine initializing Thallium engine" << std::endl;
    m_eng = new thallium::engine(m_protocol.first, m_protocol.second, true, -1); //3rd arg: use dedicated progress thread, 4th arg: use this thread for RPCs
    m_is_initialized = true;
    verboseStream << "ADProvenanceDBengine initialized Thallium engine" << std::endl;
  }
}

void ADProvenanceDBengine::data_v::finalize(){
  if(m_eng != nullptr){	  
    verboseStream << "ADProvenanceDBengine finalizing Thallium engine" << std::endl;
    m_eng->finalize();
    verboseStream << "ADProvenanceDBengine deleting Thallium engine instance" << std::endl;
    delete m_eng;
    m_eng = nullptr;
    m_is_initialized = false;
    verboseStream << "ADProvenanceDBengine completed finalize" << std::endl;
  }
}

std::string ADProvenanceDBengine::getProtocolFromAddress(const std::string &addr){
  size_t pos = addr.find(':');
  if(pos == std::string::npos) throw std::runtime_error("Address \"" + addr + "\" does not have expected form");
  std::string protocol = addr.substr(0,pos);
  //Ignore opening quotation marks
  while(protocol[0] == '"' || protocol[0] == '\'') protocol = protocol.substr(1,std::string::npos);
  return protocol;
}


#endif
