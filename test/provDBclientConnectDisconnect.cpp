#include <chimbuko_config.h>
#include <cassert>
#include <chimbuko/ad/ADProvenanceDBclient.hpp>
#include <chimbuko/verbose.hpp>
#include <chimbuko/core/util/string.hpp>
#ifdef USE_MPI
#include <mpi.h>
#endif
#include <chrono>
#include <thread>

using namespace chimbuko;

//This test program just connects to the provDB, waits a short period and disconnects
//It is used to test options of the provdb_admin
int main(int argc, char** argv) {
#ifdef USE_MPI
  MPI_Init(&argc, &argv);
#endif

  chimbuko::enableVerboseLogging() = true;
  assert(argc >= 3);
  std::string addr = argv[1];

  int shards = strToAny<int>(argv[2]);
  
  std::cout << "Provenance DB admin is on address: " << addr << std::endl;
  int rank = 0;
#ifdef USE_MPI
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

  ADProvenanceDBclient client(rank);
  std::cout << "Client with rank " << rank << " attempting connection" << std::endl;
  client.connectSingleServer(addr, shards);

  std::this_thread::sleep_for( std::chrono::seconds(5));

  client.disconnect();

#ifdef USE_MPI
  MPI_Finalize();
#endif
  return 0;
}
