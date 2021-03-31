#include <iostream>
#include <math.h>
#include <random>
#include <string>
#include <cassert>
#include <mpi.h>
#include <sstream>
#include <thread> 

using clock_value_t = long long;

__device__ void sleep(clock_value_t sleep_cycles)
{
  clock_value_t start = clock64();
  clock_value_t cycles_elapsed;
  do { cycles_elapsed = clock64() - start; } 
  while (cycles_elapsed < sleep_cycles);
}
__global__ void the_kernel(clock_value_t wait){
  sleep(wait);
}



int main(int argc, char **argv)
{
  assert(argc>=3);
  int cycles = std::stoi(argv[1]);
  int freq = std::stoi(argv[2]); //how frequently (in cycles) an outlier is inserted
  int ooffset = std::stoi(argv[3]); //how many cycles we wait for the first outlier to be inserted
  
  int nstreams = 2;

  int which_device =0; //which device to use
  int which_stream = 0;
  long long base_cycles = 1e8;
  int mult = 100;
  int arg = 4;
  while(arg < argc){
    std::string sarg = argv[arg];
    if(sarg == "-mult"){
      mult = std::stoi(argv[arg+1]);
      std::cout << "Set mult to " << mult << std::endl;
      arg+=2;
    }else if(sarg == "-base"){
      std::stringstream ss; ss << argv[arg+1]; ss >> base_cycles;
      std::cout << "Set base cycles to " << base_cycles << std::endl;
      arg+=2;
    }else if(sarg == "-streams"){
      nstreams = std::stoi(argv[arg+1]);
      std::cout << "Set streams to " << nstreams << std::endl;      
      arg+=2;
    }else if(sarg == "-device"){
      which_device = std::stoi(argv[arg+1]);
      std::cout << "Using device " << which_device << std::endl;
      arg+=2;
    }else if(sarg == "-stream"){
      which_stream = std::stoi(argv[arg+1]);
      std::cout << "Applying outliers to stream " << which_stream << std::endl;
      arg+=2;
    }else{
      std::cerr << "Unknown arg " << sarg << std::endl;
      exit(-1);
    }
  }
  
  MPI_Init(&argc, &argv); //tau plugin system is only initialized if MPI_Init called, even if not actually using MPI!

  int ndevice = 1;
  assert( cudaGetDeviceCount(&ndevice) == cudaSuccess );
  std::cout << "Number of devices " << ndevice << std::endl;

  if(which_device >= ndevice){
    std::cout << "Device index is larger than the number of devices!" << std::endl;
    exit(-1);
  }
  cudaSetDevice(which_device);
  
  std::vector<cudaStream_t> streams(nstreams);
  for(int s=0;s<nstreams;s++)
    cudaStreamCreate(&streams[s]);
  
  std::default_random_engine reng(1234);
  std::uniform_real_distribution<> uniform_dist(0., 1.0);

  //Each thread has a different stream
  std::thread sthreads[nstreams];
  for(int s=0;s<nstreams;s++){
    sthreads[s] = std::thread( [&,s](){
	for(int c=0;c<cycles;c++){
	  std::cout << "Stream thread " << s << " running kernel for cycle " << c << std::endl;
	  long long wait = base_cycles;
	  if(c>=ooffset  && (c - ooffset) % freq == 0 && s==which_stream)
	    wait *= mult;
	  
	  std::cout << "Issuing to stream " << s << " a wait of " << wait << " cycles" <<  std::endl;
	  
	  the_kernel<<<1, 1, 0, streams[s]>>>(wait);
	  std::cout << "Syncing stream " << s << std::endl;
	  cudaStreamSynchronize(streams[s]);
	  std::cout << "Syncing stream " << s << " complete" << std::endl;
	}
      });
  }

  for(int s=0;s<nstreams;s++)
    sthreads[s].join();

  for(int s=0;s<nstreams;s++)
    cudaStreamDestroy(streams[s]);

  
  std::cout << "Done" << std::endl;

  MPI_Finalize();
  
  return 0;
}
