#include "chimbuko/ad/ADProvenanceDBclient.hpp"

using namespace chimbuko;

int main(int argc, char** argv){
#ifdef ENABLE_PROVDB
#  ifdef ENABLE_MARGO_STATE_DUMP
  if(argc != 2){
    std::cout << "Usage: provdb_dump_state <server address e.g. ofi+tcp;ofi_rxm://172.17.0.4:5000>" << std::endl;
    return 1;
  }
  ADProvenanceDBclient ad(0);
  ad.connect(argv[1], 1);
  ad.serverDumpState();
  ad.disconnect();

  return 0;
#  else
  std::cout << "Margo dump configuration option is disabled" << std::endl;
  return 1;
#  endif
#else
  std::cout << "ProvDB not in use" << std::endl;
  return 1;
#endif
}
