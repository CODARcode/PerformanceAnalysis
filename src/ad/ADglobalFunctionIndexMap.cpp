#include <chimbuko/ad/ADglobalFunctionIndexMap.hpp>

using namespace chimbuko;

unsigned long ADglobalFunctionIndexMap::lookup(const unsigned long local_idx, const std::string &func_name){
  if(!connectedToPS()) return local_idx;
  auto it = m_idxmap.find(local_idx);
  if(it != m_idxmap.end()){
    VERBOSE(std::cout << "ADglobalFunctionIndexMap local index " << local_idx << " already in map, maps to global index " << it->second << std::endl);
    return it->second;
  }else{
    //Obtain the index from the pserver
    Message msg;
    msg.set_info(m_net_client->get_client_rank(), m_net_client->get_server_rank(), MessageType::REQ_GET, MessageKind::FUNCTION_INDEX);
    msg.set_msg(func_name);

    VERBOSE(std::cout << "ADglobalFunctionIndexMap sending message: " << msg.data() << std::endl);
    std::string str_rep = m_net_client->send_and_receive(msg);
    VERBOSE(std::cout << "ADglobalFunctionIndexMap received response: " << str_rep << std::endl);
	  
    nlohmann::json rep = nlohmann::json::parse(str_rep);
    if(!rep.contains("Buffer")) throw std::runtime_error("ADglobalFunctionIndexMap received malformed message: " + rep.dump());
    unsigned long global_idx = rep["Buffer"];
    m_idxmap[local_idx] = global_idx;
    VERBOSE(std::cout << "ADglobalFunctionIndexMap local index " << local_idx << " maps to global idx " << global_idx << std::endl);

    return global_idx;
  }
  assert(0);
}      

unsigned long ADglobalFunctionIndexMap::lookup(const unsigned long local_idx) const{
  if(!connectedToPS()) return local_idx;
  auto it = m_idxmap.find(local_idx);
  if(it != m_idxmap.end()) return it->second;
  else throw std::runtime_error("ADglobalFunctionIndexMap::lookup (const version): local index " + anyToStr(local_idx) + " not in map");
}

  
