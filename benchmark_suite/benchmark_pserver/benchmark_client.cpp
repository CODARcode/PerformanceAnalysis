//A fake AD that sends data to the pserver at a regular cadence
#include<mpi.h>
#include<chimbuko/ad/ADNetClient.hpp>
#include<chimbuko/ad/utils.hpp>
#include<chimbuko/util/commandLineParser.hpp>
#include<chimbuko/param/sstd_param.hpp>
#include<chimbuko/param/hbos_param.hpp>
#include<chimbuko/param/copod_param.hpp>
#include<chimbuko/ad/ADEvent.hpp>
#include<chimbuko/util/Anomalies.hpp>
#include<chimbuko/ad/ADLocalFuncStatistics.hpp>
#include<chimbuko/ad/ADLocalCounterStatistics.hpp>
#include<chimbuko/ad/ADLocalAnomalyMetrics.hpp>
#include<chimbuko/ad/ADcombinedPSdata.hpp>
#include<chimbuko/ad/ADglobalFunctionIndexMap.hpp>
#include<chimbuko/verbose.hpp>
#include "gtest/gtest.h"
#include<unit_test_common.hpp>

using namespace chimbuko;

struct Args{
  std::string pserver_addr;
  int cycles;
  int nfuncs;
  int ncounters;
  int nanomalies_per_func;
  size_t cycle_time_ms;
  int hpserver_nthr;
  int perf_write_freq;
  std::string perf_dir;
  std::string benchmark_pattern;
  std::string algorithm;
  int hbos_bins;

  Args(){
    cycles = 10;
    nfuncs = 100;
    ncounters = 100;
    nanomalies_per_func = 2;
    cycle_time_ms = 1000;
    hpserver_nthr = 1;
    perf_write_freq = 10;
    perf_dir=".";
    benchmark_pattern = "ad";
    algorithm = "sstd";
    hbos_bins = 20;
  }
};

int main(int argc, char **argv){
  if(const char* env_p = std::getenv("CHIMBUKO_VERBOSE")){
    std::cout << "Enabling verbose debug output" << std::endl;
    enableVerboseLogging() = true;
  }       

  MPI_Init(&argc, &argv);
  {

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    commandLineParser<Args> cmdline;
    addMandatoryCommandLineArgDefaultHelpString(cmdline, pserver_addr);
    addOptionalCommandLineArgDefaultHelpString(cmdline, cycles);
    addOptionalCommandLineArgDefaultHelpString(cmdline, nfuncs);
    addOptionalCommandLineArgDefaultHelpString(cmdline, ncounters);
    addOptionalCommandLineArgDefaultHelpString(cmdline, nanomalies_per_func);
    addOptionalCommandLineArgDefaultHelpString(cmdline, cycle_time_ms);
    addOptionalCommandLineArgDefaultHelpString(cmdline, hpserver_nthr);
    addOptionalCommandLineArgDefaultHelpString(cmdline, perf_write_freq);
    addOptionalCommandLineArgDefaultHelpString(cmdline, perf_dir);

    //set the comms pattern:
    //"ad" = like the usual AD client (default)
    //"ping" = each cycle just send a (blocking)ping
    //"ad_statsonly" = AD client style but without synchronizing the parameters
    //"ad_paramsonly" = AD client style but without sending the anomaly statistics
    addOptionalCommandLineArgDefaultHelpString(cmdline, benchmark_pattern); 

    addOptionalCommandLineArgDefaultHelpString(cmdline, algorithm); //algorithm, default "sstd"
    addOptionalCommandLineArgDefaultHelpString(cmdline, hbos_bins); //default 20

    if(argc == 1 || (argc == 2 && std::string(argv[1]) == "-help")){
      cmdline.help();
      MPI_Finalize();
      return 0;
    }

    Args args;
    cmdline.parse(args, argc-1, (const char**)(argv+1));

    if(args.hpserver_nthr > 1){
      std::string orig = args.pserver_addr;
      args.pserver_addr = getHPserverIP(args.pserver_addr, args.hpserver_nthr, rank);
      std::cout << "Client rank " << rank << " connecting to endpoint " << args.pserver_addr << " (base " << orig << ")" << std::endl;
    }

    std::string fn_perf = stringize("client_perf.%d.log", rank);
    std::string fn_perf_prd = stringize("client_perf_prd.%d.log", rank);
    PerfStats stats(args.perf_dir, fn_perf);
    PerfPeriodic stats_prd(args.perf_dir, fn_perf_prd);
  
    ADThreadNetClient net_client;
    net_client.connect_ps(rank, 0, args.pserver_addr);

    if(!net_client.use_ps()) fatal_error("Client did not connect to pserver");

    MPI_Barrier(MPI_COMM_WORLD); //have all clients sync up before proceeding
        
    net_client.linkPerf(&stats);  

    //We need to correctly treat the function indices and use the global index appropriately
    ADglobalFunctionIndexMap lidx_gidx_map(0, &net_client);
    std::unordered_map<unsigned long,std::string> lidx_fname_map;

    {
      std::vector<unsigned long> lidx(args.nfuncs);
      std::vector<std::string> fnames(args.nfuncs);
      
      for(int i=0;i<args.nfuncs;i++){
	std::string fname = "func"+anyToStr(i);
	fnames.push_back(fname);
	lidx.push_back(i);
	lidx_fname_map[i] = fname;
      }
      lidx_gidx_map.lookup(lidx,fnames); //get the global indices from the pserver

      if(rank == 0){
	std::cout << "Rank 0 lfid->gfid mapping:" <<std::endl;
	for(unsigned long i=0;i<args.nfuncs;i++)
	  std::cout << i << " " << lidx_gidx_map.lookup(i) << std::endl;
      }
    }

    //Set up a params object with the required number of params
    ParamInterface *params;
    if(args.algorithm == "sstd"){
      SstdParam *p = new SstdParam;
      for(int i=0;i<args.nfuncs;i++){
	RunStats &r = (*p)[lidx_gidx_map.lookup(i)];
	for(int j=0;j<100;j++)
	  r.push(double(j));
      }
      params = p;
    }else if(args.algorithm == "hbos" || args.algorithm == "copod"){
      Histogram::Data d;
      d.counts.resize(args.hbos_bins);
      d.bin_edges.resize(args.hbos_bins+1);
      for(int i=0;i<args.hbos_bins;i++) d.counts[i] = i;
      for(int i=0;i<args.hbos_bins+1;i++) d.bin_edges[i] = i;
      Histogram hd;
      hd.set_hist_data(d);
      hd.set_min_max(0.0001, args.hbos_bins-0.0001);

      if(args.algorithm == "hbos"){
	HbosParam *p = new HbosParam;
	for(int i=0;i<args.nfuncs;i++) (*p)[lidx_gidx_map.lookup(i)].getHistogram() = hd;
	params = p;
      }else{
	CopodParam *p = new CopodParam;
	for(int i=0;i<args.nfuncs;i++) (*p)[lidx_gidx_map.lookup(i)].getHistogram() = hd;
	params = p;
      }
    }else{
      fatal_error("Unknown AD algorithm");
    }


    int nevent = 100;
    if(nevent < args.nanomalies_per_func) nevent = args.nanomalies_per_func;
  
    //Create the fake function statistics and anomalies
    //Note the actual values here are not expected to influence the data packet size, only the number of functions and counters
    std::cout << "Rank " << rank << " generating fake function stats and anomalies" << std::endl;
    CallList_t fake_execs;
    ExecDataMap_t fake_exec_map;
    Anomalies anomalies;

    for(int i=0;i<args.nfuncs;i++){
      unsigned long gfid = lidx_gidx_map.lookup(i);

      for(int j=0;j<nevent;j++){
	ExecData_t e = createFuncExecData_t(0, rank, 0,
					    gfid, lidx_fname_map[i],
					    100*i, 100);
	auto it = fake_execs.insert(fake_execs.end(),e);
	fake_exec_map[gfid].push_back(it);

	if(j<args.nanomalies_per_func){
	  it->set_label(-1);
	  anomalies.recordAnomaly(it);
	}else{
	  it->set_label(1);
	}
      }
    }

    //Create the fake counters
    std::cout << "Rank " << rank << " generating fake counters" << std::endl;
    std::unordered_map<std::string, RunStats> counter_stats;
    for(int i=0;i<args.ncounters;i++){
      RunStats stats;
      for(int j=0;j<100;j++)
	stats.push(double(j));
      counter_stats["counter"+anyToStr(i)] = stats;
    }

    PerfTimer cyc_timer;
    PerfTimer timer;

    //To make the benchmark as lightweight as possible, precompute the messages and send the same each cycle
    std::string params_msg = params->serialize();

    int pid = 0;
    int step = 0;
  
    ADLocalFuncStatistics prof_stats(pid,rank,step);
    prof_stats.gatherStatistics(&fake_exec_map);
    prof_stats.gatherAnomalies(anomalies);

    ADLocalCounterStatistics count_stats(pid,step, nullptr); //currently collect all counters
    for(auto const &e : counter_stats)
      count_stats.setStats(e.first,e.second);

    unsigned long first_ts = 0;
    unsigned long last_ts = 1234;
    ADLocalAnomalyMetrics metrics(pid, rank, step, first_ts, last_ts, anomalies);

    ADcombinedPSdata comb_stats(prof_stats, count_stats, metrics);

    std::string comb_stats_msg = comb_stats.net_serialize(); 

    int n_steps_accum_prd = 0;
    int async_send_calls = 0;

    //Decide which activities we are performing
    bool do_param_sync = args.benchmark_pattern == "ad" || args.benchmark_pattern == "ad_paramsonly";
    bool do_stats = args.benchmark_pattern == "ad" || args.benchmark_pattern == "ad_statsonly";
    bool do_ping = args.benchmark_pattern == "ping";

    
    for(int c=0;c<args.cycles;c++){
      cyc_timer.start();
      std::cout << "Rank " << rank << " starting cycle " << c << std::endl;
      //Send a parameters object
      if(do_param_sync){
	timer.start();
	Message msg;
	msg.set_info(rank, 0, MessageType::REQ_ADD, MessageKind::PARAMETERS);
	msg.set_msg(params_msg, false);
	stats.add("param_update_msg_format_ms", timer.elapsed_ms());
	stats.add("param_update_msg_size_bytes", params_msg.size());
	
	timer.start();
	net_client.send_and_receive(msg);
	stats.add("param_update_comms_ms", timer.elapsed_ms());
      }

      //Send combined statistics information
      if(do_stats){
	timer.start();
	Message msg;
	msg.set_info(net_client.get_client_rank(), net_client.get_server_rank(), MessageType::REQ_ADD, MessageKind::AD_PS_COMBINED_STATS, c);
	msg.set_msg(comb_stats_msg);
	stats.add("comb_stats_msg_format_ms", timer.elapsed_ms());
	stats.add("comb_stats_msg_size_bytes", comb_stats_msg.size());
	
	timer.start();
	net_client.async_send(msg);
	stats.add("comb_stats_update_async_comms_ms", timer.elapsed_ms());
      }

      //Send ping
      if(do_ping){
	Message msg;
	msg.set_info(rank, 0, MessageType::REQ_ECHO, MessageKind::CMD);
	msg.set_msg("ping", false);
	
	timer.start();
	net_client.send_and_receive(msg);
	stats.add("ping_pong_comms_ms", timer.elapsed_ms());
      }

      //Sleep to catch up with 1 second cycle cadence as in real client
      double comms_time = cyc_timer.elapsed_ms();
      if(comms_time < args.cycle_time_ms){
	int sleep_time = args.cycle_time_ms - (int)comms_time;
	std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
	stats.add("sleep_ms", sleep_time);
      }else{
	std::cout << "Rank " << rank << " comms time " << comms_time << "ms longer than cycle time " << args.cycle_time_ms << "ms: server is overloaded!" << std::endl;
	stats.add("sleep_ms", 0);
      }
	
      // timer.start();
      // std::this_thread::sleep_for(std::chrono::milliseconds(args.cycle_time_ms));
      // stats.add("sleep_ms", timer.elapsed_ms());

      stats.add("benchmark_cycle_time_ms", cyc_timer.elapsed_ms());

      n_steps_accum_prd++;
      async_send_calls++;
      
      if(c>0 && c % args.perf_write_freq == 0){
	int noutstanding = net_client.getNwork();
	stats_prd.add("ps_incomplete_async_sends", noutstanding);
	stats_prd.add("io_steps", n_steps_accum_prd);
	stats_prd.add("ps_total_async_send_calls", async_send_calls);

	stats_prd.write();
	stats.write();

	n_steps_accum_prd = 0;
      }
    
    }//cycle loop

    if(net_client.getNwork() > 0){
      //Continue to write out #outstanding until queue is drained at same freq
      int sleep_time_ms = args.perf_write_freq * args.cycle_time_ms;
      while(1){
	std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time_ms));
	int noutstanding = net_client.getNwork();
	stats_prd.add("ps_incomplete_async_sends", noutstanding);
	stats_prd.add("io_steps", 0);
	stats_prd.add("ps_total_async_send_calls", async_send_calls); //running total stays fixed
      
	stats_prd.write();
      
	if(noutstanding == 0)
	  break;
      }
    }
    
    stats.write();
    delete params;
  }
  
  MPI_Finalize();
  return 0;
}
