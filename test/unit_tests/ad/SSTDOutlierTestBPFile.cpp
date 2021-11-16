#include "chimbuko/chimbuko.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/util/string.hpp"
#include "chimbuko/util/commandLineParser.hpp"
#include<chimbuko/ad/ADOutlier.hpp>
#include<chimbuko/param/sstd_param.hpp>
#include<chimbuko/message.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"
#include "../unit_test_cmdline.hpp"

#include<thread>
#include<chrono>
#include <cstdlib>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <random>
#include <set>
#include <ctime>

using namespace chimbuko;

//Derived class to allow access to protected member functions
class ADOutlierSSTDTest: public ADOutlierSSTD{
public:
  ADOutlierSSTDTest(ADOutlier::OutlierStatistic stat = ADOutlier::ExclusiveRuntime): ADOutlierSSTD(stat){}

  std::pair<size_t, size_t> sync_param_test(ParamInterface* param){ return this->ADOutlierSSTD::sync_param(param); }

  unsigned long compute_outliers_test(Anomalies &anomalies,
				      const unsigned long func_id, std::vector<CallListIterator_t>& data){
    return this->compute_outliers(anomalies,func_id, data);
  }

  double getStatisticValueTest(const ExecData_t &e) const { return this->getStatisticValue(e);}

  ParamInterface const* get_global_parametersTest() const{ return this->get_global_parameters();}

};

bool parseInputStepTest(int &step, ADParser **m_parser, const ChimbukoParams &params, unsigned long long& n_func_events,unsigned long long& n_comm_events,unsigned long long& n_counter_events) {

  if (!(*m_parser)->getStatus()) return false;

  int expect_step = step+1;

  (*m_parser)->beginStep();
  if (!(*m_parser)->getStatus()){
    verboseStream << "driver parser appears to have disconnected, ending" << std::endl;
    return false;
  }

  step = (*m_parser)->getCurrentStep();
  if(step != expect_step){ verboseStream << "Got step " << step << " expected " << expect_step << std::endl; }

  verboseStream << "driver rank " << params.rank << " updating attributes" << std::endl;
  (*m_parser)->update_attributes();
  verboseStream << "driver rank " << params.rank << " fetching func data" << std::endl;
  (*m_parser)->fetchFuncData();
  verboseStream << "driver rank " << params.rank << " fetching comm data" << std::endl;
  (*m_parser)->fetchCommData();
  verboseStream << "driver rank " << params.rank << " fetching counter data" << std::endl;
  (*m_parser)->fetchCounterData();
  verboseStream << "driver rank " << params.rank << " finished gathering data" << std::endl;

  (*m_parser)->endStep();

  // count total number of events
  n_func_events += (unsigned long long)(*m_parser)->getNumFuncData();
  n_comm_events += (unsigned long long)(*m_parser)->getNumCommData();
  n_counter_events += (unsigned long long)(*m_parser)->getNumCounterData();

  verboseStream << "driver completed input parse for step " << step << std::endl;
  return true;

}

void create_save_json(const std::unordered_map<unsigned long, std::vector<double> > &data, const int &rank, const std::string & prefix) {
  nlohmann::json j(data);
  std::string fname = prefix + std::to_string(rank) + ".json";
  std::ofstream ofile(fname);
  ofile << j;

}

void create_save_json(const std::unordered_map<unsigned long, std::vector<double> > &data, const int &rank, const std::string & prefix, const int &io_step) {
  nlohmann::json j(data);
  std::string fname = prefix + "-" + std::to_string(rank) + "-step-" + std::to_string(io_step) + ".json";
  std::ofstream ofile(fname);
  ofile << j;

}

TEST(SSTDADOutlierBPFileWithoutPServer, Works) {
  //Get trace data dir from command line
  if(_argc < 2){
    throw std::runtime_error("Path to trace data directory must be provided as an argument!");
  }
  std::string trace_data_dir = _argv[1];

  //int file_suffix = 1;
  int ranks = 4;
  std::vector<int> v_io_steps(ranks);
  std::vector<int> v_functions(ranks);
  std::vector<unsigned long> v_outliers(ranks), v_tot_events(ranks);

  std::vector<std::vector<double> > v_tot_time(ranks);
  std::vector<double> v_ad_compute_time;
  std::clock_t ts, ti, tad;

  ts = std::clock();
  for(int mpi_rank_bp = 0; mpi_rank_bp < ranks; mpi_rank_bp++) { // used for BPFile
    ti = std::clock();
    ChimbukoParams params;
    //Parameters for the connection to the instrumented binary trace output
    params.trace_engineType = "BPFile"; // argv[1]; // BPFile or SST
    params.trace_data_dir = trace_data_dir; // *.bp location
    std::string bp_prefix = "tau-metrics"; //argv[3]; // bp file prefix (e.g. tau-metrics-[nwchem])

    //The remainder are optional arguments. Enable using the appropriate command line switch
    params.program_idx = 0;
    params.pserver_addr = "";  //don't use pserver by default
    params.hpserver_nthr = 1;
    params.outlier_sigma = 6.0;     // anomaly detection algorithm parameter
    params.anom_win_size = 10; // size of window of events captured around anomaly
    params.perf_outputpath = ""; //don't use perf output by default
    params.perf_step = 10;   // make output every 10 steps
    params.prov_outputpath = "";
  #ifdef ENABLE_PROVDB
    params.nprovdb_shards = 1;
    params.provdb_addr_dir = ""; //don't use provDB by default
  #endif
    params.err_outputpath = ""; //use std::cerr for errors by default
    params.trace_connect_timeout = 60;
    params.parser_beginstep_timeout = 30;
    params.rank = -1234; //assign an invalid value as default for use below
    params.outlier_statistic = "exclusive_runtime";
    params.step_report_freq = 1;

    //getOptionalArgsParser().parse(params, argc-4, (const char**)(argv+4));

    //By default assign the rank index of the trace data as the MPI rank of the AD process
    //Allow override by user
    if(params.rank < 0)
      params.rank = mpi_rank_bp; //mpi_world_rank;

    params.verbose = params.rank == 0; //head node produces verbose output

    //Assume the rank index of the data is the same as the driver rank parameter
    params.trace_inputFile = bp_prefix + "-" + std::to_string(mpi_rank_bp) + ".bp"; //std::to_string(params.rank) + ".bp";

    //If we are forcing the parsed data rank to match the driver rank parameter, this implies it was not originally
    //Thus we need to obtain the input data rank also from the command line and modify the filename accordingly
    //if(params.override_rank)
    //  params.trace_inputFile = bp_prefix + "-" + std::to_string(overrideRankArg::input_data_rank()) + ".bp";

    //If neither the provenance database or the provenance output path are set, default to outputting to pwd
    if(params.prov_outputpath.size() == 0
  #ifdef ENABLE_PROVDB
       && params.provdb_addr_dir.size() == 0
  #endif
       ){
      params.prov_outputpath = "./bpfile_test_results";
    }
    std::cout << "ChimbukoParams configuration:" << std::endl;
    params.print();

    //Initialize
    ADParser *parser = new ADParser(params.trace_data_dir + "/" + params.trace_inputFile, params.program_idx, params.rank, params.trace_engineType,
  			  params.trace_connect_timeout);

    parser->setBeginStepTimeout(params.parser_beginstep_timeout);
    parser->setDataRankOverride(false); //params.override_rank);

    ADEvent *event = new ADEvent(params.verbose);
    event->linkFuncMap(parser->getFuncMap());
    event->linkEventType(parser->getEventType());
    event->linkCounterMap(parser->getCounterMap());

    //ADOutlierHBOSTest *outlier = new ADOutlierHBOSTest();
    ADOutlierSSTDTest *outlier = new ADOutlierSSTDTest();
    outlier->linkExecDataMap(event->getExecDataMap());

    ADCounter *counter = new ADCounter();
    counter->linkCounterMap(parser->getCounterMap());

    //run now
    int step = parser->getCurrentStep();
    unsigned long long n_func_events = 0, n_comm_events = 0, n_counter_events = 0;
    unsigned long n_outliers = 0, n_tot_events = 0; //n_executions = 0,
    std::set<unsigned long> n_functions;


    unsigned long first_event_ts, last_event_ts;

    int io_steps = 0;
    std::unordered_map<unsigned long, std::vector<double> > outs_map;

    while(parseInputStepTest(step, &parser, params, n_func_events, n_comm_events, n_counter_events)) {
      std::cout << ++io_steps << std::endl;

      //extract counters
      for(size_t c=0;c < parser->getNumCounterData();c++){
        Event_t ev(parser->getCounterData(c),
    	       EventDataType::COUNT,
    	       c,
    	       eventID(params.rank, step, c));
        counter->addCounter(ev);
      }

      //extract events
      std::vector<Event_t> events = parser->getEvents();
      for(auto &e : events)
        event->addEvent(e);
      if(events.size()){
        first_event_ts = events.front().ts();
        last_event_ts = events.back().ts();
      }else{
        first_event_ts = last_event_ts = -1; //no events!
      }

      //outlier detection run
      Anomalies anomalies;
      ADOutlierSSTDTest testSstd;
      SstdParam local_params_ad;
      SstdParam &global_params_ad = *(SstdParam *)testSstd.get_global_parametersTest();

      const ExecDataMap_t* m_execDataMap = event->getExecDataMap();
      verboseStream << "Starting OUtlier Detection" << std::endl;
      if (m_execDataMap == nullptr){ verboseStream << "Empty ExecDataMap_t" << std::endl; }

      tad = std::clock();
      for (auto it : *m_execDataMap) { //loop over functions (key is function index)
        verboseStream << "Looping over m_execDataMap" << std::endl;
        unsigned long func_id = it.first;
        n_functions.insert(func_id);
        //std::vector<double> runtimes;
        for (auto itt : it.second) { //loop over events for that function
          if(itt->get_label() == 0){
            local_params_ad[func_id].push(testSstd.getStatisticValueTest(*itt)); //runtimes.push_back(testSstd.getStatisticValueTest(*itt));
            ++n_tot_events;
          }
        }
        // if (!global_params_ad.find(func_id)) { // If func_id does not exist
        //   local_params_ad[func_id].create_histogram(runtimes);
        // }
        // else { //merge with exisiting func_id, not overwrite
        //   //param[func_id] += g[func_id];
        //   local_params_ad[func_id].merge_histograms(global_params_ad[func_id], runtimes);
        // }

        //n_tot_events += std::accumulate(local_params_ad[func_id].counts().begin(), local_params_ad[func_id].counts().end(), 0);
      }

      std::pair<size_t, size_t> msgsz = testSstd.sync_param_test(&local_params_ad);

      //Run anomaly detection algorithm
      //tad = std::clock();
      for (auto it : *m_execDataMap) { //loop over function index
        const unsigned long func_id = it.first;
        //tad = std::clock();
        const unsigned long n = testSstd.compute_outliers_test(anomalies,func_id, it.second);
        n_outliers += n;
        //double tad_taken = (std::clock() - tad) / (double) CLOCKS_PER_SEC;
        //v_ad_compute_time.push_back(tad_taken);
        //++n_executions;
        std::vector<double> r_times;
        for(auto itt : anomalies.funcEvents(func_id, Anomalies::EventType::Outlier)){
          const double r_time = testSstd.getStatisticValueTest(*itt);
          r_times.push_back(r_time);
        }
        if(outs_map.find(func_id) == outs_map.end()){
          outs_map[func_id] = r_times;
        }
        else{
          for(int i=0; i<r_times.size(); i++) {
            outs_map[func_id].push_back(r_times.at(i));
          }
        }
        r_times.clear();
      }
      double tad_taken = (std::clock() - tad) / (double) CLOCKS_PER_SEC;
      v_ad_compute_time.push_back(tad_taken);

      create_save_json(anomalies.allSstdScores(), mpi_rank_bp, "sstd_funcStats", io_steps);
    }

    create_save_json(outs_map, mpi_rank_bp, "SSTD_outs");

    v_io_steps[mpi_rank_bp] = io_steps;
    v_tot_events[mpi_rank_bp] = n_tot_events;
    v_functions[mpi_rank_bp] = n_functions.size();
    v_outliers[mpi_rank_bp] = n_outliers;

    //Average time to compute AD using HBOS
    double avg_time = std::accumulate(v_ad_compute_time.begin(), v_ad_compute_time.end(), 0.0) / v_ad_compute_time.size();
    v_tot_time[mpi_rank_bp].push_back(avg_time);
    v_ad_compute_time.clear();
    double rank_time = (std::clock() - ti) / (double) CLOCKS_PER_SEC;
    v_tot_time[mpi_rank_bp].push_back(rank_time);

    std::cout << "\n\nTest Summary for rank " << params.rank <<  " in file " << params.trace_inputFile << std::endl;
    std::cout << "Number of IO steps: " << io_steps << std::endl;
    std::cout << "Number of Functions: " << n_functions.size() << std::endl;
    std::cout << "Number of Events: " << n_tot_events << std::endl;
    //std::cout << "Number of Executions: " << n_executions << std::endl;
    std::cout << "Number of Anomalies: " << n_outliers << std::endl;

    parser->~ADParser();
    event->~ADEvent();
    counter->~ADCounter();
  }

  std::cout << "\n\n--------------------------------------------\nTest Summary for all BPFile runs\n--------------------------------------------" << std::endl;
  for(int i=0;i<ranks;i++) {
    std::cout<< "Summary for rank " << i << " using BPFile " << "tau-metrics-" << i << ".bp" << std::endl;
    std::cout << "Number of IO steps: " << v_io_steps.at(i) << std::endl;
    std::cout << "Number of Functions: " << v_functions.at(i) << std::endl;
    std::cout << "Number of Events: " << v_tot_events.at(i) << std::endl;
    std::cout << "Number of Anomalies: " << v_outliers.at(i) << std::endl;
    std::cout << "Average Time taken by SSTD: " << v_tot_time[i][0] << " seconds" << std::endl;
    std::cout << "Total Time: " << v_tot_time[i][1] << " seconds" << std::endl;
    std::cout << "--------------------------------------------" << std::endl;
  }
  double final_time = (std::clock() - ts) / (double) CLOCKS_PER_SEC;
  std::cout << "Total Time for all ranks: " << final_time << " seconds" << std::endl;
  //std::cout << "Final i: " << i << std::endl;
} //End Test
