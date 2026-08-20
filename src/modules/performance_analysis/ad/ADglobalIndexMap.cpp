#include <chimbuko/modules/performance_analysis/ad/ADglobalIndexMap.hpp>
#include <chimbuko/modules/performance_analysis/pserver/PScommon.hpp>
#include <chimbuko/core/util/error.hpp>
#include <cereal/archives/portable_binary.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/utility.hpp>

using namespace chimbuko;
using namespace chimbuko::modules::performance_analysis;

unsigned long ADglobalIndexMap::lookup(const unsigned long local_idx, const std::string &name){
  if(!connectedToPS()) return local_idx;

  int rank = m_net_client->get_client_rank();
  auto it = m_idxmap.find(local_idx);
  if(it != m_idxmap.end()){
    verboseStream << "ADglobalIndexMap rank " << rank << " local index " << local_idx << " already in map, maps to global index " << it->second << std::endl;
    return it->second;
  }else{
    //Obtain the index from the pserver
    verboseStream << "ADglobalIndexMap rank " << rank << " obtaining global index corresponding to local index " << local_idx << std::endl;

    Message msg;
    msg.set_info(m_net_client->get_client_rank(), m_net_client->get_server_rank(), MessageType::REQ_GET, m_msg_kind);
    {
      std::stringstream ss;
      cereal::PortableBinaryOutputArchive wr(ss);
      std::pair<unsigned long, std::string> towr(m_pid, name);
      wr(towr);
      msg.setContent(ss.str());
    }

    m_net_client->send_and_receive(msg, msg);
    unsigned long global_idx = strToAny<unsigned long>(msg.getContent());

    m_idxmap[local_idx] = global_idx;
    verboseStream << "ADglobalIndexMap rank " << rank << " local index " << local_idx << " maps to global idx " << global_idx << std::endl;

    return global_idx;
  }
  assert(0);
}      


std::vector<unsigned long> ADglobalIndexMap::lookup(const std::vector<unsigned long> &local_idx, const std::vector<std::string> &name){
  if(!connectedToPS()) return local_idx;
  if(name.size() != local_idx.size()) fatal_error("Size of input arrays differ");

  int rank = m_net_client->get_client_rank();

  std::vector<unsigned long> out(local_idx.size());

  //Get indices we already know and make a list of those we need to lookup
  std::vector<size_t> get_remote_offsets; //offsets within array local_idx/out
  std::vector<std::string> get_remote_names;
  for(size_t i=0;i<local_idx.size();i++){
    auto it = m_idxmap.find(local_idx[i]);
    if(it != m_idxmap.end()){
      out[i] = it->second;
    }else{
      get_remote_offsets.push_back(i);
      get_remote_names.push_back(name[i]);
    }
  }
  size_t n_lookup = get_remote_offsets.size();

  verboseStream << "ADglobalIndexMap rank " << rank << " found stored global indices for " << name.size()-n_lookup << " names and need to look up " << n_lookup << " from pserver" << std::endl;  

  //Do remote lookup
  if(n_lookup){
    verboseStream << "ADglobalIndexMap rank " << rank << " looking up global indices for " << n_lookup << " names" << std::endl;

    Message msg;
    msg.set_info(m_net_client->get_client_rank(), m_net_client->get_server_rank(), MessageType::REQ_GET, m_msg_kind);
    {
      std::stringstream ss;
      cereal::PortableBinaryOutputArchive wr(ss);
      wr(m_pid);
      wr(get_remote_names);
      msg.setContent(ss.str());
    }
    
    m_net_client->send_and_receive(msg, msg);
    
    std::vector<unsigned long> global_indices;
    {
      std::stringstream ss; ss << msg.getContent();
      cereal::PortableBinaryInputArchive rd(ss);
      rd(global_indices);
    }    

    if(global_indices.size() != n_lookup) 
      fatal_error("Global indices returned not complete");

    for(size_t i=0;i<n_lookup;i++){
      out[ get_remote_offsets[i] ] = global_indices[i];
      m_idxmap[local_idx[ get_remote_offsets[i] ] ] = global_indices[i];
    }

    verboseStream << "ADglobalIndexMap rank " << rank << " received global indices for " << n_lookup << " names" << std::endl;
  }

  return out;
}


unsigned long ADglobalIndexMap::lookup(const unsigned long local_idx) const{
  if(!connectedToPS()) return local_idx;
  auto it = m_idxmap.find(local_idx);
  if(it != m_idxmap.end()) return it->second;
  else throw std::runtime_error("ADglobalIndexMap::lookup (const version): local index " + anyToStr(local_idx) + " not in map");
}

  














unsigned long ADglobalStringIndexMap::lookup(const std::string &name){
  if(!connectedToPS()){
    auto it = m_idxmap.find(name);
    if(it == m_idxmap.end()){
      unsigned long idx = m_disconnected_globidx++;
      m_idxmap[name] = idx;
      return idx;
    }else{
      return it->second;
    }
  }

  int rank = m_net_client->get_client_rank();
  auto it = m_idxmap.find(name);
  if(it != m_idxmap.end()){
    verboseStream << "ADglobalStringIndexMap rank " << rank << " name " << name << " already in map, maps to global index " << it->second << std::endl;
    return it->second;
  }else{
    //Obtain the index from the pserver
    verboseStream << "ADglobalStringIndexMap rank " << rank << " obtaining global index corresponding to name " << name << std::endl;

    Message msg;
    msg.set_info(m_net_client->get_client_rank(), m_net_client->get_server_rank(), MessageType::REQ_GET, m_msg_kind);
    {
      std::stringstream ss;
      cereal::PortableBinaryOutputArchive wr(ss);
      std::pair<unsigned long, std::string> towr(m_pid, name);
      wr(towr);
      msg.setContent(ss.str());
    }

    m_net_client->send_and_receive(msg, msg);
    unsigned long global_idx = strToAny<unsigned long>(msg.getContent());

    m_idxmap[name] = global_idx;
    verboseStream << "ADglobalStringIndexMap rank " << rank << " name " << name << " maps to global idx " << global_idx << std::endl;

    return global_idx;
  }
  assert(0);
}

void ADglobalStringIndexMap::linkNetClient(ADNetClient *net_client)
{
  assert(m_idxmap.size() == 0); // fail if we've already started assigning a local mapping
  m_net_client = net_client;
}

std::vector<unsigned long> ADglobalStringIndexMap::lookup(const std::vector<std::string> &name){
  std::vector<unsigned long> out(name.size());

  if (!connectedToPS())
  {
    int i=0;
    for (auto const &nm : name)
    {
      auto it = m_idxmap.find(nm);
      if (it == m_idxmap.end())
      {
        unsigned long idx = m_disconnected_globidx++;
        m_idxmap[nm] = idx;
        out[i++] = idx;
      }
      else
      {
        out[i++] = it->second;
      }
    }
    return out;
  }

  int rank = m_net_client->get_client_rank();
  
  //Get indices we already know and make a list of those we need to lookup
  std::vector<size_t> get_remote_offsets; //offsets within array local_idx/out
  std::vector<std::string> get_remote_names;
  for(size_t i=0;i<name.size();i++){
    auto it = m_idxmap.find(name[i]);
    if(it != m_idxmap.end()){
      out[i] = it->second;
    }else{
      get_remote_offsets.push_back(i);
      get_remote_names.push_back(name[i]);
    }
  }
  size_t n_lookup = get_remote_offsets.size();

  verboseStream << "ADglobalStringIndexMap rank " << rank << " found stored global indices for " << name.size()-n_lookup << " names and need to look up " << n_lookup << " from pserver" << std::endl;  

  //Do remote lookup
  if(n_lookup){
    verboseStream << "ADglobalStringIndexMap rank " << rank << " looking up global indices for " << n_lookup << " names" << std::endl;

    Message msg;
    msg.set_info(m_net_client->get_client_rank(), m_net_client->get_server_rank(), MessageType::REQ_GET, m_msg_kind);
    {
      std::stringstream ss;
      cereal::PortableBinaryOutputArchive wr(ss);
      wr(m_pid);
      wr(get_remote_names);
      msg.setContent(ss.str());
    }
    
    m_net_client->send_and_receive(msg, msg);
    
    std::vector<unsigned long> global_indices;
    {
      std::stringstream ss; ss << msg.getContent();
      cereal::PortableBinaryInputArchive rd(ss);
      rd(global_indices);
    }    

    if(global_indices.size() != n_lookup) 
      fatal_error("Global indices returned not complete");

    for(size_t i=0;i<n_lookup;i++){
      out[ get_remote_offsets[i] ] = global_indices[i];
      m_idxmap[name[ get_remote_offsets[i] ] ] = global_indices[i];
    }

    verboseStream << "ADglobalStringIndexMap rank " << rank << " received global indices for " << n_lookup << " names" << std::endl;
  }

  return out;
}

