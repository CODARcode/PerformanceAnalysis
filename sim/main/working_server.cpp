//This example demonstrates the simulator using the ADSim API directly, and using a real outlier algorithm
#include<mpi.h>
#include<sim.hpp>
#include<random>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <string>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/archives/binary.hpp>
#include <fstream>
#include <nlohmann/json.hpp>
#include <time.h>
#include <iomanip>

#define SERVER_PORT htons(5002)

using namespace chimbuko_sim;





int main(int argc, char **argv){
  MPI_Init(&argc, &argv);

  //Server socket configuration
  char buffer[1000000];
  int n;

  int serverSock=socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = SERVER_PORT;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  
  /* bind (this socket, local address, address length)
           bind server socket (serverSock) to server address (serverAddr).
           Necessary so that server can use a specific port */
  bind(serverSock, (struct sockaddr*)&serverAddr, sizeof(struct sockaddr));

  // wait for a client
  /* listen (this socket, request queue length) */
  listen(serverSock,1);

  sockaddr_in clientAddr;
  socklen_t sin_size=sizeof(struct sockaddr_in);
  int clientSock=accept(serverSock,(struct sockaddr*)&clientAddr, &sin_size);

  int initialized = 0;

  int window_size = 5; //number of events to record around an anomaly in the provenance data
  int pid = 0; //program index
  unsigned long program_start = 100;
  unsigned long step_freq = 1000;

  std::cout << "Initializing server from config file" <<std::endl;
  //Read from server configuration file
  std::ifstream file("server_config.json");
  nlohmann::json configs = nlohmann::json::parse(file);

  //Set the AD algorithm *before instantiating the simulator!
  ADalgParams & alg = adAlgorithmParams();
  alg.algorithm = configs["algorithm"]; 
  alg.sstd_sigma = configs["sstd_sigma"];
  
  //Setup the "AD" instances
  int n_ranks = configs["ranks"];
  std::vector<ADsim> ad;
  for(int r=0;r<n_ranks;r++)
    ad.push_back(ADsim(window_size, pid, r, program_start, step_freq));

  //Setup some functions
  const std::string func = configs["func_name"];
  registerFunc("child");

  //Setup some systems
  std::vector<int> cpu_threads(4); for(int i=0;i<4;i++) cpu_threads[i] = i;

  int start_val = 200;
  int step_index = 0;
  int f_count = 0;
  std::cout << "Initialization Complete." << std::endl;
  
  while (1)
  {
	bzero(buffer, 1000000);

	//receive a message from a client
        n = read(clientSock, buffer, 999990);
	std::cout << "Confirmation code  " << n << std::endl;
        //std::cout << "Server received:  " << buffer << std::endl;

        // use json parser
        nlohmann::json j = nlohmann::json::parse(buffer);
	std::cout << "Parsed by JSON Parser: " << j.dump() << std::endl;

	std::cout << "Length of data: " << j["data"].size() << std::endl;
	

	int nexecs = j["data"].size();
	std::cout << "Total number to Executions : " << nexecs << std::endl;

	std::vector<int> starts(nexecs);
	std::vector<int> runtimes(nexecs);
	
	for (int i=0; i<nexecs; i++) {
		if(i==0) starts[i] = 200;
		else starts[i] = starts[i-1] + runtimes[i-1] + 1;
		runtimes[i] = j["data"][i];
	}

 	for(int i=0;i<nexecs;i++){
		//std::cout <<"adExec iteration: "<< i << std::endl;
    		CallListIterator_t child = ad[0].addExec(0, "child", starts[i], runtimes[i]);
  	}
	std::cout << "Done adExec" << std::endl;

  	//Run the simulation
  	int nstep = ad[0].largest_step() + 1;
  	std::cout << "Got data for " << nstep << " steps" << std::endl;
	
  	for(int i=0;i<nstep;i++){

		std::cout << "Step " << i << " contains " << ad[0].nStepExecs(i) << " execs" << std::endl;
    		ad[0].step(i);
		std::cout << "Finished step" <<std::endl;
  	}
	
      	//Send ACK message back to client 
	strcpy(buffer, "0");
       	n = write(clientSock, buffer, strlen(buffer));
	std::cout << "Confirmation code  " << n << std::endl;

	
  }

  MPI_Finalize();
  std::cout << "Done" << std::endl;
  return 0;
}
