#include <iostream>
#include <math.h>
#include <random>
#include <string>
#include <cassert>
#include <mpi.h>
#include <sstream>
#include <chrono>
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
  MPI_Init(&argc, &argv); //tau plugin system is only initialized if MPI_Init called, even if not actually using MPI!
  assert(argc>=3);
  int cycles = std::stoi(argv[1]);
  int freq = std::stoi(argv[2]); //how frequently (in cycles) an outlier is inserted
  int ooffset = std::stoi(argv[3]); //how many cycles we wait for the first outlier to be inserted
  
  int device_max = -1;

  int which_device =0; //to which device is the outlier inserted
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
    }else if(sarg == "-device_max"){
      device_max = std::stoi(argv[arg+1]);
      std::cout << "Set device max to " << device_max << std::endl;      
      arg+=2;
    }else if(sarg == "-device"){
      which_device = std::stoi(argv[arg+1]);
      std::cout << "Outliers are being applied to device " << which_device << std::endl;
      arg+=2;
    }else{
      std::cerr << "Unknown arg " << sarg << std::endl;
      exit(-1);
    }
  }

  //Have an initial wait to give the AD time to connect, such that it doesn't miss data at the start of the loop
  std::this_thread::sleep_for( std::chrono::milliseconds(5000) );
  
  int ndevice = 1;
  assert( cudaGetDeviceCount(&ndevice) == cudaSuccess );
  std::cout << "Number of devices " << ndevice << std::endl;

  if(device_max != -1 && ndevice > device_max){
    ndevice = device_max;
    std::cout << "Constraining to " << ndevice << " devices" << std::endl;
  }

  float *x;
  cudaMallocManaged(&x, sizeof(float)); ///necessary to stop adios2 hanging!
  
  std::default_random_engine reng(1234);
  std::uniform_real_distribution<> uniform_dist(0., 1.0);

  for(int c=0;c<cycles;c++){
    std::cout << "Running kernel for cycle " << c << std::endl;
    for(int d=0;d<ndevice;d++){
      //long long wait = base_cycles * uniform_dist(reng);

      long long wait = base_cycles;
      if(c>=ooffset  && (c - ooffset) % freq == 0 && d==which_device)
	wait *= mult;
      
      std::cout << "Issuing to device " << d << " a wait of " << wait << " cycles" <<  std::endl;
      cudaSetDevice(d);
      the_kernel<<<1, 1>>>(wait);
    }
    
    for(int d=0;d<ndevice;d++){
      std::cout << "Syncing device " << d << std::endl;
      cudaSetDevice(d);
      cudaDeviceSynchronize();
    }
    std::cout << "Sync complete" << std::endl;
  }

  cudaFree(x);
  //cudaFree(y);

  
  std::cout << "Done" << std::endl;

  MPI_Finalize();
  
  return 0;
}
