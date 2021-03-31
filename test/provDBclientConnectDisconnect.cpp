#include <cassert>
#include <chimbuko/ad/ADProvenanceDBclient.hpp>
#include <chimbuko/verbose.hpp>
#include <chimbuko/util/string.hpp>
#include <mpi.h>
#include <chrono>
#include <thread>

using namespace chimbuko;

//This test program just connects to the provDB, waits a short period and disconnects
//It is used to test options of the provdb_admin
int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);

  chimbuko::enableVerboseLogging() = true;
  assert(argc >= 3);
  std::string addr = argv[1];

  int shards = strToAny<int>(argv[2]);
  
  std::cout << "Provenance DB admin is on address: " << addr << std::endl;
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);


  ADProvenanceDBclient client(rank);
  std::cout << "Client with rank " << rank << " attempting connection" << std::endl;
  client.connect(addr, shards);

  std::this_thread::sleep_for( std::chrono::seconds(5));

  client.disconnect();

  MPI_Finalize();
  return 0;
}
