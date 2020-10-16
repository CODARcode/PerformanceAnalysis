#include <chimbuko/ad/ADglobalFunctionIndexMap.hpp>

using namespace chimbuko;

unsigned long ADglobalFunctionIndexMap::lookup(const unsigned long local_idx, const std::string &func_name){
  if(!connectedToPS()) return local_idx;

  int rank = m_net_client->get_client_rank();
  auto it = m_idxmap.find(local_idx);
  if(it != m_idxmap.end()){
    VERBOSE(std::cout << "ADglobalFunctionIndexMap rank " << rank << " local index " << local_idx << " already in map, maps to global index " << it->second << std::endl);
    return it->second;
  }else{
    //Obtain the index from the pserver
    VERBOSE(std::cout << "ADglobalFunctionIndexMap rank " << rank << " obtaining global index corresponding to local index " << local_idx << std::endl);

    Message msg;
    msg.set_info(m_net_client->get_client_rank(), m_net_client->get_server_rank(), MessageType::REQ_GET, MessageKind::FUNCTION_INDEX);
    msg.set_msg(func_name);

    std::string str_rep = m_net_client->send_and_receive(msg);
	     
    msg.set_msg(str_rep, true);
    unsigned long global_idx = strToAny<unsigned long>(msg.buf());

    m_idxmap[local_idx] = global_idx;
    VERBOSE(std::cout << "ADglobalFunctionIndexMap rank " << rank << " local index " << local_idx << " maps to global idx " << global_idx << std::endl);

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

  
