//In this example the ranks are arrayed on a 2D grid with horizontal communications being performed by thread 0 and vertical by thread 1
//Anomalies are introduced randomly with a user-input chance
#include <iostream>
#include <math.h>
#include <random>
#include <string>
#include <cassert>
#include <mpi.h>
#include <sstream>
#include <thread>
#include <random>


template<typename T>
T strToAny(const std::string &s){
  T out;
  std::stringstream ss; ss << s; ss >> out;
  if(ss.fail()) throw std::runtime_error("Failed to parse \"" + s + "\"");
  return out;
}
template<>
inline std::string strToAny<std::string>(const std::string &s){ return s; }

int main(int argc, char **argv)
{
  if(argc != 5){
    std::cout << "Usage ./main <base size (MB)> <anomaly size mult> <anomaly chance> <# calls>" << std::endl;
    return 0;
  }

  size_t base_size_MB = strToAny<size_t>(argv[1]);
  size_t anomaly_mult = strToAny<size_t>(argv[2]);
  double anomaly_chance = strToAny<double>(argv[3]); //chance of being anomalous
  int ncalls = strToAny<int>(argv[4]);

  int provided;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided); 
  if(provided != MPI_THREAD_MULTIPLE) throw std::runtime_error("Could not obtain thread safe MPI instance");
  
  int glob_rank, glob_ranks;
  assert( MPI_Comm_rank(MPI_COMM_WORLD, &glob_rank) == MPI_SUCCESS );
  assert( MPI_Comm_size(MPI_COMM_WORLD, &glob_ranks) == MPI_SUCCESS );

  if(glob_ranks < 4) throw std::runtime_error("Number of ranks must be 4 or more");

  std::cout << "Rank " << glob_rank << " of " << glob_ranks << std::endl;
  
  //We want to organize the ranks into a 2D regular grid, hence the number of ranks must be the square of an integer
  int side_length = (int)sqrt(double(glob_ranks));
  if(side_length * side_length != glob_ranks) throw std::runtime_error("Number of ranks must be the square of an integer");
  
  //mapping = col + side_length * row
  int row = glob_rank % side_length;
  int col = glob_rank / side_length;

  std::cout << "Grid size " << side_length << " row " << row << " col " << col << std::endl;
  
  //Setup communicators between all elements of a column, and between all elements of a row
  std::vector<MPI_Comm> row_comms(side_length);
  std::vector<MPI_Comm> column_comms(side_length);
  for(int i=0;i<side_length;i++){
    assert( MPI_Comm_split( MPI_COMM_WORLD, row, glob_rank, &row_comms[i] ) == MPI_SUCCESS );
    assert( MPI_Comm_split( MPI_COMM_WORLD, col, glob_rank, &column_comms[i] ) == MPI_SUCCESS );
  }

  int rowcomm_rank, colcomm_rank;
  assert( MPI_Comm_rank(row_comms[row], &rowcomm_rank) == MPI_SUCCESS );
  assert( MPI_Comm_rank(column_comms[col], &colcomm_rank) == MPI_SUCCESS );

  std::cout << "Global rank " << glob_rank << " with row " << row << " col " << col << " is rank " << rowcomm_rank << " of row comm and rank " << colcomm_rank << " of col comm" << std::endl;

  //Within each communicator communicate the rank indices
  std::vector<int> rowcomm_ranks(side_length,0);
  std::vector<int> colcomm_ranks(side_length,0);
  rowcomm_ranks[row] = rowcomm_rank;
  colcomm_ranks[col] = colcomm_rank;
  assert( MPI_Allreduce(MPI_IN_PLACE, rowcomm_ranks.data(), side_length, MPI_INT, MPI_SUM, row_comms[row]) == MPI_SUCCESS );
  assert( MPI_Allreduce(MPI_IN_PLACE, colcomm_ranks.data(), side_length, MPI_INT, MPI_SUM, column_comms[col]) == MPI_SUCCESS );

  //Create enough space for max size send/recv
  size_t max_size = base_size_MB * anomaly_mult * 1024 * 1024;
  void* bufs[2] = { malloc(max_size), malloc(max_size) };

  //Have 1 thread that communicates between rows and one between columns
  std::vector<std::thread> threads(2);
  for(int i=0;i<2;i++){
    MPI_Comm *comm = i==0 ? &row_comms[row] : &column_comms[col];
    void* buf = bufs[i];
    std::vector<int> *rank_map_p = i==0 ? &rowcomm_ranks : &colcomm_ranks;

    threads[i] = std::thread([=]{
	std::string descr = i==0 ? "row" : "col";
	const std::vector<int> &rank_map = *rank_map_p;

	//Go round-robin over the nodes in the slice
	std::mt19937 eng(i); //every rank has the same random number generator
	std::uniform_real_distribution<double> dis(0.,1.);
	for(int c=0;c<ncalls;c++){
	  bool is_anomaly = dis(eng) < anomaly_chance;
	  size_t size_MB = base_size_MB *(is_anomaly ? anomaly_mult : 1);
	  size_t size_bytes = size_MB * 1024*1024;
	  int sender_sliceidx = c % side_length;
	  int sender_rank = rank_map[sender_sliceidx];
	  if(glob_rank == 0)  std::cout << "Call " << c << " " << descr << " comm bcast " << size_MB << " from slice idx " << sender_sliceidx << " rank " << sender_rank << std::endl;
	  MPI_Bcast(buf, size_bytes, MPI_BYTE, sender_rank , *comm);	  
	}
	MPI_Barrier(*comm);
      });
  }
  
  for(int i=0;i<2;i++)
    threads[i].join();

  MPI_Barrier(MPI_COMM_WORLD);

  free(bufs[0]);
  free(bufs[1]);

  std::cout << "Done" << std::endl;

  MPI_Finalize();
  
  return 0;
}
