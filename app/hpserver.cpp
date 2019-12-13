#ifdef _USE_MPINET
#include "chimbuko/net/mpi_net.hpp"
#else
#include "chimbuko/net/zmq_net.hpp"
#include "chimbuko/net/hzmq_net.hpp"
#include "chimbuko/util/mtQueue.hpp"
#endif
#include <mpi.h>
#include "chimbuko/param/sstd_param.hpp"
#include <fstream>
#include <iostream>

#define PORT  5000
#define MPORT  6000

int main (int argc, char ** argv)
{
    chimbuko::SstdParam param;
    int nt = -1, n_ad_modules = 0;
    int world_size, world_rank, name_len, portn;
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    std::string logdir = "./";
    std::string ws_addr;
#ifdef _USE_MPINET
    int provided;
    chimbuko::MPINet net;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
#else
    chimbuko::ZMQNet net;
    chimbuko::hZMQNet bnet;

//    MPI_Init(NULL, NULL);
//
	 MPI_Init(&argc, &argv);
#endif

/*
 *	The following part is for h-- paramter server
 *	It takes world rank to produce the file which will 
 *	have port  number and server address
 *
 */

	mtQueue <std::string> qu;


	std::string worker_pserver ="wps.txt";
	std::string worker_port ="wport.txt";
	std::string master_pserver ="mps.txt";
	std::string master_port="mport.txt";


	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	MPI_Get_processor_name(processor_name, &name_len);


// the argument 1 will tell whether it is a worker parameter server or master paramter server
// "1" means its a client/worker parameter server  and "0" means it is a master parameter server
//
	int client=atoi(argv[1]);
	
// if client == 1  then we are starting worker parameter server
// if client == 0  then we are starting master parameter server
//
//
	if (!client)
		portn=MPORT+world_rank;
	else
		portn=PORT+world_rank;

	if (client){
		worker_pserver = worker_pserver+ std::to_string(world_rank);
		worker_port = worker_port+ std::to_string(world_rank);
	}

	std::ofstream filepointer;

	if (client){
		std::cout << "[client pserver]" << std::endl;
		filepointer.open(worker_pserver);
		filepointer << std::string(processor_name); 
		filepointer.close();

		filepointer.open(worker_port);
		filepointer << std::to_string(portn);
		filepointer.close();
	}
	else{
		std::cout <<"[master pserver]"<<std::endl;
		filepointer.open(master_pserver);
                filepointer << std::string(processor_name); 
                filepointer.close();

                filepointer.open(master_port);
                filepointer << std::to_string(portn);
                filepointer.close();

	}

	std::cout << "I am out "<<std::endl;

	std::string  port_addr="tcp://*:";
        port_addr = port_addr + std::to_string(portn);
	std::cout << port_addr << std::endl;  

    try {
        if (argc > 2) {
            nt = atoi(argv[1]);
        }
        if (argc > 3) {
            logdir = std::string(argv[2]);
        }
        if (argc > 4) {
            n_ad_modules = atoi(argv[3]);
            ws_addr = std::string(argv[4]);
        }

        if (nt <= 0) {
            nt = std::max(
                (int)std::thread::hardware_concurrency() - 5,
                1
            );
	    nt = 5; // puting this number for debugging
        }

        //nt = std::max((int)std::thread::hardware_concurrency() - 2, 1);
        std::cout << "Run parameter server with " << nt << " threads" << std::endl;
    //    if (ws_addr.size() && n_ad_modules)
    //    {
    //        std::cout << "Run anomaly statistics sender (ws @ " << ws_addr << ")." << std::endl;
    //        param.reset_anomaly_stat({n_ad_modules});
    //    }

        // Note: for some reasons, internal MPI initialization cause segmentation error!! 
        net.set_parameter( dynamic_cast<chimbuko::ParamInterface*>(&param) );
	

	if (client)
    	   	net.init(nullptr, nullptr, nt, qu);
	else
		net.init(nullptr, nullptr, nt);	


	char name[100];
	char port[100];
	char temp[400];

      //  net.run_stat_sender(ws_addr, false);
        if (client){
		std::ifstream ReadFile;

		ReadFile.open(master_pserver);
		ReadFile >> name;
		ReadFile.close();

		ReadFile.open(master_port);
		ReadFile >> port;
		ReadFile.close();

    		sprintf(temp, "tcp://%s:%s",name,port);
    		//printf("%s\n", temp);
		std::string temp2 = temp;
                //std::cout << temp2 << std::endl;

        	net.init_client_mode(temp2, qu);
	}
//	starting subsription and publishing protocol
//	
//
	if (client){
     		sprintf(temp, "tcp://%s:6000", name);
		bnet.init_sub(temp); 
	}
	else{
 		bnet.init_pub();
	}

#ifdef _PERF_METRIC
        	net.run(logdir);
#else
	//if (!client)
        	net.run(port_addr);
#endif

        // at this point, all pseudo AD modules finished sending 
        // anomaly statistics data
       // std::this_thread::sleep_for(std::chrono::seconds(1));
        //net.stop_stat_sender(1000);

        // could be output to a file
        if (client)
        	std::cout << "[client]Shutdown parameter server ..." << std::endl;
	else
		std::cout << "[master]Shutdown parameter server ..." << std::endl;
        //param.show(std::cout);
        std::ofstream o;
        o.open(logdir + "parameters.txt");
        if (o.is_open())
        {
            param.show(o);
            o.close();
        }
    }
    catch (std::invalid_argument &e)
    {
        std::cout << e.what() << std::endl;
        //todo: usages()
    }
    catch (std::ios_base::failure &e)
    {
        std::cout << "I/O base exception caught\n";
        std::cout << e.what() << std::endl;
    }
    catch (std::exception &e)
    {
        std::cout << "Exception caught\n";
        std::cout << e.what() << std::endl;
    }
    while (true){

	}
    //if (!client)
    	net.finalize();
#ifdef _USE_ZMQNET
    MPI_Finalize();
#endif
    return 0;
}
