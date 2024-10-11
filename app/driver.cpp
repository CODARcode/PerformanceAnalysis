#include<chimbuko_config.h>
#ifdef USE_MPI
#include<mpi.h>
#endif
#include "chimbuko/core/chimbuko.hpp"
#include "chimbuko/core/verbose.hpp"
#include "chimbuko/core/util/string.hpp"
#include "chimbuko/core/util/error.hpp"
#include "chimbuko/modules/factory.hpp"
#include <chrono>
#include <cstdlib>

using namespace chimbuko;
using namespace std::chrono;


int main(int argc, char ** argv){
#ifdef USE_MPI
  assert( MPI_Init(&argc, &argv) == MPI_SUCCESS );
#endif

  //Parse environment variables
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE"))
    enableVerboseLogging() = true;

  //Instantiate Chimbuko
  std::unique_ptr<ChimbukoBase> driver = modules::factoryInstantiateChimbuko(argv[1], argc-2, argv+2);

  const ChimbukoBaseParams &base_params = driver->getBaseParams();
  int rank = base_params.rank;
  if(enableVerboseLogging()){
    headProgressStream(rank) << "Driver rank " << rank << ": Enabling verbose debug output" << std::endl;
  }

  verboseStream << "Driver rank " << rank << ": waiting at pre-run barrier" << std::endl;
#ifdef USE_MPI
  MPI_Barrier(MPI_COMM_WORLD);
  int mpi_world_rank, mpi_world_size;
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_world_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_world_size);
#endif

  bool error = false;
  try
    {


      // -----------------------------------------------------------------------
      // Measurement variables
      // -----------------------------------------------------------------------
      unsigned long total_frames = 0, frames = 0;
      unsigned long total_n_outliers = 0, n_outliers = 0;
      unsigned long total_processing_time = 0, processing_time = 0;
      high_resolution_clock::time_point t1, t2;

      // -----------------------------------------------------------------------
      // Start analysis
      // -----------------------------------------------------------------------
      headProgressStream(rank) << "Driver rank " << rank << ": analysis start" << std::endl;

      t1 = high_resolution_clock::now();
      driver->run();
      t2 = high_resolution_clock::now();

      headProgressStream(rank) << "Driver rank " << rank << ": analysis done!\n";

      // -----------------------------------------------------------------------
      // Average analysis time and total number of outliers
      // -----------------------------------------------------------------------
      n_outliers = driver->getBaseRunStats().n_outliers;
      frames = driver->getStep();
      processing_time = duration_cast<milliseconds>(t2 - t1).count();

#ifdef USE_MPI
      verboseStream << "Driver rank " << rank << ": waiting at post-run barrier" << std::endl;
      MPI_Barrier(MPI_COMM_WORLD);

      {
	const unsigned long local_measures[] = {processing_time, n_outliers, frames};
	unsigned long global_measures[] = {0, 0, 0};
	MPI_Reduce(
		   local_measures, global_measures, 3, MPI_UNSIGNED_LONG,
		   MPI_SUM, 0, MPI_COMM_WORLD
		   );
	total_processing_time = global_measures[0];
	total_n_outliers = global_measures[1];
	total_frames = global_measures[2];
      }
#else
      //Without MPI only report local parameters. In principle we could aggregate using the Pserver if we really want the global information
      total_processing_time = processing_time;
      total_n_outliers = n_outliers;
      total_frames = frames;

      int mpi_world_size = 1;
#endif


      headProgressStream(rank) << "Driver rank " << rank << ": Final report\n"
			       << "Avg. num. frames per rank : " << (double)total_frames/(double)mpi_world_size << "\n"
			       << "Avg. processing time per rank : " << (double)total_processing_time/(double)mpi_world_size << " msec\n"
			       << "Total num. outliers  : " << total_n_outliers << "\n"
			       << std::endl;
    }
  catch (const std::invalid_argument &e){
    std::cout << '[' << getDateTime() << ", rank " << rank << "] Driver : caught invalid argument: " << e.what() << std::endl;
    if(base_params.err_outputpath.size()) recoverable_error(std::string("Driver : caught invalid argument: ") + e.what()); //ensure errors also written to error logs
    error = true;
  }
  catch (const std::ios_base::failure &e){
    std::cout << '[' << getDateTime() << ", rank " << rank << "] Driver : I/O base exception caught: " << e.what() << std::endl;
    if(base_params.err_outputpath.size()) recoverable_error(std::string("Driver : I/O base exception caught: ") + e.what());
    error = true;
  }
  catch (const std::exception &e){
    std::cout << '[' << getDateTime() << ", rank " << rank << "] Driver : Exception caught: " << e.what() << std::endl;
    if(base_params.err_outputpath.size()) recoverable_error(std::string("Driver : Exception caught: ") + e.what());
    error = true;
  }

#ifdef USE_MPI
  MPI_Finalize();
#endif
  headProgressStream(rank) << "Driver is exiting" << std::endl;
  return error ? 1 : 0;
}
