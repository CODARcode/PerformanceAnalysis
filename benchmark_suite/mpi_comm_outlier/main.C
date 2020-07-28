//In this example, the ranks are distributed on a torus and send to their neighbor to the right
//The communication is performed in 2 stages, first the amount of data is sent, and second the actual data is sent
//The anomaly is introduced by having one rank periodically send much more data

#include<mpi.h>
#include<iostream>
#include<cassert>
#include<sstream>

void mpi_error_check(int err){
  //std::cout << "Comm error status int " << err << " (MPI_SUCCESS=" << MPI_SUCCESS << ")" << std::endl;
  if(err != MPI_SUCCESS){
    char estring[MPI_MAX_ERROR_STRING];
    int eclass,len;
    MPI_Error_class(err, &eclass);
    MPI_Error_string(err, estring, &len);
    printf("Error %d: %s\n", eclass, estring);fflush(stdout);
    exit(1);
  }
}

template<typename T>
T strToAny(const std::string &s){
  T out;
  std::stringstream ss; ss << s; ss >> out;
  if(ss.fail()) throw std::runtime_error("Failed to parse \"" + s + "\"");
  return out;
}
template<>
inline std::string strToAny<std::string>(const std::string &s){ return s; }



//Send to the right and receive from the left
//The amount of data sent is determined by the present rank, and the amount received is determined from a separate communication from the left neighbor
//Returns pointer to newly allocated receive buffer
void* torus_comm(const int rank,
		const int lneighbor, const int rneighbor,
		size_t bytes_send, void *send_buf){
  //First communicate the number of bytes
  uint64_t bytes_send_u = bytes_send, bytes_recv_u;
  mpi_error_check( MPI_Sendrecv(&bytes_send_u, 1, MPI_UINT64_T, rneighbor, 0,
			 &bytes_recv_u, 1, MPI_UINT64_T, lneighbor, 0,
				MPI_COMM_WORLD, MPI_STATUS_IGNORE) );
  

  std::cout << "Rank " << rank << " receiving " << bytes_recv_u << " from left neighbor " << lneighbor << " and sending " << bytes_send_u << " to right neighbor " << rneighbor << std::endl;

  //Then communicate the data
  size_t bytes_recv = bytes_recv_u;
  void *recv_buf = malloc(bytes_recv);

  mpi_error_check(MPI_Sendrecv(send_buf, bytes_send, MPI_BYTE, rneighbor, 1,
		      recv_buf, bytes_recv, MPI_BYTE, lneighbor, 1,
		      MPI_COMM_WORLD, MPI_STATUS_IGNORE) );
  return recv_buf;
}

int main(int argc, char **argv){
  if(argc < 5){
    std::cout << "Usage: <binary> <ncycles> <anom rank index> <anom mult> <anom freq>" << std::endl;
    exit(0);
  }
  int cycles = strToAny<int>(argv[1]);
  int anom_rank = strToAny<int>(argv[2]);
  int anom_mult = strToAny<int>(argv[3]);
  int anom_freq = strToAny<int>(argv[4]);

  MPI_Init(&argc, &argv);
  
  //MPI_Errhandler_set(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
  
  int rank, ranks;
  assert( MPI_Comm_rank(MPI_COMM_WORLD, &rank) == MPI_SUCCESS );
  assert( MPI_Comm_size(MPI_COMM_WORLD, &ranks) == MPI_SUCCESS );
  int rneighbor = (rank + 1) % ranks;
  int lneighbor = (rank - 1 + ranks) % ranks;
  
  std::cout << "Rank " << rank << " left neighbor is " << lneighbor << " and right neighbor " << rneighbor << std::endl;

  for(int i=0;i<cycles;i++){
    size_t send_bytes = 1024*1024;
    if(rank == anom_rank && i > 0 && i % anom_freq == 0)
      send_bytes *= anom_mult;

    void* send_buf = malloc(send_bytes);
    
    void* recv_buf = torus_comm(rank, lneighbor, rneighbor, send_bytes, send_buf);

    free(send_buf);
    free(recv_buf);
  }
  
  MPI_Finalize();
}
