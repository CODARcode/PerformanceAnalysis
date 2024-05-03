#include "chimbuko/core/ad/ADProvenanceDBclient.hpp"

using namespace chimbuko;

int main(int argc, char** argv){
#ifdef ENABLE_PROVDB
  if(argc != 2){
    std::cout << "Usage: provdb_shutdown <server address e.g. ofi+tcp;ofi_rxm://172.17.0.4:5000>" << std::endl;
    return 1;
  }
  ADProvenanceDBclient ad(0);
  ad.setEnableHandshake(false);
  ad.connectSingleServer(argv[1], 1);
  ad.stopServer();
  ad.disconnect();

  return 0;
#else
  std::cout << "ProvDB not in use" << std::endl;
  return 1;
#endif
}

