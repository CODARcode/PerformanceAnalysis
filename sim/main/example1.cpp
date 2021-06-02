//This example demonstrates the simulator using the "thread execution API"
#include<mpi.h>
#include<sim.hpp>

using namespace chimbuko_sim;

int main(int argc, char **argv){
  MPI_Init(&argc, &argv);

  int window_size = 5; //number of events to record around an anomaly in the provenance data
  int pid = 0; //program index
  
  //Setup the "AD" instances
  int n_ranks = 1;
  std::vector<ADsim> ad;
  for(int r=0;r<n_ranks;r++)
    ad.push_back(ADsim(window_size, pid, r));
  
  //Setup some functions, providing some fake statistics to attach to their provenance data
  registerFunc("main", 1000, 200, 20);
  registerFunc("child1", 500, 50, 200);
  registerFunc("child2", 600, 60, 300);

  //Setup some systems
  std::vector<int> cpu_threads(4); for(int i=0;i<4;i++) cpu_threads[i] = i;

  int gpus = 2;
  std::vector<int> gpu_threads(gpus); //assign virtual thread indices to gpus and register
  for(int g=0;g<gpus;g++){    
    gpu_threads[g] = cpu_threads.size() + g; //GPU virtual index should not correspond with a CPU core thread index
    ad[0].registerGPUthread(gpu_threads[g]);
  }

  threadExecution thr0(0, ad[0]); //thread 0 (a cpu thread) on first ad

  ad[0].beginStep(50);

  thr0.enterNormal("main", 100, "tag_main"); //events can be tagged and retrieved once exit has been called, allowing for external manipulation
  thr0.addCounter("main_counter", 1234, 120);
  thr0.addComm(CommType::Send, 1, 1024, 130);
  thr0.enterAnomaly("child1", 140, 1.99);
  thr0.exit(149);
  thr0.exit(150);
 
  auto it = thr0.getTagged("tag_main");
  std::cout << it << std::endl;

  ad[0].endStep(200);


  MPI_Finalize();
  return 0;
}
