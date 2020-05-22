#include<chimbuko/ad/ADProvenanceDBclient.hpp>
#include<chimbuko/verbose.hpp>

#ifdef ENABLE_PROVDB

using namespace chimbuko;


void ADProvenanceDBengine::data_v::initialize(){
  if(!m_is_initialized){
    m_eng = new thallium::engine(m_protocol.first, m_protocol.second);
    m_is_initialized = true;
  }
}

void ADProvenanceDBengine::data_v::finalize(){
  if(m_eng != nullptr){	  
    m_eng->finalize();
    delete m_eng;
    m_eng = nullptr;
    m_is_initialized = false;
  }
}




#endif
