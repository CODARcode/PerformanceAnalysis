//The parameter server main program. This program collects statistics from the node-instances of the anomaly detector
#include <chimbuko/pserver/PSstatSender.hpp>
#ifdef _USE_MPINET
#include <chimbuko/net/mpi_net.hpp>
#else
#include <chimbuko/net/zmq_net.hpp>
#endif
#include <mpi.h>
#include <chimbuko/param/sstd_param.hpp>
#include <fstream>

using namespace chimbuko;

int main (int argc, char ** argv){
  SstdParam param; //global collection of parameters used to identify anomalies
  GlobalAnomalyStats global_stats; //global anomaly statistics
    
  int nt = -1, n_ad_modules = 0;
  std::string logdir = ".";
  std::string ws_addr;
#ifdef _USE_MPINET
  int provided;
  MPINet net;
  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
#else
  ZMQNet net;
  MPI_Init(&argc, &argv);
#endif

  PSstatSender stat_sender;

  try {
    if (argc > 1) {
      nt = atoi(argv[1]);
    }
    if (argc > 2) {
      logdir = std::string(argv[2]);
    }
    if (argc > 3) {
      n_ad_modules = atoi(argv[3]);
      ws_addr = std::string(argv[4]);
    }

    if (nt <= 0) {
      nt = std::max(
		    (int)std::thread::hardware_concurrency() - 5,
		    1
		    );
    }

    //nt = std::max((int)std::thread::hardware_concurrency() - 2, 1);
    std::cout << "Run parameter server with " << nt << " threads" << std::endl;
    if (ws_addr.size() && n_ad_modules){
      std::cout << "Run anomaly statistics sender (ws @ " << ws_addr << ")." << std::endl;
      global_stats.reset_anomaly_stat({n_ad_modules});
    }

    net.add_payload(new NetPayloadUpdateParams(&param));
    net.add_payload(new NetPayloadGetParams(&param));
    net.add_payload(new NetPayloadUpdateAnomalyStats(&global_stats));
    net.init(nullptr, nullptr, nt);

    //Start sending anomaly statistics to viz
    stat_sender.add_payload(new PSstatSenderGlobalAnomalyStatsPayload(&global_stats));
    stat_sender.run_stat_sender(ws_addr);

    //Start communicating with the AD instances
#ifdef _PERF_METRIC
    net.run(logdir);
#else
    net.run();
#endif

    // at this point, all pseudo AD modules finished sending 
    // anomaly statistics data
    std::this_thread::sleep_for(std::chrono::seconds(1));
    stat_sender.stop_stat_sender(1000);

    // could be output to a file
    std::cout << "Shutdown parameter server ..." << std::endl;
    //param.show(std::cout);
    std::ofstream o;
    o.open(logdir + "/parameters.txt");
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

  net.finalize();
#ifdef _USE_ZMQNET
  MPI_Finalize();
#endif
  return 0;
}
