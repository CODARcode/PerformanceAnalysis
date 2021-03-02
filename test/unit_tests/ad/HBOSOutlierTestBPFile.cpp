#include "chimbuko/chimbuko.hpp"
#include "chimbuko/verbose.hpp"
#include "chimbuko/util/string.hpp"
#include "chimbuko/util/commandLineParser.hpp"
#include<chimbuko/ad/ADOutlier.hpp>
#include<chimbuko/param/sstd_param.hpp>
#include<chimbuko/message.hpp>
#include "gtest/gtest.h"
#include "../unit_test_common.hpp"

#include<thread>
#include<chrono>
#include <cstdlib>
#include <condition_variable>
#include <mutex>
#include <cstring>
#include <random>

using namespace chimbuko;

class ADOutlierHBOSTest: public ADOutlierHBOS{
public:
  ADOutlierHBOSTest(ADOutlier::OutlierStatistic stat = ADOutlier::ExclusiveRuntime): ADOutlierHBOS(stat){}

  std::pair<size_t, size_t> sync_param_test(ParamInterface* param){ return this->ADOutlierHBOS::sync_param(param); }

  unsigned long compute_outliers_test(Anomalies &anomalies,
				      const unsigned long func_id, std::vector<CallListIterator_t>& data){
    return this->compute_outliers(anomalies,func_id, data);
  }
};

bool parseInputStepTest(int &step, ADParser **m_parser, unsigned long long& n_func_events,unsigned long long& n_comm_events,unsigned long long& n_counter_events) {

  if (!m_parser->getStatus()) return false;

  int expect_step = step+1;

  m_parser->beginStep();
  if (!m_parser->getStatus()){
    verboseStream << "driver parser appears to have disconnected, ending" << std::endl;
    return false;
  }

  step = m_parser->getCurrentStep();
  if(step != expect_step){ recoverable_error(stringize("Got step %d expected %d\n", step, expect_step)); }

  verboseStream << "driver rank " << m_params.rank << " updating attributes" << std::endl;
  m_parser->update_attributes();
  verboseStream << "driver rank " << m_params.rank << " fetching func data" << std::endl;
  m_parser->fetchFuncData();
  verboseStream << "driver rank " << m_params.rank << " fetching comm data" << std::endl;
  m_parser->fetchCommData();
  verboseStream << "driver rank " << m_params.rank << " fetching counter data" << std::endl;
  m_parser->fetchCounterData();
  verboseStream << "driver rank " << m_params.rank << " finished gathering data" << std::endl;

  m_parser->endStep();

  // count total number of events
  n_func_events += (unsigned long long)m_parser->getNumFuncData();
  n_comm_events += (unsigned long long)m_parser->getNumCommData();
  n_counter_events += (unsigned long long)m_parser->getNumCounterData();

  verboseStream << "driver completed input parse for step " << step << std::endl;
  return true;

}

TEST(HBOSADOutlierBPFileWithoutPServer, Works) {

  ChimbukoParams params;
  int mpi_rank_bp = 0; // used for BPFile
  //Parameters for the connection to the instrumented binary trace output
  params.trace_engineType = "BPFile"; // argv[1]; // BPFile or SST
  params.trace_data_dir = "../../data"; // argv[2]; // *.bp location
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
  params.provdb_addr = ""; //don't use provDB by default
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
  params.trace_inputFile = bp_prefix + "-" + std::to_string(params.rank) + ".bp";

  //If we are forcing the parsed data rank to match the driver rank parameter, this implies it was not originally
  //Thus we need to obtain the input data rank also from the command line and modify the filename accordingly
  //if(params.override_rank)
  //  params.trace_inputFile = bp_prefix + "-" + std::to_string(overrideRankArg::input_data_rank()) + ".bp";

  //If neither the provenance database or the provenance output path are set, default to outputting to pwd
  if(params.prov_outputpath.size() == 0
#ifdef ENABLE_PROVDB
     && params.provdb_addr.size() == 0
#endif
     ){
    params.prov_outputpath = "./bpfile_test_results";
  }
  std::cout << "ChimbukoParams configuration:" << std::endl;
  params.print();

  //Initialize
  //Chimbuko driver(params);

  // headProgressStream(params.rank) << "Driver rank " << params.rank
  //     << ": analysis start " << (driver.use_ps() ? "with": "without")
  //     << " pserver" << std::endl;
  // std::cout << params.rank << "Driver rank " << params.rank
  //     << ": analysis start " << (driver.use_ps() ? "with": "without")
  //     << " pserver" << std::endl;

  ADParser *parser = new ADParser(params.trace_data_dir + "/" + params.trace_inputFile, params.program_idx, params.rank, params.trace_engineType,
			  params.trace_connect_timeout);

  parser->setBeginStepTimeout(params.parser_beginstep_timeout);
  parser->setDataRankOverride(false); //params.override_rank);

  ADEvent *event = new ADEvent(params.verbose);
  event->linkFuncMap(parser->getFuncMap());
  event->linkEventType(parser->getEventType());
  event->linkCounterMap(parser->getCounterMap());

  ADOutlierHBOSTest *outlier = new ADOutlierHBOSTest();
  outlier->linkExecDataMap(event->getExecDataMap());

  ADCounter *counter = new ADCounter();
  counter->linkCounterMap(parser->getCounterMap());

  //run now
  int step = parser->getCurrentStep();
  unsigned long long n_func_events = 0, n_comm_events = 0, n_counter_events = 0;

  ASSERT_EQ(step, -1);

  unsigned long first_event_ts, last_event_ts;

  int i = 0;
  while(parseInputStepTest(step, &parser, n_func_events, n_comm_events, n_counter_events)) {
    std::cout << ++i << std::endl;

    //extract counters
    for(size_t c=0;c < parser->getNumCounterData();c++){
      Event_t ev(parser->getCounterData(c),
  	       EventDataType::COUNT,
  	       c,
  	       generate_event_id(params.rank, step, c));
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

  }

  std::cout << "Final i: " << i << std::endl;
}
