//This example demonstrates the simulator using the ADSim API directly, and using a real outlier algorithm
#include<mpi.h>
#include<sim.hpp>
#include<random>

using namespace chimbuko_sim;

int main(int argc, char **argv){
  MPI_Init(&argc, &argv);

  int window_size = 5; //number of events to record around an anomaly in the provenance data
  int pid = 0; //program index
  unsigned long program_start = 100;
  unsigned long step_freq = 1000;

  //Set the AD algorithm *before instantiating the simulator!
  ADalgParams & alg = adAlgorithmParams();
  alg.algorithm = "sstd";
  alg.sstd_sigma = 12.0;
  
  //Setup the "AD" instances
  int n_ranks = 1;
  std::vector<ADsim> ad;
  for(int r=0;r<n_ranks;r++)
    ad.push_back(ADsim(window_size, pid, r, program_start, step_freq));

  //Setup some functions
  registerFunc("main");
  registerFunc("child");

  //Setup some systems
  std::vector<int> cpu_threads(4); for(int i=0;i<4;i++) cpu_threads[i] = i;

  int gpus = 2;
  std::vector<int> gpu_threads(gpus); //assign virtual thread indices to gpus and register
  for(int g=0;g<gpus;g++){    
    gpu_threads[g] = cpu_threads.size() + g; //GPU virtual index should not correspond with a CPU core thread index
    ad[0].registerGPUthread(gpu_threads[g]);
  }

  std::default_random_engine gen;
  std::normal_distribution<double> dist(100,20); //normal std.dev = 20

  unsigned long child_start = 200;
  int nexec = 500;
  std::vector<int> starts(nexec);
  std::vector<int> runtimes(nexec);
  for(int i=0;i<nexec;i++){
    if(i==0) starts[i] = child_start;
    else starts[i] = starts[i-1] + runtimes[i-1] + 1;
    runtimes[i] = int( dist(gen) );
  }
  //Add an anomaly
  starts.push_back( starts.back() + runtimes.back() + 1 );
  runtimes.push_back( 400 );    
  nexec++;

  int program_end = starts.back() + runtimes.back() + 50;

  auto main = ad[0].addExec(0, "main", program_start, program_end - program_start);
  for(int i=0;i<nexec;i++){
    CallListIterator_t child = ad[0].addExec(0, "child", starts[i], runtimes[i]);
    ad[0].bindParentChild(main, child);
  }

  //Run the simulation
  int nstep = ad[0].largest_step() + 1;
  std::cout << "Got data for " << nstep << " steps" << std::endl;

  for(int i=0;i<nstep;i++){
    std::cout << "Step " << i << " contains " << ad[0].nStepExecs(i) << " execs" << std::endl;
    ad[0].step(i);
  }

  MPI_Finalize();
  return 0;
}
