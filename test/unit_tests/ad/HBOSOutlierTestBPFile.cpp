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

  headProgressStream(params.rank) << "Driver rank " << params.rank
      << ": analysis start " << (driver.use_ps() ? "with": "without")
      << " pserver" << std::endl;
  std::cout << params.rank << "Driver rank " << params.rank
      << ": analysis start " << (driver.use_ps() ? "with": "without")
      << " pserver" << std::endl;

  ADParser *parser = new ADParser(params.trace_data_dir + "/" + params.trace_inputFile, params.program_idx, params.rank, params.trace_engineType,
			  params.trace_connect_timeout);

  parser->setBeginStepTimeout(params.parser_beginstep_timeout);
  parser->setDataRankOverride(false); //params.override_rank);

  ADEvent event = new ADEvent(params.verbose);
  event->linkFuncMap(parser->getFuncMap());
  event->linkEventType(parser->getEventType());
  event->linkCounterMap(parser->getCounterMap());

  ADOutlierHBOSTest outlier = new ADOutlierHBOSTest();
  outlier->linkExecDataMap(event->getExecDataMap());

  ADCounter counter = new ADCounter();
  counter->linkCounterMap(parser->getCounterMap());

  //run now
  int step = parser->getCurrentStep();

  ASSERT_EQ(step, -1);
  

}
