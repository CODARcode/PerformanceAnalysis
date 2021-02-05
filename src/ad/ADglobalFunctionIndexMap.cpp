#include <chimbuko/ad/ADglobalFunctionIndexMap.hpp>
#include <chimbuko/util/error.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/utility.hpp>


using namespace chimbuko;

unsigned long ADglobalFunctionIndexMap::lookup(const unsigned long local_idx, const std::string &func_name){
  if(!connectedToPS()) return local_idx;

  int rank = m_net_client->get_client_rank();
  auto it = m_idxmap.find(local_idx);
  if(it != m_idxmap.end()){
    verboseStream << "ADglobalFunctionIndexMap rank " << rank << " local index " << local_idx << " already in map, maps to global index " << it->second << std::endl;
    return it->second;
  }else{
    //Obtain the index from the pserver
    verboseStream << "ADglobalFunctionIndexMap rank " << rank << " obtaining global index corresponding to local index " << local_idx << std::endl;

    Message msg;
    msg.set_info(m_net_client->get_client_rank(), m_net_client->get_server_rank(), MessageType::REQ_GET, MessageKind::FUNCTION_INDEX);
    {
      std::stringstream ss;
      cereal::PortableBinaryOutputArchive wr(ss);
      std::pair<unsigned long, std::string> towr(m_pid, func_name);
      wr(towr);
      msg.set_msg(ss.str(), false);
    }

    m_net_client->send_and_receive(msg, msg);
    unsigned long global_idx = strToAny<unsigned long>(msg.buf());

    m_idxmap[local_idx] = global_idx;
    verboseStream << "ADglobalFunctionIndexMap rank " << rank << " local index " << local_idx << " maps to global idx " << global_idx << std::endl;

    return global_idx;
  }
  assert(0);
}      


std::vector<unsigned long> ADglobalFunctionIndexMap::lookup(const std::vector<unsigned long> &local_idx, const std::vector<std::string> &func_name){
  if(!connectedToPS()) return local_idx;
  if(func_name.size() != local_idx.size()) fatal_error("Size of input arrays differ");

  int rank = m_net_client->get_client_rank();

  std::vector<unsigned long> out(local_idx.size());

  //Get indices we already know and make a list of those we need to lookup
  std::vector<size_t> get_remote_offsets; //offsets within array local_idx/out
  std::vector<std::string> get_remote_func_names;
  for(size_t i=0;i<local_idx.size();i++){
    auto it = m_idxmap.find(local_idx[i]);
    if(it != m_idxmap.end()){
      out[i] = it->second;
    }else{
      get_remote_offsets.push_back(i);
      get_remote_func_names.push_back(func_name[i]);
    }
  }
  size_t n_lookup = get_remote_offsets.size();

  verboseStream << "ADglobalFunctionIndexMap rank " << rank << " found stored global indices for " << func_name.size()-n_lookup << " functions and need to look up " << n_lookup << " from pserver" << std::endl;  

  //Do remote lookup
  if(n_lookup){
    verboseStream << "ADglobalFunctionIndexMap rank " << rank << " looking up global indices for " << n_lookup << " functions" << std::endl;

    Message msg;
    msg.set_info(m_net_client->get_client_rank(), m_net_client->get_server_rank(), MessageType::REQ_GET, MessageKind::FUNCTION_INDEX);
    {
      std::stringstream ss;
      cereal::PortableBinaryOutputArchive wr(ss);
      wr(m_pid);
      wr(get_remote_func_names);
      msg.set_msg(ss.str(), false);
    }
    
    m_net_client->send_and_receive(msg, msg);
    
    std::vector<unsigned long> global_indices;
    {
      std::stringstream ss; ss << msg.buf();
      cereal::PortableBinaryInputArchive rd(ss);
      rd(global_indices);
    }    

    if(global_indices.size() != n_lookup) 
      fatal_error("Global indices returned not complete");

    for(size_t i=0;i<n_lookup;i++){
      out[ get_remote_offsets[i] ] = global_indices[i];
      m_idxmap[local_idx[ get_remote_offsets[i] ] ] = global_indices[i];
    }

    verboseStream << "ADglobalFunctionIndexMap rank " << rank << " received global indices for " << n_lookup << " functions" << std::endl;
  }

  return out;
}


unsigned long ADglobalFunctionIndexMap::lookup(const unsigned long local_idx) const{
  if(!connectedToPS()) return local_idx;
  auto it = m_idxmap.find(local_idx);
  if(it != m_idxmap.end()) return it->second;
  else throw std::runtime_error("ADglobalFunctionIndexMap::lookup (const version): local index " + anyToStr(local_idx) + " not in map");
}

  
