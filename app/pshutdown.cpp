#include "chimbuko/verbose.hpp"
#include "chimbuko/util/error.hpp"
#include "chimbuko/ad/ADNetClient.hpp"

using namespace chimbuko;

int main(int argc, char** argv){
#ifdef _USE_ZMQNET
  if(argc != 2){
    std::cout << "Usage: pserver_shutdown <server address e.g. tcp://localhost:5559>" << std::endl;
    return 1;
  }
  ADNetClient ad;
  ad.connect_ps(0,0,argv[1]);
  ad.stopServer();
  ad.disconnect_ps();

  return 0;
#else
  std::cout << "Not implemented for MPINET" << std::endl;
  return 1;
#endif
}
