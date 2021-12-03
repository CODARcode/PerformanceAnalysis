//This example demonstrates the simulator using the ADSim API directly with multiple ranks and multiple threads per rank
//Anomalies are explicitly tagged
#include<chimbuko_config.h>
#ifdef USE_MPI
#include<mpi.h>
#endif
#include<sim.hpp>
#include<random>

using namespace chimbuko_sim;

int main(int argc, char **argv){
#ifdef USE_MPI
  MPI_Init(&argc, &argv);
#endif

  provDBsetup pdb_setup;
  int i=1;
  while(i<argc){
    std::string sarg(argv[i]);
    if(sarg == "-remote_provdb"){ //Allow for connection to an existing remote provDB server
      if(i+3 >= argc) fatal_error("Not enough arguments provided");
      pdb_setup.remote_server_addr_dir = argv[i+1];
      pdb_setup.remote_server_nshards = std::stoi(argv[i+2]);
      pdb_setup.remote_server_instances = std::stoi(argv[i+3]);
      pdb_setup.use_local = false;
      i+=4;
    }else{
      fatal_error(stringize("Unknown argument: %s",argv[i]));
    }
  }

  int window_size = 5; //number of events to record around an anomaly in the provenance data
  int pid = 0; //program index
  unsigned long program_start = 100;
  unsigned long step_freq = 1000;
 
  //Setup the "AD" instances
  int n_ranks = 4;
  int threads_per_rank = 2;

  std::vector<ADsim> ad;
  for(int r=0;r<n_ranks;r++)
    ad.push_back(ADsim(window_size, pid, r, program_start, step_freq, pdb_setup));

  //Setup some functions
  registerFunc("main", 500, 50, 100);
  registerFunc("child", 250, 25, 100);

  int anom_runtimes[3] = {500,750,1000};
  double anom_scores[3] = {2,3,4};

  //Setup some traces
  int nexec = 500;
  int anom_freq = 50;
    
  for(int rank=0;rank<n_ranks;rank++){
    for(int thread=0;thread<threads_per_rank;thread++){
      int anom_off = 0;

      unsigned long child_start = 200;

      unsigned long time = program_start;
      auto main = ad[rank].addExec(thread, "main", time, 0); //will update the end time later
      for(int i=0;i<nexec;i++){
	CallListIterator_t child;
	unsigned long runtime;
	if(i!=0 && i % anom_freq == 0){ //an anomaly!
	  runtime = anom_runtimes[anom_off];
	  double score = anom_scores[anom_off];	 
	  child = ad[rank].addExec(thread, "child", time, runtime, true, score);
	  
	  std::cout << rank << " " << thread << " " << i << " anomaly " << runtime << std::endl;
	  
	  anom_off = (anom_off + 1) % 3; //cycle through 3 times
	}else{
	  runtime = 250;
	  child = ad[rank].addExec(thread, "child", time, runtime);

	  std::cout << rank << " " << thread << " " << i << " normal " << runtime << std::endl;
	}
	ad[rank].bindParentChild(main, child);

	time = time + runtime + 1;
      }
      ad[rank].updateRuntime(main, time - program_start);
    }
  }

  //Run the simulation
  int nstep = ad[0].largest_step() + 1;
  for(int r=1;r<n_ranks;r++)
    nstep = std::max( (unsigned long)nstep, ad[r].largest_step() + 1);
  
  std::cout << "Got data for " << nstep << " steps" << std::endl;

  for(int i=0;i<nstep;i++){
    for(int r=0;r<n_ranks;r++){
      std::cout << "Rank " << r << " step " << i << " contains " << ad[r].nStepExecs(i) << " execs" << std::endl;
      ad[r].step(i);
    }
    //PServer dump its output
    getPserver().writeStreamingOutput();
  }

#ifdef USE_MPI
  MPI_Finalize();
#endif
  std::cout << "Done" << std::endl;
  return 0;
}
